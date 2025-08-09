/*
 * ESP32 Recipe Controller with Power Monitoring - Enhanced Recipe UI
 * 
 * HARDWARE SETUP:
 * ===============
 * SPEED CONTROL PINS:
 * - GPIO 16 (SPEED1_PIN) -> Relay 1 or Motor Controller Input 1
 * - GPIO 17 (SPEED2_PIN) -> Relay 2 or Motor Controller Input 2
 * - GPIO 5  (SPEED3_PIN) -> Relay 3 or Motor Controller Input 3
 * 
 * POWER MONITORING:
 * - GPIO 34 (V_PIN) -> ZMPT101B voltage sensor output
 * - GPIO 35 (I_PIN) -> SCT013 current transformer
 * 
 * CALIBRATION:
 * ============
 * 1. Upload and run the calibration sketch first
 * 2. Use a purely resistive load (incandescent bulb, heater)
 * 3. Measure actual voltage/current with multimeter
 * 4. Calculate: New CV1 = (Vreal * 1000) / Vesp32
 * 5. Calculate: New CI1 = (Ireal * 1000) / Iesp32
 * 6. Replace V_CALIB and I_CALIB values below
 */

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Ticker.h>

// ==================== CONFIGURATION ====================
#define AP_SSID "ESP32-RecipeAP"
#define AP_PASSWORD "recipe123"

#define SPEED1_PIN 16
#define SPEED2_PIN 17  
#define SPEED3_PIN 5
#define V_PIN 34
#define I_PIN 35

#define V_CALIB 1000
#define I_CALIB 1000

#define UPDATE_INTERVAL 1000
#define SAMPLES 100
#define ADC_BITS 12
#define ADC_MAX 4095
#define VREF 3.3

// ==================== GLOBAL VARIABLES ====================
WebServer server(80);
Ticker sensorTicker;
Ticker recipeTimer;

float voltage = 0.0;
float current = 0.0;
float power = 0.0;
int currentSpeed = 0;
bool recipeRunning = false;
unsigned long recipeStartTime = 0;
unsigned long recipeDuration = 0;
int recipeSpeed = 0;
unsigned long recipeRemainingTime = 0;

// ==================== HTML CONTENT (SPLIT INTO PARTS) ====================
const char* htmlPart1 = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Recipe Controller</title>
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
            color: #333;
        }
        
        .container {
            max-width: 1000px;
            margin: 0 auto;
            padding: 20px;
        }
        
        .header {
            background: rgba(255, 255, 255, 0.95);
            border-radius: 15px;
            padding: 20px;
            margin-bottom: 20px;
            box-shadow: 0 8px 32px rgba(31, 38, 135, 0.37);
            backdrop-filter: blur(4px);
            border: 1px solid rgba(255, 255, 255, 0.18);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .hamburger {
            cursor: pointer;
            padding: 10px;
            border-radius: 8px;
            transition: background 0.3s;
        }
        
        .hamburger:hover {
            background: rgba(0, 0, 0, 0.1);
        }
        
        .hamburger span {
            display: block;
            width: 25px;
            height: 3px;
            background: #333;
            margin: 5px 0;
            transition: 0.3s;
        }
        
        .card {
            background: rgba(255, 255, 255, 0.95);
            border-radius: 15px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 8px 32px rgba(31, 38, 135, 0.37);
            backdrop-filter: blur(4px);
            border: 1px solid rgba(255, 255, 255, 0.18);
        }
        
        .power-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .power-item {
            text-align: center;
            padding: 15px;
            background: linear-gradient(45deg, #f0f2f5, #e1e8ed);
            border-radius: 10px;
            border: 2px solid transparent;
            transition: all 0.3s ease;
        }
        
        .power-item:hover {
            border-color: #667eea;
            transform: translateY(-2px);
        }
        
        .power-value {
            font-size: 1.8em;
            font-weight: bold;
            color: #667eea;
            margin-bottom: 5px;
        }
        
        .power-label {
            font-size: 0.9em;
            color: #666;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
)rawliteral";

const char* htmlPart2 = R"rawliteral(
        .speed-controls {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }
        
        .toggle-container {
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 10px;
        }
        
        .toggle-label {
            font-weight: bold;
            text-transform: uppercase;
            letter-spacing: 1px;
            font-size: 0.9em;
            color: #666;
        }
        
        .toggle-switch {
            position: relative;
            width: 60px;
            height: 34px;
        }
        
        .toggle-switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: .4s;
            border-radius: 34px;
        }
        
        .slider:before {
            position: absolute;
            content: "";
            height: 26px;
            width: 26px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        
        input:checked + .slider {
            background-color: #667eea;
        }
        
        input:checked + .slider:before {
            transform: translateX(26px);
        }
        
        .toggle-switch.off input:checked + .slider {
            background-color: #ff6b6b;
        }
        
        .toggle-switch.speed1 input:checked + .slider {
            background-color: #4ecdc4;
        }
        
        .toggle-switch.speed2 input:checked + .slider {
            background-color: #45b7d1;
        }
        
        .toggle-switch.speed3 input:checked + .slider {
            background-color: #f093fb;
        }
        
        .recipe-section {
            display: none;
            animation: fadeIn 0.5s ease-in-out;
        }
        
        .recipe-section.show {
            display: block;
        }
        
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }
        
        .recipe-form {
            background: #f8f9fa;
            padding: 20px;
            border-radius: 10px;
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
            font-size: 1em;
            transition: border-color 0.3s;
        }
        
        .form-group input:focus, .form-group select:focus {
            border-color: #667eea;
            outline: none;
        }
)rawliteral";

const char* htmlPart3 = R"rawliteral(
        .ingredient-table {
            background: white;
            border-radius: 10px;
            overflow: hidden;
            border: 2px solid #ddd;
            margin-bottom: 20px;
        }
        
        .table-header {
            background: linear-gradient(45deg, #667eea, #764ba2);
            color: white;
            display: grid;
            grid-template-columns: 2fr 1fr 1fr 50px;
            gap: 1px;
            font-weight: bold;
            text-align: center;
            padding: 0;
        }
        
        .table-header > div {
            padding: 15px 10px;
            background: linear-gradient(45deg, #667eea, #764ba2);
        }
        
        .ingredient-row {
            display: grid;
            grid-template-columns: 2fr 1fr 1fr 50px;
            gap: 1px;
            background: #f0f0f0;
        }
        
        .ingredient-row input {
            border: none;
            padding: 12px 10px;
            background: white;
            border-radius: 0;
            font-size: 1em;
        }
        
        .ingredient-row input:focus {
            background: #e3f2fd;
            outline: 2px solid #667eea;
        }
        
        .remove-btn {
            background: #ff6b6b;
            color: white;
            border: none;
            cursor: pointer;
            font-size: 18px;
            font-weight: bold;
            display: flex;
            align-items: center;
            justify-content: center;
            transition: background 0.3s;
        }
        
        .remove-btn:hover {
            background: #ff5252;
        }
        
        .add-ingredient-btn {
            background: linear-gradient(45deg, #4caf50, #45a049);
            color: white;
            border: none;
            padding: 15px;
            border-radius: 8px;
            cursor: pointer;
            font-size: 1em;
            font-weight: bold;
            width: 100%;
            margin-bottom: 20px;
            transition: all 0.3s ease;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        .add-ingredient-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 8px 25px rgba(76, 175, 80, 0.4);
        }
        
        .btn {
            background: linear-gradient(45deg, #667eea, #764ba2);
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 8px;
            cursor: pointer;
            font-size: 1em;
            font-weight: bold;
            transition: all 0.3s ease;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-right: 10px;
        }
        
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 8px 25px rgba(102, 126, 234, 0.4);
        }
        
        .btn-danger {
            background: linear-gradient(45deg, #ff6b6b, #ee5a5a);
        }
)rawliteral";

const char* htmlPart4 = R"rawliteral(
        .recipe-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(320px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }
        
        .recipe-card {
            background: linear-gradient(45deg, #e3f2fd, #bbdefb);
            border-radius: 15px;
            padding: 20px;
            border: 2px solid transparent;
            transition: all 0.3s ease;
            cursor: pointer;
            position: relative;
        }
        
        .recipe-card:hover {
            border-color: #667eea;
            transform: translateY(-2px);
            box-shadow: 0 8px 25px rgba(102, 126, 234, 0.3);
        }
        
        .recipe-card.expanded {
            border-color: #667eea;
            box-shadow: 0 8px 25px rgba(102, 126, 234, 0.3);
        }
        
        .recipe-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
        }
        
        .recipe-name {
            font-size: 1.3em;
            font-weight: bold;
            color: #1976d2;
            margin: 0;
        }
        
        .recipe-summary {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 15px;
        }
        
        .summary-item {
            text-align: center;
            padding: 10px;
            background: rgba(255, 255, 255, 0.7);
            border-radius: 8px;
        }
        
        .summary-value {
            font-size: 1.2em;
            font-weight: bold;
            color: #667eea;
        }
        
        .summary-label {
            font-size: 0.8em;
            color: #666;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        .recipe-details {
            display: none;
            margin-top: 20px;
            padding-top: 20px;
            border-top: 2px solid #ccc;
        }
        
        .recipe-details.show {
            display: block;
        }
        
        .ingredient-list {
            margin-bottom: 20px;
        }
        
        .ingredient-item {
            display: grid;
            grid-template-columns: 2fr 1fr 1fr;
            gap: 10px;
            padding: 10px;
            background: rgba(255, 255, 255, 0.8);
            border-radius: 5px;
            margin-bottom: 5px;
            font-size: 0.9em;
        }
        
        .ingredient-name {
            font-weight: bold;
        }
        
        .recipe-controls {
            display: flex;
            gap: 10px;
            justify-content: center;
        }
        
        .run-btn {
            background: linear-gradient(45deg, #4caf50, #45a049);
            flex: 1;
        }
        
        .stop-btn {
            background: linear-gradient(45deg, #ff6b6b, #ee5a5a);
            flex: 1;
        }
        
        .delete-btn {
            background: linear-gradient(45deg, #9e9e9e, #757575);
            padding: 8px 12px;
            font-size: 0.8em;
        }
        
        .timer-display {
            font-size: 1.5em;
            font-weight: bold;
            color: #ff6b6b;
            text-align: center;
            margin: 10px 0;
        }
        
        .running-indicator {
            position: absolute;
            top: 10px;
            right: 10px;
            width: 15px;
            height: 15px;
            background: #4caf50;
            border-radius: 50%;
            animation: pulse 1.5s infinite;
        }
        
        @keyframes pulse {
            0% { box-shadow: 0 0 0 0 rgba(76, 175, 80, 0.7); }
            70% { box-shadow: 0 0 0 10px rgba(76, 175, 80, 0); }
            100% { box-shadow: 0 0 0 0 rgba(76, 175, 80, 0); }
        }
        
        .loading {
            display: none;
            text-align: center;
            padding: 20px;
        }
        
        .spinner {
            border: 4px solid #f3f3f3;
            border-top: 4px solid #667eea;
            border-radius: 50%;
            width: 40px;
            height: 40px;
            animation: spin 1s linear infinite;
            margin: 0 auto 10px;
        }
        
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        
        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-left: 10px;
        }
        
        .status-indicator.running {
            background: #4caf50;
            animation: pulse 1.5s infinite;
        }
        
        .status-indicator.stopped {
            background: #f44336;
        }
        
        @media (max-width: 768px) {
            .container { padding: 10px; }
            .power-grid { grid-template-columns: 1fr 1fr; }
            .speed-controls { grid-template-columns: repeat(2, 1fr); }
            .table-header, .ingredient-row { grid-template-columns: 1.5fr 1fr 1fr 40px; }
            .recipe-summary { grid-template-columns: 1fr; }
            .ingredient-item { grid-template-columns: 1fr; text-align: center; }
        }
    </style>
</head>
<body>
)rawliteral";

const char* htmlPart5 = R"rawliteral(
    <div class="container">
        <div class="header">
            <h1>Recipe Controller</h1>
            <div class="hamburger" onclick="toggleMenu()">
                <span></span>
                <span></span>
                <span></span>
            </div>
        </div>
        
        <div id="homeSection">
            <div class="card">
                <h2>Power Monitoring</h2>
                <div class="power-grid">
                    <div class="power-item">
                        <div class="power-value" id="voltage">0.0</div>
                        <div class="power-label">Voltage (V)</div>
                    </div>
                    <div class="power-item">
                        <div class="power-value" id="current">0.0</div>
                        <div class="power-label">Current (A)</div>
                    </div>
                    <div class="power-item">
                        <div class="power-value" id="power">0.0</div>
                        <div class="power-label">Power (W)</div>
                    </div>
                </div>
            </div>
            
            <div class="card">
                <h2>Speed Control <span class="status-indicator" id="statusIndicator"></span></h2>
                <div class="speed-controls">
                    <div class="toggle-container">
                        <div class="toggle-label">OFF</div>
                        <label class="toggle-switch off">
                            <input type="radio" name="speed" value="0" checked onchange="setSpeed(0)">
                            <span class="slider"></span>
                        </label>
                    </div>
                    <div class="toggle-container">
                        <div class="toggle-label">Speed 1</div>
                        <label class="toggle-switch speed1">
                            <input type="radio" name="speed" value="1" onchange="setSpeed(1)">
                            <span class="slider"></span>
                        </label>
                    </div>
                    <div class="toggle-container">
                        <div class="toggle-label">Speed 2</div>
                        <label class="toggle-switch speed2">
                            <input type="radio" name="speed" value="2" onchange="setSpeed(2)">
                            <span class="slider"></span>
                        </label>
                    </div>
                    <div class="toggle-container">
                        <div class="toggle-label">Speed 3</div>
                        <label class="toggle-switch speed3">
                            <input type="radio" name="speed" value="3" onchange="setSpeed(3)">
                            <span class="slider"></span>
                        </label>
                    </div>
                </div>
            </div>
        </div>
        
        <div id="recipeSection" class="recipe-section">
            <div class="card">
                <h2>Recipe Editor</h2>
                <div class="recipe-form">
                    <div class="form-group">
                        <label for="recipeName">Recipe Name:</label>
                        <input type="text" id="recipeName" placeholder="Enter recipe name">
                    </div>
                    
                    <div class="form-group">
                        <label>Ingredients:</label>
                        <div class="ingredient-table">
                            <div class="table-header">
                                <div>Ingredient Name</div>
                                <div>Weight (g)</div>
                                <div>Calories</div>
                                <div></d
// ==================== POWER MONITORING FUNCTIONS ====================
float readVoltage() {
    long sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sum += analogRead(V_PIN);
        delayMicroseconds(100);
    }
    float avgReading = sum / (float)SAMPLES;
    float voltage = (avgReading * VREF / ADC_MAX) * V_CALIB / 1000.0;
    return voltage;
}

float readCurrent() {
    long sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        int reading = analogRead(I_PIN);
        sum += (reading - (ADC_MAX / 2)) * (reading - (ADC_MAX / 2));
        delayMicroseconds(100);
    }
    float rms = sqrt(sum / (float)SAMPLES);
    float current = (rms * VREF / ADC_MAX) * I_CALIB / 1000.0;
    return current;
}

// ==================== SPEED CONTROL FUNCTIONS ====================
void setSpeedLevel(int speed) {
    // Turn off all speeds first
    digitalWrite(SPEED1_PIN, LOW);
    digitalWrite(SPEED2_PIN, LOW);
    digitalWrite(SPEED3_PIN, LOW);
    
    // Set the requested speed
    switch (speed) {
        case 1:
            digitalWrite(SPEED1_PIN, HIGH);
            break;
        case 2:
            digitalWrite(SPEED2_PIN, HIGH);
            break;
        case 3:
            digitalWrite(SPEED3_PIN, HIGH);
            break;
        default:
            // Speed 0 - all off (already done above)
            break;
    }
    
    currentSpeed = speed;
}

// ==================== RECIPE MANAGEMENT ====================
String getRecipesJson() {
    if (!LittleFS.begin()) {
        return "[]";
    }
    
    File file = LittleFS.open("/recipes.json", "r");
    if (!file) {
        return "[]";
    }
    
    String content = file.readString();
    file.close();
    return content;
}

bool saveRecipesJson(const String& recipesJson) {
    if (!LittleFS.begin()) {
        return false;
    }
    
    File file = LittleFS.open("/recipes.json", "w");
    if (!file) {
        return false;
    }
    
    file.print(recipesJson);
    file.close();
    return true;
}

void stopCurrentRecipe() {
    recipeRunning = false;
    recipeTimer.detach();
    setSpeedLevel(0); // Turn off motor
    recipeStartTime = 0;
    recipeDuration = 0;
    recipeSpeed = 0;
    recipeRemainingTime = 0;
}

void onRecipeComplete() {
    stopCurrentRecipe();
    Serial.println("Recipe completed!");
}

// ==================== WEB SERVER HANDLERS ====================
void handleRoot() {
    String html = getCompleteHTML();
    server.send(200, "text/html", html);
}

void handleStatus() {
    DynamicJsonDocument doc(512);
    doc["voltage"] = voltage;
    doc["current"] = current;
    doc["power"] = power;
    doc["speedState"] = currentSpeed;
    doc["recipeRunning"] = recipeRunning;
    doc["currentRunningRecipe"] = recipeRunning ? recipeDuration : -1; // Use recipeDuration as recipe ID placeholder
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleToggle() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    String body = server.arg("plain");
    DynamicJsonDocument doc(256);
    
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }
    
    if (!doc.containsKey("speed")) {
        server.send(400, "text/plain", "Missing speed parameter");
        return;
    }
    
    int speed = doc["speed"];
    if (speed < 0 || speed > 3) {
        server.send(400, "text/plain", "Invalid speed value");
        return;
    }
    
    // Prevent manual speed changes during recipe execution
    if (recipeRunning) {
        server.send(409, "text/plain", "Cannot change speed while recipe is running");
        return;
    }
    
    setSpeedLevel(speed);
    server.send(200, "text/plain", "Speed set successfully");
}

void handleSaveRecipe() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    String body = server.arg("plain");
    DynamicJsonDocument newRecipe(2048);
    
    if (deserializeJson(newRecipe, body) != DeserializationError::Ok) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }
    
    // Validate required fields
    if (!newRecipe.containsKey("name") || !newRecipe.containsKey("ingredients") || 
        !newRecipe.containsKey("timer") || !newRecipe.containsKey("speedLevel")) {
        server.send(400, "text/plain", "Missing required fields");
        return;
    }
    
    // Load existing recipes
    String recipesStr = getRecipesJson();
    DynamicJsonDocument recipesDoc(8192);
    
    if (deserializeJson(recipesDoc, recipesStr) != DeserializationError::Ok) {
        recipesDoc = DynamicJsonDocument(8192);
        recipesDoc.to<JsonArray>();
    }
    
    JsonArray recipes = recipesDoc.as<JsonArray>();
    
    // Add the new recipe
    recipes.add(newRecipe);
    
    // Save back to file
    String updatedRecipes;
    serializeJson(recipesDoc, updatedRecipes);
    
    if (saveRecipesJson(updatedRecipes)) {
        server.send(200, "text/plain", "Recipe saved successfully");
    } else {
        server.send(500, "text/plain", "Failed to save recipe");
    }
}

void handleGetRecipes() {
    String recipes = getRecipesJson();
    server.send(200, "application/json", recipes);
}

void handleDeleteRecipe() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    String body = server.arg("plain");
    DynamicJsonDocument doc(256);
    
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }
    
    if (!doc.containsKey("recipeId")) {
        server.send(400, "text/plain", "Missing recipeId parameter");
        return;
    }
    
    long recipeId = doc["recipeId"];
    
    // Load existing recipes
    String recipesStr = getRecipesJson();
    DynamicJsonDocument recipesDoc(8192);
    
    if (deserializeJson(recipesDoc, recipesStr) != DeserializationError::Ok) {
        server.send(500, "text/plain", "Failed to load recipes");
        return;
    }
    
    JsonArray recipes = recipesDoc.as<JsonArray>();
    
    // Find and remove the recipe
    for (int i = recipes.size() - 1; i >= 0; i--) {
        JsonObject recipe = recipes[i];
        if (recipe["id"] == recipeId) {
            recipes.remove(i);
            break;
        }
    }
    
    // Save back to file
    String updatedRecipes;
    serializeJson(recipesDoc, updatedRecipes);
    
    if (saveRecipesJson(updatedRecipes)) {
        server.send(200, "text/plain", "Recipe deleted successfully");
    } else {
        server.send(500, "text/plain", "Failed to delete recipe");
    }
}

void handleRunRecipe() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    if (recipeRunning) {
        server.send(409, "text/plain", "Another recipe is already running");
        return;
    }
    
    String body = server.arg("plain");
    DynamicJsonDocument doc(256);
    
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }
    
    if (!doc.containsKey("recipeId")) {
        server.send(400, "text/plain", "Missing recipeId parameter");
        return;
    }
    
    long recipeId = doc["recipeId"];
    
    // Load recipes and find the one to run
    String recipesStr = getRecipesJson();
    DynamicJsonDocument recipesDoc(8192);
    
    if (deserializeJson(recipesDoc, recipesStr) != DeserializationError::Ok) {
        server.send(500, "text/plain", "Failed to load recipes");
        return;
    }
    
    JsonArray recipes = recipesDoc.as<JsonArray>();
    JsonObject recipeToRun;
    bool found = false;
    
    for (JsonObject recipe : recipes) {
        if (recipe["id"] == recipeId) {
            recipeToRun = recipe;
            found = true;
            break;
        }
    }
    
    if (!found) {
        server.send(404, "text/plain", "Recipe not found");
        return;
    }
    
    // Start the recipe
    recipeRunning = true;
    recipeStartTime = millis();
    recipeDuration = recipeId; // Store recipe ID in recipeDuration for identification
    recipeSpeed = recipeToRun["speedLevel"];
    recipeRemainingTime = recipeToRun["timer"];
    
    // Set the motor speed
    setSpeedLevel(recipeSpeed);
    
    // Set timer for recipe completion
    recipeTimer.once(recipeToRun["timer"], onRecipeComplete);
    
    Serial.println("Recipe started: " + String(recipeToRun["name"].as<String>()));
    server.send(200, "text/plain", "Recipe started successfully");
}

void handleStopRecipe() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    if (!recipeRunning) {
        server.send(409, "text/plain", "No recipe is currently running");
        return;
    }
    
    stopCurrentRecipe();
    server.send(200, "text/plain", "Recipe stopped successfully");
}

void handleGetRecipeStatus() {
    DynamicJsonDocument doc(256);
    doc["running"] = recipeRunning;
    
    if (recipeRunning && recipeStartTime > 0) {
        unsigned long elapsed = (millis() - recipeStartTime) / 1000;
        long remaining = recipeRemainingTime - elapsed;
        if (remaining < 0) remaining = 0;
        doc["remainingTime"] = remaining;
        doc["elapsedTime"] = elapsed;
        doc["totalTime"] = recipeRemainingTime;
        doc["currentSpeed"] = recipeSpeed;
    } else {
        doc["remainingTime"] = 0;
        doc["elapsedTime"] = 0;
        doc["totalTime"] = 0;
        doc["currentSpeed"] = 0;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}

// ==================== SENSOR UPDATE FUNCTION ====================
void updateSensors() {
    voltage = readVoltage();
    current = readCurrent();
    power = voltage * current;
    
    // Print to serial for debugging
    Serial.printf("V: %.1fV, I: %.2fA, P: %.1fW, Speed: %d, Recipe: %s\n", 
                  voltage, current, power, currentSpeed, recipeRunning ? "Running" : "Stopped");
}

// ==================== SETUP FUNCTION ====================
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32 Recipe Controller Starting ===");
    
    // Initialize GPIO pins
    pinMode(SPEED1_PIN, OUTPUT);
    pinMode(SPEED2_PIN, OUTPUT);
    pinMode(SPEED3_PIN, OUTPUT);
    pinMode(V_PIN, INPUT);
    pinMode(I_PIN, INPUT);
    
    // Set initial state - all speeds off
    setSpeedLevel(0);
    
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS initialization failed!");
    } else {
        Serial.println("LittleFS initialized successfully");
    }
    
    // Create WiFi Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.printf("WiFi AP started: %s\n", AP_SSID);
    Serial.printf("Password: %s\n", AP_PASSWORD);
    Serial.printf("IP address: %s\n", IP.toString().c_str());
    
    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/toggle", HTTP_POST, handleToggle);
    server.on("/saveRecipe", HTTP_POST, handleSaveRecipe);
    server.on("/getRecipes", HTTP_GET, handleGetRecipes);
    server.on("/deleteRecipe", HTTP_POST, handleDeleteRecipe);
    server.on("/runRecipe", HTTP_POST, handleRunRecipe);
    server.on("/stopRecipe", HTTP_POST, handleStopRecipe);
    server.on("/getRecipeStatus", HTTP_GET, handleGetRecipeStatus);
    server.onNotFound(handleNotFound);
    
    // Start web server
    server.begin();
    Serial.println("Web server started on port 80");
    
    // Start sensor update timer
    sensorTicker.attach_ms(UPDATE_INTERVAL, updateSensors);
    
    Serial.println("=== System Ready ===");
    Serial.println("Connect to WiFi: " + String(AP_SSID));
    Serial.println("Browse to: http://192.168.4.1");
    Serial.println("========================");
}

// ==================== MAIN LOOP ====================
void loop() {
    server.handleClient();
    
    // Update recipe remaining time if running
    if (recipeRunning && recipeStartTime > 0) {
        unsigned long elapsed = (millis() - recipeStartTime) / 1000;
        long remaining = recipeRemainingTime - elapsed;
        
        if (remaining <= 0 && recipeRunning) {
            // Recipe should have completed, but timer might have failed
            onRecipeComplete();
        }
    }
    
    delay(10); // Small delay to prevent watchdog issues
}
