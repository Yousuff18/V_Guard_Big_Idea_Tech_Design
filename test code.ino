/*
 * ESP32 Recipe Manager with Speed Control and Power Monitoring
 * 
 * Hardware Setup:
 * - ESP32 Dev Module
 * - ZMPT101B AC Voltage sensor on GPIO 34 (requires voltage divider for safety)
 * - SCT013 Current Transformer on GPIO 35 (requires burden resistor ~47Ω)
 * - Speed control relays on GPIOs 16, 17, 5
 * 
 * IMPORTANT CALIBRATION:
 * 1. Run the calibration sketch first with a purely resistive load
 * 2. Replace V_CALIB and I_CALIB values below with calibrated values
 * 3. For better accuracy, consider using ADS1115 ADC module
 * 
 * ADC Setup Notes:
 * - ZMPT101B: Connect output through voltage divider (1:1) with 1.65V bias
 * - SCT013: Connect through burden resistor (47Ω) with 1.65V bias
 * - Both sensors require proper AC coupling and bias voltage at ESP32 ADC mid-point
 */

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ========== CONFIGURATION SECTION - MODIFY THESE VALUES ==========

// Network Configuration
const char* AP_SSID = "ESP32-RecipeAP";           // Change this to your preferred SSID
const char* AP_PASSWORD = "Recipe123456";          // Change this to your preferred password (min 8 chars)

// GPIO Pin Configuration - Change these if using different pins
#define SPEED1_PIN 16                               // Speed 1 relay control pin
#define SPEED2_PIN 17                               // Speed 2 relay control pin  
#define SPEED3_PIN 5                                // Speed 3 relay control pin

// Sensor Pin Configuration
#define VOLTAGE_PIN 34                              // ZMPT101B voltage sensor pin
#define CURRENT_PIN 35                              // SCT013 current sensor pin

// Calibration Constants - REPLACE WITH VALUES FROM CALIBRATION SKETCH
#define V_CALIB 1000                                // Replace with CV1 after calibration
#define I_CALIB 1000                                // Replace with CI1 after calibration

// Timing Configuration
#define SENSOR_UPDATE_INTERVAL 1000                 // Sensor reading interval in ms
#define ADC_SAMPLES 1000                            // Number of ADC samples for RMS calculation

// ========== END CONFIGURATION SECTION ==========

// Web server and global variables
WebServer server(80);
unsigned long lastSensorUpdate = 0;
unsigned long recipeEndTime = 0;
bool recipeRunning = false;
String runningRecipeName = "";
int currentSpeed = 0; // 0=OFF, 1=Speed1, 2=Speed2, 3=Speed3

// Sensor variables
float voltage = 0.0;
float current = 0.0;
float power = 0.0;

// Recipe timer variables
unsigned long recipeStartTime = 0;
unsigned int recipeTimerSeconds = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("An error occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");
  
  // Initialize GPIO pins
  pinMode(SPEED1_PIN, OUTPUT);
  pinMode(SPEED2_PIN, OUTPUT);
  pinMode(SPEED3_PIN, OUTPUT);
  
  // Set all speed pins to LOW (OFF state)
  digitalWrite(SPEED1_PIN, LOW);
  digitalWrite(SPEED2_PIN, LOW);
  digitalWrite(SPEED3_PIN, LOW);
  
  // Configure ADC pins with 11dB attenuation for 0-3.3V range
  analogSetPinAttenuation(VOLTAGE_PIN, ADC_11db);
  analogSetPinAttenuation(CURRENT_PIN, ADC_11db);
  
  // Set up Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Access Point IP address: ");
  Serial.println(IP);
  Serial.printf("Connect to WiFi: %s\n", AP_SSID);
  Serial.printf("Password: %s\n", AP_PASSWORD);
  Serial.printf("Then open browser to: http://%s\n", IP.toString().c_str());
  
  // Set up web server routes
  setupRoutes();
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  
  // Update sensor readings
  if (millis() - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
    readSensors();
    lastSensorUpdate = millis();
  }
  
  // Check recipe timer
  checkRecipeTimer();
}

void setupRoutes() {
  // Serve main page
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getMainPage());
  });
  
  // API endpoint for sensor status
  server.on("/status", HTTP_GET, []() {
    StaticJsonDocument<200> doc;
    doc["voltage"] = voltage;
    doc["current"] = current;
    doc["power"] = power;
    doc["speedState"] = currentSpeed;
    doc["recipeRunning"] = recipeRunning;
    doc["runningRecipe"] = runningRecipeName;
    
    if (recipeRunning && recipeTimerSeconds > 0) {
      unsigned long elapsed = (millis() - recipeStartTime) / 1000;
      doc["remainingTime"] = (elapsed < recipeTimerSeconds) ? (recipeTimerSeconds - elapsed) : 0;
    } else {
      doc["remainingTime"] = 0;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });
  
  // API endpoint for speed control
  server.on("/toggle", HTTP_POST, []() {
    if (recipeRunning) {
      server.send(400, "application/json", "{\"error\":\"Cannot change speed while recipe is running\"}");
      return;
    }
    
    if (server.hasArg("speed")) {
      int speed = server.arg("speed").toInt();
      setSpeed(speed);
      server.send(200, "application/json", "{\"success\":true}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Missing speed parameter\"}");
    }
  });
  
  // Recipe management endpoints
  server.on("/recipes", HTTP_GET, handleGetRecipes);
  server.on("/recipes", HTTP_POST, handleSaveRecipe);
  server.on("/recipes", HTTP_DELETE, handleDeleteRecipe);
  server.on("/run-recipe", HTTP_POST, handleRunRecipe);
  
  // 404 handler
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
}

void setSpeed(int speed) {
  // Turn off all speed pins first
  digitalWrite(SPEED1_PIN, LOW);
  digitalWrite(SPEED2_PIN, LOW);
  digitalWrite(SPEED3_PIN, LOW);
  
  currentSpeed = speed;
  
  // Turn on the selected speed pin
  switch (speed) {
    case 1:
      digitalWrite(SPEED1_PIN, HIGH);
      Serial.println("Speed 1 ON");
      break;
    case 2:
      digitalWrite(SPEED2_PIN, HIGH);
      Serial.println("Speed 2 ON");
      break;
    case 3:
      digitalWrite(SPEED3_PIN, HIGH);
      Serial.println("Speed 3 ON");
      break;
    default:
      currentSpeed = 0;
      Serial.println("All speeds OFF");
      break;
  }
}

void readSensors() {
  // Read voltage sensor (ZMPT101B)
  // Perform RMS calculation
  long voltageSum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    int sampleV = analogRead(VOLTAGE_PIN);
    // Remove DC bias (assuming 1.65V bias = 2048 ADC counts)
    int acV = sampleV - 2048;
    voltageSum += (long)acV * acV;
  }
  
  float voltageRMS = sqrt((float)voltageSum / ADC_SAMPLES);
  // Convert to actual voltage using calibration factor
  voltage = (voltageRMS * 3.3 / 4096.0) * V_CALIB / 1000.0;
  
  // Read current sensor (SCT013)
  long currentSum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    int sampleI = analogRead(CURRENT_PIN);
    // Remove DC bias
    int acI = sampleI - 2048;
    currentSum += (long)acI * acI;
  }
  
  float currentRMS = sqrt((float)currentSum / ADC_SAMPLES);
  // Convert to actual current using calibration factor
  current = (currentRMS * 3.3 / 4096.0) * I_CALIB / 1000.0;
  
  // Calculate power
  power = voltage * current;
  
  // Debug output
  Serial.printf("V: %.2fV, I: %.2fA, P: %.2fW\n", voltage, current, power);
}

void checkRecipeTimer() {
  if (recipeRunning && recipeTimerSeconds > 0) {
    unsigned long elapsed = (millis() - recipeStartTime) / 1000;
    if (elapsed >= recipeTimerSeconds) {
      // Timer finished, stop recipe
      setSpeed(0);
      recipeRunning = false;
      runningRecipeName = "";
      recipeTimerSeconds = 0;
      Serial.println("Recipe completed - stopping motor");
    }
  }
}

void handleGetRecipes() {
  File file = LittleFS.open("/recipes.json", "r");
  if (!file) {
    server.send(200, "application/json", "[]");
    return;
  }
  
  String recipes = file.readString();
  file.close();
  server.send(200, "application/json", recipes);
}

void handleSaveRecipe() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  // Load existing recipes
  DynamicJsonDocument recipes(8192);
  File file = LittleFS.open("/recipes.json", "r");
  if (file) {
    deserializeJson(recipes, file);
    file.close();
  } else {
    recipes = DynamicJsonDocument(8192);
  }
  
  // Add new recipe
  JsonArray array = recipes.as<JsonArray>();
  array.add(doc);
  
  // Save back to file
  file = LittleFS.open("/recipes.json", "w");
  if (file) {
    serializeJson(recipes, file);
    file.close();
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to save recipe\"}");
  }
}

void handleDeleteRecipe() {
  if (!server.hasArg("name")) {
    server.send(400, "application/json", "{\"error\":\"Missing recipe name\"}");
    return;
  }
  
  String recipeName = server.arg("name");
  
  // Load existing recipes
  DynamicJsonDocument recipes(8192);
  File file = LittleFS.open("/recipes.json", "r");
  if (!file) {
    server.send(404, "application/json", "{\"error\":\"No recipes found\"}");
    return;
  }
  
  deserializeJson(recipes, file);
  file.close();
  
  // Find and remove recipe
  JsonArray array = recipes.as<JsonArray>();
  for (int i = array.size() - 1; i >= 0; i--) {
    if (array[i]["recipeName"] == recipeName) {
      array.remove(i);
      break;
    }
  }
  
  // Save back to file
  file = LittleFS.open("/recipes.json", "w");
  if (file) {
    serializeJson(recipes, file);
    file.close();
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to delete recipe\"}");
  }
}

void handleRunRecipe() {
  if (recipeRunning) {
    server.send(400, "application/json", "{\"error\":\"Recipe already running\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  String recipeName = doc["recipeName"];
  int speed = doc["speed"];
  int timer = doc["timer"];
  
  // Start recipe
  recipeRunning = true;
  runningRecipeName = recipeName;
  recipeStartTime = millis();
  recipeTimerSeconds = timer;
  
  setSpeed(speed);
  
  Serial.printf("Starting recipe: %s, Speed: %d, Timer: %ds\n", 
                recipeName.c_str(), speed, timer);
  
  server.send(200, "application/json", "{\"success\":true}");
}

// Continue with HTML page content...
String getMainPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Recipe Manager</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 10px;
        }
        
        .container {
            max-width: 480px;
            margin: 0 auto;
            background: white;
            border-radius: 20px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.1);
            overflow: hidden;
        }
        
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            text-align: center;
            position: relative;
        }
        
        .hamburger {
            position: absolute;
            right: 20px;
            top: 20px;
            background: none;
            border: none;
            color: white;
            font-size: 24px;
            cursor: pointer;
            padding: 5px;
        }
        
        .content {
            padding: 20px;
        }
        
        .sensor-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 15px;
            margin-bottom: 30px;
        }
        
        .sensor-card {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 15px;
            text-align: center;
            border: 2px solid #e9ecef;
        }
        
        .sensor-value {
            font-size: 1.5em;
            font-weight: bold;
            color: #333;
            margin-bottom: 5px;
        }
        
        .sensor-label {
            color: #666;
            font-size: 0.9em;
        }
        
        .speed-controls {
            margin-bottom: 30px;
        }
        
        .speed-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 15px;
        }
        
        .toggle-switch {
            position: relative;
            width: 100%;
            height: 60px;
        }
        
        .toggle-switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        
        .toggle-label {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            border-radius: 30px;
            transition: 0.3s;
            display: flex;
            align-items: center;
            justify-content: center;
            font-weight: bold;
            color: #666;
        }
        
        .toggle-switch input:checked + .toggle-label {
            background-color: #4CAF50;
            color: white;
        }
        
        .toggle-switch input:disabled + .toggle-label {
            background-color: #ddd;
            cursor: not-allowed;
            opacity: 0.6;
        }
        
        .recipes-section {
            display: none;
        }
        
        .recipe-form {
            background: #f8f9fa;
            padding: 20px;
            border-radius: 15px;
            margin-bottom: 20px;
        }
        
        .form-group {
            margin-bottom: 15px;
        }
        
        .form-group label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
            color: #333;
        }
        
        .form-group input, .form-group select {
            width: 100%;
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 8px;
            font-size: 16px;
        }
        
        .ingredients-table {
            background: white;
            border-radius: 10px;
            overflow: hidden;
            margin-bottom: 15px;
            border: 2px solid #ddd;
        }
        
        .table-header {
            background: #667eea;
            color: white;
            display: grid;
            grid-template-columns: 2fr 1fr 1fr;
            gap: 1px;
            font-weight: bold;
        }
        
        .table-header > div {
            padding: 10px;
            text-align: center;
        }
        
        .ingredient-row {
            display: grid;
            grid-template-columns: 2fr 1fr 1fr;
            gap: 1px;
            background: #f8f9fa;
        }
        
        .ingredient-row input {
            border: none;
            padding: 10px;
            background: white;
            font-size: 14px;
        }
        
        .add-ingredient-btn {
            background: #28a745;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 8px;
            cursor: pointer;
            margin-bottom: 15px;
        }
        
        .btn {
            background: #667eea;
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 8px;
            cursor: pointer;
            font-size: 16px;
            width: 100%;
            margin-bottom: 10px;
        }
        
        .btn:hover {
            background: #5a6fd8;
        }
        
        .btn-danger {
            background: #dc3545;
        }
        
        .btn-danger:hover {
            background: #c82333;
        }
        
        .recipe-card {
            background: #f8f9fa;
            border-radius: 15px;
            margin-bottom: 15px;
            overflow: hidden;
            border: 2px solid #ddd;
        }
        
        .recipe-header {
            padding: 15px;
            background: white;
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .recipe-title {
            font-weight: bold;
            font-size: 1.1em;
        }
        
        .recipe-info {
            font-size: 0.9em;
            color: #666;
        }
        
        .recipe-details {
            display: none;
            padding: 15px;
            border-top: 1px solid #ddd;
        }
        
        .recipe-actions {
            display: flex;
            gap: 10px;
            margin-top: 15px;
        }
        
        .back-btn {
            background: #6c757d;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 8px;
            cursor: pointer;
            margin-bottom: 20px;
        }
        
        .timer-display {
            background: #ff6b6b;
            color: white;
            padding: 10px;
            border-radius: 8px;
            text-align: center;
            font-size: 1.2em;
            font-weight: bold;
            margin-bottom: 15px;
        }
        
        .loading {
            display: inline-block;
            width: 20px;
            height: 20px;
            border: 3px solid #f3f3f3;
            border-top: 3px solid #3498db;
            border-radius: 50%;
            animation: spin 1s linear infinite;
        }
        
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        
        .hidden {
            display: none;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Recipe Manager</h1>
            <button class="hamburger" onclick="toggleView()">☰</button>
        </div>
        
        <div class="content">
            <!-- Main Control Page -->
            <div id="mainPage">
                <!-- Sensor Readings -->
                <div class="sensor-grid">
                    <div class="sensor-card">
                        <div class="sensor-value" id="voltage">0.0</div>
                        <div class="sensor-label">Volts</div>
                    </div>
                    <div class="sensor-card">
                        <div class="sensor-value" id="current">0.0</div>
                        <div class="sensor-label">Amps</div>
                    </div>
                    <div class="sensor-card">
                        <div class="sensor-value" id="power">0.0</div>
                        <div class="sensor-label">Watts</div>
                    </div>
                </div>
                
                <!-- Timer Display (shown when recipe is running) -->
                <div id="timerDisplay" class="timer-display hidden">
                    <div>Recipe Running: <span id="runningRecipeName"></