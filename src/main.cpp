#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <MFRC522.h>
#include <SPI.h>
#include <WiFiManager.h>

#include "UUID.h"

void newCardFound();
void lightsOff();
void pulsingWait(uint8_t color);
void retrieveSpools();

UUID uuid;

// Spoolman server address
String serverName = "http://10.0.1.50:8000/";

// RGB LED Pins
#define LED_RED_PIN D1
#define LED_BLU_PIN D0
#define LED_GRN_PIN D2

int RGB_BRIGHT = 0;

// RFID Reader pins
#define RST_PIN D3  // Configurable, see typical pin layout above
#define SS_PIN D8   // Configurable, take a unused pin, only HIGH/LOW required, must be different to SS 2
// SPI_MOSI 	   D7
// SPI_MISO 	   D6
// SPI_SCK 	     D5

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

void setup() {
  Serial.begin(9600);
  // while (!Serial);

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
  Serial.print(F("UUID_LIB_VERSION: "));
  Serial.println(UUID_LIB_VERSION);
  uuid.setVariant4Mode();

  // Setup the RFID reader
  Serial.println(F("Starting the RFID reader..."));
  SPI.begin();                        // Init SPI bus
  mfrc522.PCD_Init();                 // Init MFRC522
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details

  // Setup the WiFi
  WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP
  // it is a good practice to make sure your code sets wifi mode how you want it.

  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  // wm.resetSettings();

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
}

void loop() {
  if (mfrc522.PICC_IsNewCardPresent()) {
    newCardFound();
  }

  // Pulse blue while waiting to read
  pulsingWait(LED_BLU_PIN);
  delay(1);
}

void newCardFound() {
  Serial.println("Found a new card...");
  lightsOff();
  digitalWrite(LED_GRN_PIN, HIGH);

  mfrc522.PICC_ReadCardSerial();
  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
  lightsOff();

  return;
}

void lightsOff() {
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_GRN_PIN, LOW);
  digitalWrite(LED_BLU_PIN, LOW);

  return;
}

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
