#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP_EEPROM.h>
#include <MFRC522.h>
#include <SPI.h>
#include <WiFiManager.h>

#include "HX711.h"
#include "MFRC522.h"
#include "UUID.h"

#define DEBUG_RESET 0
#define DEBUG

#define MAX_DATA 889  // The most data bytes any tag will hold

struct SpoolJson {
  const char* protocol;
  float version;
  const char* type;
  const char* color_hex;
  const char* brand;
  uint16_t min_temp;
  uint16_t max_temp;
  float k_factor;
  const char* uuid;
  uint16_t spoolman_id;
  int8_t status;
};

union ArrayToInteger {
  byte array[2];
  uint16_t integer;
};

void lightsOff();
void pulsingWait(uint8_t color);
SpoolJson retrieveSpool(uint16_t spoolman_id);
void setupScale();
SpoolJson readRfidJson();
int readRfid(byte startingByte, uint16_t length, byte* outputBuffer);
uint16_t byteToBlock(uint16_t byteNumber);
uint16_t byteOffset(uint16_t byteNumber);
int16_t getWeight();
uint8_t updateSpool(uint16_t spoolman_id, int16_t weight);
void blinkNumTimes(uint8_t color, uint8_t times);
uint16_t findSpoolByUuid(const char* uuid);

// Spoolman server address
#define SERVERNAME "http://10.0.1.50:8000/api/v1/"

// RGB LED pins
#define LED_RED D4
#define LED_BLU D3
#define LED_GRN D0

// RGB LED extra colors
#define LED_WHT 10
#define LED_YEL 11
#define LED_PUR 12
#define LED_CYN 13

// Global variables
uint16_t RGB_BRIGHT = 0;
HX711 scale;
UUID uuid;

// RFID Reader pins
// #define RST_PIN D3  // Configurable, not required
#define SS_PIN D8  // Configurable, take a unused pin, only HIGH/LOW required
// SPI_MOSI 	   D7
// SPI_MISO 	   D6
// SPI_SCK 	     D5

// HX711 pins
#define LOADCELL_SCK_PIN D1
#define LOADCELL_DOUT_PIN D2

MFRC522 mfrc522(SS_PIN);  // Create MFRC522 instance

void setup() {
  Serial.begin(9600);

  EEPROM.begin(8);

  // Setup the RGB LED
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLU, OUTPUT);
  pinMode(LED_GRN, OUTPUT);

  lightsOff();

  // Do an annoying bootup blink
  digitalWrite(LED_GRN, HIGH);
  delay(100);
  digitalWrite(LED_GRN, LOW);
  digitalWrite(LED_RED, HIGH);
  delay(100);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_BLU, HIGH);
  delay(100);
  digitalWrite(LED_BLU, LOW);
  digitalWrite(LED_GRN, HIGH);
  delay(100);
  digitalWrite(LED_GRN, LOW);
  digitalWrite(LED_RED, HIGH);
  delay(100);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_BLU, HIGH);
  delay(100);
  digitalWrite(LED_BLU, LOW);

  // Setup the UUID library
  Serial.println();
  Serial.print(F("Checking UUID Library...\t"));
  Serial.println(UUID_LIB_VERSION);
  uuid.setVariant4Mode();
  // how to gen a new uuid
  uuid.generate();
  Serial.println(uuid);

  // Setup the RFID reader
  Serial.print(F("Starting RFID reader... \t"));
  SPI.begin();                        // Init SPI bus
  mfrc522.PCD_Init();                 // Init MFRC522
  mfrc522.PCD_DumpVersionToSerial();  // Show details of MFRC522

  Serial.print(F("Calibrating the scale...\t"));
  // Setup the load cell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  // Check scale calibration
  setupScale();

  // Setup the WiFi
  Serial.println(F("Connecting to WiFi... "));

  // Solid red light while we connect to wifi
  digitalWrite(LED_RED, HIGH);

  WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP

  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  if (DEBUG_RESET) {
    wm.resetSettings();
  }

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ("OpenTagScale"),
  // then goes into a blocking loop awaiting configuration and will return success result

  // Stop trying after 20 seconds
  wm.setConnectTimeout(20);

  bool res = false;
  res = wm.autoConnect("OpenTagScale", "password");  // password protected ap

  if (!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  } else {
    // if you get here you have connected to the WiFi
    Serial.println("Connected to WiFi");
  }

  // Clear red light after wifi is connected
  lightsOff();
}

void loop() {
  int16_t weight = 0;
  SpoolJson spool;
  int8_t returnCode = 0;

  // New card is present on the reader
  if (mfrc522.PICC_IsNewCardPresent()) {
#ifdef DEBUG
    Serial.println("");
    Serial.println("Found a new card...");
#endif

    // Green LED while reading the RFID
    lightsOff();
    digitalWrite(LED_GRN, HIGH);

    spool = readRfidJson();

    // If the spool status is not 0, the RFID is invalid
    if (spool.status != EXIT_SUCCESS) {
      // Purple light 2 times
      blinkNumTimes(LED_PUR, 2);
      return;
    }

    // wait for the scale to stabilize
    blinkNumTimes(LED_GRN, 1);

    while (!scale.is_ready()) {
      pulsingWait(LED_GRN);
    }
    weight = getWeight();

#ifdef DEBUG
    Serial.print("Weight: ");
    Serial.println(weight);
#endif

    // update the spool with a new weight
    returnCode = updateSpool(spool.spoolman_id, weight);

    if (returnCode == EXIT_SUCCESS) {
      // White light 4 times
      blinkNumTimes(LED_WHT, 4);
    } else {
      // Red light 4 times
      blinkNumTimes(LED_RED, 4);
    }
  }  // end of new card present

  if (scale.is_ready()) {
    weight = getWeight();

#ifdef DEBUG
    for (int i = 0; i < 19; i++) {
      Serial.write(0x08);
    }

    Serial.print("HX711 reading: ");

    // Add padding to the weight numbers for display
    if (weight < 1000) {
      Serial.print(" ");
      if (weight < 100) {
        Serial.print(" ");
        if (weight < 10) {
          Serial.print(" ");
        }
      }
    }

    Serial.print(weight);

#endif
  }

  // Pulse blue while waiting to read
  pulsingWait(LED_BLU);
}

// Get a weight reading from the scale and return it
int16_t getWeight() {
  int16_t reading = 0;

  reading = scale.get_units(5);  // BUG: this is returning a float, i should handle it as such
  if (reading < 0) {
    reading = 0;
  }

  return reading;
}

// Turn off all of the lights
void lightsOff() {
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GRN, LOW);
  digitalWrite(LED_BLU, LOW);

  return;
}

// Pulse a light
void pulsingWait(uint8_t color) {
  int brightness;

  if (RGB_BRIGHT > 509) {
    RGB_BRIGHT = 0;
  }

  if (RGB_BRIGHT > 255) {
    brightness = RGB_BRIGHT - 255;
    brightness = 255 - brightness;
  } else {
    brightness = RGB_BRIGHT;
  }

  analogWrite(color, brightness);

  RGB_BRIGHT++;

  return;
}

// Scale calibration is done here
// TODO: Need to make it callable as needed
void setupScale() {
  float average = 0;
  double calFactor = 0.0;

  scale.set_scale();
  scale.tare();
  yield();

  EEPROM.get(0, calFactor);

  // Serial.print("stored calFactor: \t");
  Serial.println(calFactor);

  // I have no idea what a valid range is. this works for me
  if (calFactor > 1000.0 || calFactor < 100.0 || isnan(calFactor)) {
    Serial.println("\tcalFactor seems invalid.");
  } else {
    Serial.println("\tcalFactor seems valid!");
    if (!DEBUG_RESET) {
      scale.set_scale(calFactor);
      return;
    } else {
      Serial.println("Resetting scale calibration...");
    }
  }

  // Reading 20 at once had a chance of tripping the watchdog on esp8266
  Serial.print("read average: \t\t");
  average = scale.read_average(10);
  yield();
  average += scale.read_average(10);
  yield();
  average /= 2;

  Serial.println(average);  // print the average of 20 readings from the ADC

  Serial.println("Add 1082g weight...");
  for (uint8_t i = 10; i > 0; i--) {
    Serial.print(i);
    Serial.print("... ");
    delay(1000);
  }
  Serial.println("");

  average = scale.get_units(10);
  yield();
  calFactor = average / 1082;

  Serial.print("Scale factor: \t\t");
  Serial.println(calFactor);
  scale.set_scale(calFactor);

  EEPROM.put(0, calFactor);
  bool ok = EEPROM.commit();
  Serial.println((ok) ? "Saved calFactor" : "Saving calFactor failed");

  Serial.println("Remove weight...");
}

// Read the json from an rfid tag and interpret it
SpoolJson readRfidJson() {
  ArrayToInteger ndefSize;
  uint8_t dataBlocks[4];
  uint16_t byteIndex = 16;
  int8_t status = 0;
  uint8_t jsonBuffer[MAX_DATA] = "\0";  // A buffer to hold the largest possible NTAG216 written data
  JsonDocument doc;
  SpoolJson mySpool;

  // Initial size of the NDEF. Read until this is set.
  ndefSize.integer = 0;

  while (ndefSize.integer == 0) {
    // read from byte 16, 4 bytes, put them in dataBlocks[]
    // This should be the first TLV
    status = readRfid(byteIndex, 4, dataBlocks);

    if (status != 0) {
      Serial.print("readRFID returned error! ");
      Serial.println(status);
      mySpool.status = status;
      return mySpool;
    }

    // find the TLV and hope it was an NDEF
    if (dataBlocks[0] == 0x03) {  // NDEF TLV
      if (dataBlocks[1] == 0xFF) {
        ndefSize.array[0] = dataBlocks[3];
        ndefSize.array[1] = dataBlocks[2];
        byteIndex += 4;
      } else {
        ndefSize.integer = dataBlocks[1];
        byteIndex += 2;
      }
#ifdef DEBUG
      Serial.print("We have an NDEF TLV!\nThe size is: ");
      Serial.println(ndefSize.integer);
#endif
      byteIndex += 3;  // Skip the NDEF header data
      ndefSize.integer -= 3;
    } else if (dataBlocks[0] == 0x00) {
      Serial.println("Null TLV");
      byteIndex++;
      ndefSize.integer = 0;  // This will force another read
    } else {
      // TODO: Finish handling of other TLVs just in case
      Serial.print("Different TLV: ");
      Serial.println(dataBlocks[0], HEX);

      mySpool.status = -1;
      return mySpool;
    }
  }

  // read the entire NDEF data block
  status = readRfid(byteIndex, ndefSize.integer, jsonBuffer);

  if (status != 0) {
    Serial.print("readRFID returned error! ");
    Serial.println(status);
    mySpool.status = status;
    return mySpool;
  }

  // Done reading, halt the PICC
  mfrc522.PICC_HaltA();

  // Remove the leading "application/json"
  for (uint16_t i = 0; i < sizeof(jsonBuffer) - 16; i++) {
    jsonBuffer[i] = jsonBuffer[i + 16];
  }

  // unpack the JSON
  DeserializationError error = deserializeJson(doc, jsonBuffer);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());

    mySpool.status = -2;
    return mySpool;
  }

  mySpool.protocol = doc["protocol"];
  mySpool.version = doc["version"];
  mySpool.type = doc["type"];
  mySpool.color_hex = doc["color_hex"];
  mySpool.brand = doc["brand"];
  mySpool.min_temp = doc["min_temp"];
  mySpool.max_temp = doc["max_temp"];
  mySpool.k_factor = doc["k_factor"];
  mySpool.uuid = doc["UUID"];
  mySpool.spoolman_id = doc["spoolman_id"];

#ifdef DEBUG  // Print the values
  Serial.println("All of the JSON Keys are:");
  Serial.print("protocol : ");
  Serial.println(mySpool.protocol);
  Serial.print("version : ");
  Serial.println(mySpool.version, 1);
  Serial.print("type : ");
  Serial.println(mySpool.type);
  Serial.print("color_hex : ");
  Serial.println(mySpool.color_hex);
  Serial.print("brand : ");
  Serial.println(mySpool.brand);
  Serial.print("min_temp : ");
  Serial.println(mySpool.min_temp);
  Serial.print("max_temp : ");
  Serial.println(mySpool.max_temp);
  Serial.print("UUID : ");
  Serial.println(mySpool.uuid);
  Serial.print("spoolman_id : ");
  Serial.println(mySpool.spoolman_id);
#endif

  if (mySpool.spoolman_id != 0) {
    mySpool.status = 0;
  } else if (mySpool.uuid != 0) {  // No spoolman_id found, so we need to find the spool
    mySpool.spoolman_id = findSpoolByUuid(mySpool.uuid);

    if (mySpool.spoolman_id != 0) {
      mySpool.status = EXIT_SUCCESS;
    } else {
      mySpool.status = EXIT_FAILURE;
    }

  } else {
    mySpool.status = EXIT_FAILURE;

#ifdef DEBUG
    Serial.println("No spoolman_id found on the RFID.");
#endif
  }

  return mySpool;
}

// make API call to spoolman to retrieve data for a spool
SpoolJson retrieveSpool(uint16_t spoolman_id) {
  WiFiClient client;
  HTTPClient http;
  JsonDocument doc;
  char spoolman_id_char[5] = "\0";
  char url[99] = "\0";
  SpoolJson mySpool;

  itoa(spoolman_id, spoolman_id_char, 10);

  // https://donkie.github.io/spool/{spool_id}
  strcat(url, SERVERNAME);
  strcat(url, "spool/");
  strcat(url, spoolman_id_char);

#ifdef DEBUG
  Serial.print("retrieveSpool API URL: ");
  Serial.println(url);
#endif

  http.begin(client, url);
  http.GET();

  deserializeJson(doc, http.getStream());

  // Disconnect
  http.end();

  mySpool.protocol = "openspool";
  mySpool.version = 1.0;
  mySpool.type = doc["filament"]["material"];
  mySpool.color_hex = doc["filament"]["color_hex"];
  mySpool.brand = doc["filament"]["vendor"]["name"];
  mySpool.min_temp = doc["filament"]["settings_extruder_temp"];
  mySpool.max_temp = doc["filament"]["settings_extruder_temp"];
  mySpool.k_factor = 0;
  mySpool.uuid = doc["extra"]["serial_number"];
  mySpool.spoolman_id = doc["id"];
  mySpool.status = EXIT_SUCCESS;

  // This means no spool data was returned
  if (mySpool.uuid == NULL) {
    mySpool.status = EXIT_FAILURE;
    return mySpool;
  }

  if (strcmp(mySpool.uuid, "\"\"") == 0) {
    Serial.println("No UUID found, generating a new one.");
    uuid.generate();
    mySpool.uuid = uuid.toCharArray();
  } else {
    Serial.println("UUID found on the RFID.");
  }

#ifdef DEBUG  // Print the values
  Serial.println("retrieveSpool returned:");
  Serial.print("protocol : ");
  Serial.println(mySpool.protocol);
  Serial.print("version : ");
  Serial.println(mySpool.version, 1);
  Serial.print("type : ");
  Serial.println(mySpool.type);
  Serial.print("color_hex : ");
  Serial.println(mySpool.color_hex);
  Serial.print("brand : ");
  Serial.println(mySpool.brand);
  Serial.print("min_temp : ");
  Serial.println(mySpool.min_temp);
  Serial.print("max_temp : ");
  Serial.println(mySpool.max_temp);
  Serial.print("UUID : ");
  Serial.println(mySpool.uuid);
  Serial.print("spoolman_id : ");
  Serial.println(mySpool.spoolman_id);
#endif

  return mySpool;
}

// Read data from the active RFID tag
// Read returns data for 4 blocks (16 bytes) at a time.
// startingByte: byte number to begin reading from
// length: number of bytes to be read
// outputBuffer: where to save the read data to
int readRfid(byte startingByte, uint16_t length, byte* outputBuffer) {
  MFRC522::StatusCode status;
  byte bufferSize = 18;
  byte readBuffer[18];
  byte block = 0;
  byte i;
  uint16_t outputIndex = 0;
  uint16_t startingBlock;
  uint16_t stopBlock;
  byte offset = byteOffset(startingByte);

  // determine the blocks to start and stop at
  startingBlock = byteToBlock(startingByte);
  stopBlock = byteToBlock(startingByte + length);
  if (byteOffset(startingByte + length)) {
    stopBlock++;
  }

  // Read 4 blocks in
  for (block = startingBlock; block < stopBlock; block += 4) {
    bufferSize = sizeof(readBuffer);
    status = mfrc522.MIFARE_Read(block, readBuffer, &bufferSize);
    if (status != mfrc522.STATUS_OK) {
      Serial.print(F("MIFARE_Read() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return -2;
    }

    // Move the data into the outputBuffer
    for (i = 0 + offset; i < bufferSize - 2 && outputIndex < length; i++, outputIndex++) {
      outputBuffer[outputIndex] = readBuffer[i];

#ifdef DEBUG  // Print the data to serial
      if (readBuffer[i] < 0x10)
        Serial.print(F("0"));
      Serial.print(readBuffer[i], HEX);
#endif
    }
#ifdef DEBUG
    Serial.println();
#endif

    // after the first iteration of copy there is no more offset
    offset = 0;
  }

  return 0;
}

// calculate which block a specific data byte will be in.
uint16_t byteToBlock(uint16_t byteNumber) {
  uint16_t blockNumber = 0;

  blockNumber = byteNumber / 4;

  return blockNumber;
}

// Determine the offset of a specific byte in a block
uint16_t byteOffset(uint16_t byteNumber) {
  uint16_t offset = 0;

  offset = byteNumber % 4;

  return offset;
}

// make API call to spoolman to update a spool's gross weight
uint8_t updateSpool(uint16_t spoolman_id, int16_t weight) {
  WiFiClient client;
  HTTPClient http;
  JsonDocument doc;
  char spoolman_id_char[5] = "\0";
  char url[99] = "\0";
  char payload[99] = "\0";
  char weight_char[5] = "\0";
  uint8_t returnCode = 0;

  itoa(spoolman_id, spoolman_id_char, 10);

  // https://donkie.github.io/spool/{spool_id}/measure
  strcat(url, SERVERNAME);
  strcat(url, "spool/");
  strcat(url, spoolman_id_char);
  strcat(url, "/measure");

#ifdef DEBUG
  Serial.print("Update URL: ");
  Serial.println(url);
#endif

  itoa(weight, weight_char, 10);

  strcat(payload, "{\"weight\":");
  strcat(payload, weight_char);
  strcat(payload, "}");

#ifdef DEBUG
  Serial.print("Update payload: ");
  Serial.println(payload);
#endif

  http.begin(client, url);
  returnCode = http.PUT((uint8_t*)payload, strlen(payload));

  http.end();

  if (returnCode == 200) {
    return EXIT_SUCCESS;
  }
  return returnCode % 255;
}

void blinkNumTimes(uint8_t color, uint8_t times) {
  lightsOff();

  if (color == LED_WHT) {
    for (uint8_t i = 0; i < times; i++) {
      digitalWrite(LED_BLU, HIGH);
      digitalWrite(LED_GRN, HIGH);
      digitalWrite(LED_RED, HIGH);
      delay(250);
      lightsOff();
      delay(250);
    }
  } else if (color == LED_YEL) {
    for (uint8_t i = 0; i < times; i++) {
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_GRN, HIGH);
      delay(250);
      lightsOff();
      delay(250);
    }
  } else if (color == LED_PUR) {
    for (uint8_t i = 0; i < times; i++) {
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_BLU, HIGH);
      delay(250);
      lightsOff();
      delay(250);
    }
  } else if (color == LED_CYN) {
    for (uint8_t i = 0; i < times; i++) {
      digitalWrite(LED_BLU, HIGH);
      digitalWrite(LED_GRN, HIGH);
      delay(250);
      lightsOff();
      delay(250);
    }
  } else {
    for (uint8_t i = 0; i < times; i++) {
      digitalWrite(color, HIGH);
      delay(250);
      digitalWrite(color, LOW);
      delay(250);
    }
  }
}

// Find a spool in the database based on UUID and return the spoolman_id
uint16_t findSpoolByUuid(const char* uuid) {
  WiFiClient client;
  HTTPClient http;
  JsonDocument doc;
  char url[99] = "\0";
  JsonDocument filter;
  uint16_t json_count = 0;
  char sn_char[48] = "\0";
  uint16_t spoolman_id = 0;

  // filter out unwanted data
  filter[0]["id"] = true;
  filter[0]["extra"] = true;

  // List all spools. Sadly you can't search by an extra field
  // https://donkie.github.io/spool
  strcat(url, SERVERNAME);
  strcat(url, "spool");

#ifdef DEBUG
  Serial.print("API URL: ");
  Serial.println(url);
#endif

  http.begin(client, url);
  http.GET();

  DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
  }

  json_count = doc.size();

  uint16_t i = 0;
  while (i < json_count) {
    JsonObject root = doc[i];
    const char* extra_serial_number = root["extra"]["serial_number"];

    if (extra_serial_number == NULL) {
      i++;
      continue;
    }

    uint16_t j = 0;

    for (uint16_t x = 0; x < strlen(extra_serial_number); x++) {
      if (extra_serial_number[x] != '\"') {  // Skip double quotes
        sn_char[j++] = extra_serial_number[x];
      }
    }

    sn_char[j] = '\0';

    if (strcmp(sn_char, uuid) == 0) {
      spoolman_id = root["id"];
      break;
    }
    i++;
  }

#ifdef DEBUG
  Serial.print("spoolman_id: ");
  Serial.println(spoolman_id);
  Serial.print("extra_serial_number: ");
  Serial.println(sn_char);
#endif

  // Disconnect
  http.end();

  return spoolman_id;
}
