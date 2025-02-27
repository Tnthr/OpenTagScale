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

void newCardFound();
void lightsOff();
void pulsingWait(uint8_t color);
void retrieveSpools();
void setupScale();
int readRfidJson();
int readRfid(byte startingByte, uint16_t length, byte *outputBuffer);
uint16_t byteToBlock(uint16_t byteNumber);
uint16_t byteOffset(uint16_t byteNumber);

union ArrayToInteger {
  byte array[2];
  uint16_t integer;
};

// A buffer to hold the largest possible NTAG216 written data
byte jsonBuffer[889] = "\0";

// Spoolman server address
String serverName = "http://10.0.1.50:8000/";

// RGB LED Pins
#define LED_RED_PIN D3
#define LED_BLU_PIN D0
#define LED_GRN_PIN D4

// Global variables
int RGB_BRIGHT = 0;
HX711 scale;
UUID uuid;
int scaleCal = 12;
long weight = 0;

// RFID Reader pins
// #define RST_PIN D3  // Configurable, not required
#define SS_PIN D8  // Configurable, take a unused pin, only HIGH/LOW required
// SPI_MOSI 	   D7
// SPI_MISO 	   D6
// SPI_SCK 	     D5

// HX711 pins
#define LOADCELL_DOUT_PIN D1
#define LOADCELL_SCK_PIN D2

MFRC522 mfrc522(SS_PIN);  // Create MFRC522 instance

void setup() {
  Serial.begin(9600);
  // while (!Serial);

  EEPROM.begin(8);

  // Setup the RGB LED
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_BLU_PIN, OUTPUT);
  pinMode(LED_GRN_PIN, OUTPUT);

  lightsOff();

  // Do an annoying bootup blink
  digitalWrite(LED_GRN_PIN, HIGH);
  delay(100);
  digitalWrite(LED_GRN_PIN, LOW);
  digitalWrite(LED_RED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_BLU_PIN, HIGH);
  delay(100);
  digitalWrite(LED_BLU_PIN, LOW);
  digitalWrite(LED_GRN_PIN, HIGH);
  delay(100);
  digitalWrite(LED_GRN_PIN, LOW);
  digitalWrite(LED_RED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_BLU_PIN, HIGH);
  delay(100);
  digitalWrite(LED_BLU_PIN, LOW);

  // Setup the UUID library
  Serial.println();
  Serial.print(F("Checking UUID Library...\t"));
  Serial.println(UUID_LIB_VERSION);
  uuid.setVariant4Mode();

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
  WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP
  // it is a good practice to make sure your code sets wifi mode how you want it.

  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  if (DEBUG_RESET) {
    wm.resetSettings();
  }

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  // Stop trying after 20 seconds
  wm.setConnectTimeout(20);

  // Solid red light while we connect to wifi
  digitalWrite(LED_RED_PIN, HIGH);

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
  digitalWrite(LED_RED_PIN, LOW);
}

void loop() {
  if (mfrc522.PICC_IsNewCardPresent()) {
    newCardFound();
  }

  if (scale.is_ready()) {
    long reading = scale.get_units(5);
    if (reading < 0) {
      reading = 0;
    }
    if (reading != weight) {
      Serial.print("HX711 reading: ");
      Serial.println(reading);
      weight = reading;
    }
  }

  // Pulse blue while waiting to read
  pulsingWait(LED_BLU_PIN);
  // delay(1);
}

// Called when a new RFID card is found
void newCardFound() {
  Serial.println("Found a new card...");
  lightsOff();
  digitalWrite(LED_GRN_PIN, HIGH);

  readRfidJson();

  lightsOff();

  return;
}

// Turn off all of the lights
void lightsOff() {
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_GRN_PIN, LOW);
  digitalWrite(LED_BLU_PIN, LOW);

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

// make API call to spoolman to retrieve a list of spools
void retrieveSpools() {
  WiFiClient client;
  HTTPClient http;

  String APIPath = serverName + "spool";

  http.begin(client, APIPath);
  http.GET();

  // Print the response
  Serial.print(http.getString());

  // Disconnect
  http.end();
}

// how to gen a new uuid
//  uuid.generate();
//  uuid.printTo(Serial);

// Scale calibration is done here
// Need to make it callable as needed
void setupScale() {
  float average = 0;
  double calFactor = 0.0;

  scale.set_scale();
  scale.tare();
  yield();

  EEPROM.get(0, calFactor);

  // Serial.print("stored calFactor: \t");
  Serial.println(calFactor);

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

  Serial.print("read average: \t\t");
  average = average + scale.read_average(10);
  yield();
  average = average + scale.read_average(10);
  yield();
  average = average / 2;

  Serial.println(average);  // print the average of 20 readings from the ADC

  Serial.println("Add 1082g weight...");
  for (int i = 10; i > 0; i--) {
    Serial.print(i);
    Serial.print("... ");
    delay(1000);
  }
  Serial.println("");

  average = scale.get_units(10);
  yield();
  calFactor = average / 1082;

  Serial.print("scale factor: \t\t");
  Serial.println(calFactor);
  scale.set_scale(calFactor);

  EEPROM.put(0, calFactor);
  bool ok = EEPROM.commit();
  Serial.println((ok) ? "Saved calFactor" : "Saving calFactor failed");

  Serial.println("Remove weight...");
}

int readRfidJson() {
  ArrayToInteger ndefSize;
  byte dataBlocks[4];
  int byteIndex = 16;
  int status = 0;

  // read from byte 16, 4 bytes, put them in dataBlocks[]
  // This should be the first TLV
  status = readRfid(byteIndex, 4, dataBlocks);

  if (status != 0) {
    Serial.print("readRFID returned error! ");
    Serial.println(status);
    return status;
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

  } else if (dataBlocks[0] == 0x00) {
    Serial.println("Null TLV");
    byteIndex++;
  } else {
    // TODO: Finish handling of other TLVs just in case
    Serial.print("Different TLV: ");
    Serial.println(dataBlocks[0], HEX);
  }

  // read the entire NDEF data block
  status = readRfid(byteIndex, ndefSize.integer, jsonBuffer);

  if (status != 0) {
    Serial.print("readRFID returned error! ");
    Serial.println(status);
    return status;
  }

  mfrc522.PICC_HaltA();
  return 0;
}

// Read data from the active RFID tag
// Read returns data for 4 blocks (16 bytes) at a time.
// startingByte: byte number to begin reading from
// length: number of bytes to be read
// outputBuffer: where to save the read data to
int readRfid(byte startingByte, uint16_t length, byte *outputBuffer) {
  MFRC522::StatusCode status;
  byte bufferSize = 18;
  byte readBuffer[18];
  byte block = 0;
  byte i;
  uint8_t outputIndex = 0;
  uint8_t startingBlock;
  uint8_t stopBlock;
  byte offset = byteOffset(startingByte);

  // determine the blocks to start and stop at
  startingBlock = byteToBlock(startingByte);
  stopBlock = byteToBlock(startingByte + length);

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
    for (i = 0 + offset; i < 16 && outputIndex < length; i++, outputIndex++) {
      outputBuffer[outputIndex] = readBuffer[i];

#ifdef DEBUG // Print the data to serial
      if (readBuffer[i] < 0x10)
        Serial.print(F("0"));
      Serial.print(readBuffer[i], HEX);
    }
    Serial.println();
#else
    }
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