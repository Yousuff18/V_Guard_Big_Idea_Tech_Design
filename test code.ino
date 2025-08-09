/*
  ESP32 Recipe Webserver
  - ESP32 acts as WiFi Access Point (AP)
  - Web UI to control speed GPIOs with toggles (OFF, Speed 1,2,3)
  - Shows live Voltage, Current, Power from ZMPT101B & SCT013 sensors (ADC)
  - Recipe manager with multi-ingredient recipes, timer-run, and persistence in LittleFS

  Wiring & Calibration:
  - Voltage sensor ZMPT101B connected to GPIO 34 (ADC1_CH6)
  - Current sensor SCT013 connected to GPIO 35 (ADC1_CH7)
  - Use burden resistor with SCT013 (~33Ω typical)
  - Use mid-rail bias (~1.65V) for ADC input (hardware)
  - Calibrate V_CALIB and I_CALIB using calibration sketch (see /calibration folder)
  - Change SSID, Password, pins, calibration constants below as needed

  LittleFS used for storing web files & recipes JSON

  Created by ChatGPT (GPT-4)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "EmonLib.h"

// --------- User Configurable Constants ---------
#define WIFI_SSID     "ESP32-RecipeAP"
#define WIFI_PASSWORD "12345678"

#define SPEED1_PIN 16
#define SPEED2_PIN 17
#define SPEED3_PIN 5

#define VOLTAGE_PIN 34  // ZMPT101B voltage sensor ADC pin
#define CURRENT_PIN 35  // SCT013 current sensor ADC pin

#define V_CALIB 1000   // <-- EDIT THIS after calibration
#define I_CALIB 1000   // <-- EDIT THIS after calibration

#define ADC_ATTENUATION ADC_11db

#define SENSOR_READ_INTERVAL_MS 1000

// --------- Globals ---------
WebServer server(80);
EnergyMonitor emon1;

volatile uint8_t currentSpeedState = 0; // 0=Off, 1=Speed1, 2=Speed2, 3=Speed3
unsigned long speedRunEndTime = 0; // millis() when current speed run ends

// Sensor readings
float measuredVoltage = 0.0;
float measuredCurrent = 0.0;
float measuredPower = 0.0;

// Forward declarations
void handleRoot();
void handleStatus();
void handleToggle();
void handleRecipes();
void setupRoutes();
void readSensors();
void setSpeed(uint8_t speed);
void stopAllSpeeds();
void loadRecipes();
void saveRecipes();

DynamicJsonDocument recipesDoc(16384); // Adjust size if needed

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Setup GPIOs
  pinMode(SPEED1_PIN, OUTPUT);
  pinMode(SPEED2_PIN, OUTPUT);
  pinMode(SPEED3_PIN, OUTPUT);
  stopAllSpeeds();

  // Setup ADC attenuation
  analogSetPinAttenuation(VOLTAGE_PIN, ADC_ATTENUATION);
  analogSetPinAttenuation(CURRENT_PIN, ADC_ATTENUATION);

  // Setup energy monitor (EmonLib)
  emon1.voltage(VOLTAGE_PIN, V_CALIB, 1.732);
  emon1.current(CURRENT_PIN, I_CALIB);

  // Initialize LittleFS
  if(!LittleFS.begin()){
    Serial.println("An error has occurred while mounting LittleFS");
  } else {
    Serial.println("LittleFS mounted successfully");
  }

  // Setup WiFi Access Point
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Load recipes from LittleFS
  loadRecipes();

  // Setup HTTP server routes
  setupRoutes();

  server.begin();

  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  // Update sensor readings at interval
  static unsigned long lastSensorRead = 0;
  unsigned long now = millis();
  if(now - lastSensorRead > SENSOR_READ_INTERVAL_MS){
    readSensors();
    lastSensorRead = now;
  }

  // Handle auto-stop for speed timer
  if(currentSpeedState != 0 && speedRunEndTime > 0 && now >= speedRunEndTime){
    stopAllSpeeds();
    currentSpeedState = 0;
    speedRunEndTime = 0;
  }
}

// --- Implementations ---

void stopAllSpeeds(){
  digitalWrite(SPEED1_PIN, LOW);
  digitalWrite(SPEED2_PIN, LOW);
  digitalWrite(SPEED3_PIN, LOW);
}

void setSpeed(uint8_t speed){
  // speed: 0=off,1=Speed1,2=Speed2,3=Speed3
  stopAllSpeeds();
  switch(speed){
    case 1: digitalWrite(SPEED1_PIN, HIGH); break;
    case 2: digitalWrite(SPEED2_PIN, HIGH); break;
    case 3: digitalWrite(SPEED3_PIN, HIGH); break;
    default: break;
  }
  currentSpeedState = speed;
}

void readSensors(){
  emon1.calcVI(142, 1480); // Default samples for stable RMS
  measuredVoltage = emon1.Vrms;
  measuredCurrent = emon1.Irms;
  measuredPower = measuredVoltage * measuredCurrent;
}

void setupRoutes(){
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/recipes", HTTP_GET, handleRecipes);
  server.on("/recipes", HTTP_POST, handleRecipes);
  server.on("/recipes", HTTP_DELETE, handleRecipes);

  // Serve static files (html/css/js) from LittleFS /data folder
  server.serveStatic("/", LittleFS, "/index.html");
  server.serveStatic("/app.js", LittleFS, "/app.js");
  server.serveStatic("/styles.css", LittleFS, "/styles.css");
}

void handleRoot(){
  server.sendHeader("Location", "/index.html");
  server.send(302, "text/plain", "");
}

void handleStatus(){
  StaticJsonDocument<256> doc;
  doc["voltage"] = measuredVoltage;
  doc["current"] = measuredCurrent;
  doc["power"] = measuredPower;
  doc["speedState"] = currentSpeedState;
  String resp;
  serializeJson(doc, resp);
  server.send(200, "application/json", resp);
}

void handleToggle(){
  if(server.hasArg("speed")){
    String speedStr = server.arg("speed");
    uint8_t speed = speedStr.toInt();
    if(speed <= 3){
      if(speed == 0){ // OFF
        stopAllSpeeds();
        speedRunEndTime = 0;
      } else {
        // When toggled manually, no auto timer, so clear timer end
        speedRunEndTime = 0;
        setSpeed(speed);
      }
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid speed value\"}");
}

void handleRecipes(){
  if(server.method() == HTTP_GET){
    // Return recipes JSON
    String jsonString;
    serializeJson(recipesDoc, jsonString);
    server.send(200, "application/json", jsonString);
  }
  else if(server.method() == HTTP_POST){
    if(server.hasArg("plain") == false){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing body\"}");
      return;
    }
    String body = server.arg("plain");
    DynamicJsonDocument newRecipeDoc(8192);
    DeserializationError error = deserializeJson(newRecipeDoc, body);
    if(error){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
      return;
    }
    // Add new recipe to recipesDoc
    JsonArray recipes = recipesDoc.as<JsonArray>();
    if(recipes.isNull()){
      recipesDoc.clear();
      recipes = recipesDoc.to<JsonArray>();
    }
    recipes.add(newRecipeDoc);
    saveRecipes();
    server.send(200, "application/json", "{\"success\":true}");
  }
  else if(server.method() == HTTP_DELETE){
    // Expect query param id (index)
    if(!server.hasArg("id")){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing id\"}");
      return;
    }
    int id = server.arg("id").toInt();
    JsonArray recipes = recipesDoc.as<JsonArray>();
    if(recipes.isNull() || id < 0 || id >= (int)recipes.size()){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid id\"}");
      return;
    }
    // Remove recipe at index
    JsonArray newArray = DynamicJsonDocument(16384).to<JsonArray>();
    for(size_t i=0; i < recipes.size(); i++){
      if((int)i != id){
        newArray.add(recipes[i]);
      }
    }
    recipesDoc.clear();
    recipesDoc.to<JsonArray>().addAll(newArray);
    saveRecipes();
    server.send(200, "application/json", "{\"success\":true}");
  }
}

void loadRecipes(){
  if(LittleFS.exists("/recipes.json")){
    File f = LittleFS.open("/recipes.json", "r");
    if(f){
      DeserializationError err = deserializeJson(recipesDoc, f);
      if(err){
        Serial.println("Failed to parse recipes.json");
        recipesDoc.clear();
      }
      f.close();
    }
  } else {
    recipesDoc.clear();
  }
}

void saveRecipes(){
  File f = LittleFS.open("/recipes.json", "w");
  if(!f){
    Serial.println("Failed to open recipes.json for writing");
    return;
  }
  serializeJson(recipesDoc, f);
  f.close();
}