/*
  ESP32 Recipe Controller - Single .ino file version
  - ESP32 acts as Access Point
  - Web UI served from memory (no FS)
  - Controls 3 speed GPIOs + off toggle (mutually exclusive)
  - Shows live voltage/current/power sensor data
  - Recipe editor with multi-ingredient support, timers, and persistence in SPIFFS (optional)
  
  Wiring:
    Speed1 GPIO 16
    Speed2 GPIO 17
    Speed3 GPIO 5
    Voltage sensor ADC GPIO 34
    Current sensor ADC GPIO 35

  Calibration constants V_CALIB, I_CALIB to be edited after calibration

  Created by ChatGPT
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "EmonLib.h"

#define WIFI_SSID     "ESP32-RecipeAP"
#define WIFI_PASSWORD "12345678"

#define SPEED1_PIN 16
#define SPEED2_PIN 17
#define SPEED3_PIN 5

#define VOLTAGE_PIN 34
#define CURRENT_PIN 35

#define V_CALIB 1000 // EDIT after calibration
#define I_CALIB 1000 // EDIT after calibration

#define SENSOR_READ_INTERVAL 1000

WebServer server(80);
EnergyMonitor emon1;

uint8_t currentSpeedState = 0; // 0=off,1=Speed1,2=Speed2,3=Speed3
unsigned long speedRunEndTime = 0;

float voltage = 0;
float current = 0;
float power = 0;

// ---- Embedded Web Assets ----
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>ESP32 Recipe Controller</title>
<style>
body{font-family:sans-serif;margin:0;padding:0;background:#f7f8fa;color:#222;}
header{background:#3f51b5;color:#fff;padding:0.5rem 1rem;display:flex;align-items:center;}
h1{margin:0;font-weight:600;font-size:1.25rem;flex-grow:1;}
button#hamburger{background:none;border:none;color:#fff;font-size:1.5rem;cursor:pointer;}
nav#side-nav{position:fixed;top:0;left:-260px;width:250px;height:100vh;background:#2c387e;color:#fff;padding:1rem;transition:left 0.3s ease;z-index:20;}
nav#side-nav.visible{left:0;}
nav#side-nav ul{list-style:none;padding:0;margin:2rem 0 0 0;}
nav#side-nav li{margin:1rem 0;}
nav#side-nav a{color:#fff;text-decoration:none;font-weight:600;}
button#close-nav{background:none;border:none;color:#fff;font-size:2rem;cursor:pointer;position:absolute;top:0.2rem;right:0.5rem;}
main{max-width:800px;margin:1rem auto;padding:0 1rem 4rem 1rem;}
section{margin-bottom:2rem;}
.toggle-group{display:flex;gap:1rem;flex-wrap:wrap;margin-bottom:1rem;}
.switch{position:relative;display:inline-block;width:70px;height:34px;user-select:none;}
.switch input{opacity:0;width:0;height:0;}
.slider{position:absolute;cursor:pointer;background:#ccc;border-radius:34px;top:0;left:0;right:0;bottom:0;transition:.4s;}
.slider:before{position:absolute;content:"";height:26px;width:26px;left:4px;bottom:4px;background:#fff;border-radius:50%;transition:.4s;}
input:checked + .slider{background:#3f51b5;}
input:checked + .slider:before{transform:translateX(36px);}
.switch{display:flex;flex-direction:column;align-items:center;font-size:.8rem;color:#333;width:auto;height:auto;}
.switch input{margin-bottom:.5rem;opacity:1;position:relative;width:auto;height:auto;cursor:pointer;}
#sensor-data p{margin:.25rem 0;font-weight:600;}
#recipe-form label,#recipe-form fieldset{display:block;margin-bottom:1rem;}
#recipe-form input,#recipe-form select{width:100%;padding:.4rem;font-size:1rem;box-sizing:border-box;}
#ingredients-table{width:100%;border-collapse:collapse;margin-bottom:.5rem;}
#ingredients-table th,#ingredients-table td{border:1px solid #ccc;padding:.3rem .5rem;text-align:center;}
#ingredients-table input{width:90%;padding:.2rem;font-size:.9rem;}
#add-ingredient{background:#3f51b5;color:#fff;border:none;padding:.5rem 1rem;cursor:pointer;font-weight:600;border-radius:4px;}
#add-ingredient:hover{background:#2c387e;}
.recipe-card{background:#fff;border-radius:6px;box-shadow:0 1px 5px rgba(0,0,0,0.15);margin-bottom:1rem;padding:1rem;cursor:pointer;}
.recipe-card.collapsed .details{display:none;}
.recipe-header{display:flex;justify-content:space-between;align-items:center;}
.recipe-header h4{margin:0;font-weight:700;}
.toggle-run{margin-left:1rem;}
.details table{width:100%;border-collapse:collapse;margin-top:.5rem;}
.details th,.details td{border:1px solid #ddd;padding:.4rem .6rem;text-align:center;}
.details th{background:#f0f0f0;}
button.delete-btn{background:#e53935;color:#fff;border:none;border-radius:4px;padding:.25rem .6rem;cursor:pointer;}
button.delete-btn:hover{background:#ab000d;}
.hidden{display:none !important;}
#loading-spinner{position:fixed;top:50%;left:50%;background:rgba(0,0,0,.6);color:#fff;padding:1rem 2rem;border-radius:8px;transform:translate(-50%,-50%);font-weight:700;font-size:1.2rem;z-index:1000;}
@media(max-width:600px){.toggle-group{flex-direction:column;gap:.75rem;}}
</style>
</head><body>
<header><button id="hamburger" aria-label="Menu">&#9776;</button><h1>ESP32 Recipe Controller</h1></header>
<nav id="side-nav" class=""><button id="close-nav" aria-label="Close Menu">&times;</button>
<ul>
<li><a href="#" id="nav-home">Home</a></li>
<li><a href="#" id="nav-recipes">Edit Recipes</a></li>
</ul>
</nav>
<main>
<section id="home-page">
<h2>Speed Controls</h2>
<div class="toggle-group" role="radiogroup" aria-label="Speed Controls">
<label class="switch">
<input type="radio" name="speedToggle" id="toggleOff" value="0" checked />
<span class="slider"></span>Off
</label>
<label class="switch">
<input type="radio" name="speedToggle" id="toggleSpeed1" value="1" />
<span class="slider"></span>Speed 1
</label>
<label class="switch">
<input type="radio" name="speedToggle" id="toggleSpeed2" value="2" />
<span class="slider"></span>Speed 2
</label>
<label class="switch">
<input type="radio" name="speedToggle" id="toggleSpeed3" value="3" />
<span class="slider"></span>Speed 3
</label>
</div>
<div id="sensor-data">
<h3>Sensor Readings</h3>
<p>Voltage: <span id="voltage">--</span> V</p>
<p>Current: <span id="current">--</span> A</p>
<p>Power: <span id="power">--</span> W</p>
</div>
</section>
<section id="recipe-page" class="hidden">
<h2>Edit Recipes</h2>
<form id="recipe-form">
<label>Recipe Name:<br /><input type="text" id="recipeName" required /></label>
<fieldset id="ingredients-fieldset">
<legend>Ingredients</legend>
<table id="ingredients-table" aria-describedby="ingredients-desc">
<thead><tr><th>Ingredient Name</th><th>Weight (g)</th><th>Calories</th><th>Remove</th></tr></thead>
<tbody id="ingredients-body"></tbody>
</table>
<p id="ingredients-desc">Add one or more ingredients with weight and calories.</p>
<button type="button" id="add-ingredient">Add Ingredient</button>
</fieldset>
<label>Serving Size:<br /><input type="number" id="servingSize" min="1" required /></label>
<label>Speed Level:<br />
<select id="speedLevel" required>
<option value="1">Speed 1</option>
<option value="2">Speed 2</option>
<option value="3">Speed 3</option>
</select>
</label>
<label>Timer (seconds):<br /><input type="number" id="timer" min="1" required /></label>
<button type="submit">Save Recipe</button>
</form>
<hr />
<section id="saved-recipes">
<h3>Saved Recipes</h3>
<div id="recipes-list"></div>
</section>
</section>
</main>
<div id="loading-spinner" class="hidden">Loading...</div>
<script>
(() => {
  // UI Elements
  const hamburger = document.getElementById('hamburger');
  const sideNav = document.getElementById('side-nav');
  const closeNav = document.getElementById('close-nav');
  const navHome = document.getElementById('nav-home');
  const navRecipes = document.getElementById('nav-recipes');
  const homePage = document.getElementById('home-page');
  const recipePage = document.getElementById('recipe-page');
  const speedToggles = document.querySelectorAll('input[name="speedToggle"]');
  const voltageSpan = document.getElementById('voltage');
  const currentSpan = document.getElementById('current');
  const powerSpan = document.getElementById('power');
  const loadingSpinner = document.getElementById('loading-spinner');
  const recipeForm = document.getElementById('recipe-form');
  const ingredientsBody = document.getElementById('ingredients-body');
  const addIngredientBtn = document.getElementById('add-ingredient');
  const recipesList = document.getElementById('recipes-list');
  const recipeNameInput = document.getElementById('recipeName');
  const servingSizeInput = document.getElementById('servingSize');
  const speedLevelSelect = document.getElementById('speedLevel');
  const timerInput = document.getElementById('timer');

  // State
  let recipes = [];
  let runningRecipeId = null;

  // Show/hide side nav
  hamburger.addEventListener('click', () => sideNav.classList.add('visible'));
  closeNav.addEventListener('click', () => sideNav.classList.remove('visible'));

  // Navigation
  navHome.addEventListener('click', e => {
    e.preventDefault();
    homePage.classList.remove('hidden');
    recipePage.classList.add('hidden');
    sideNav.classList.remove('visible');
  });
  navRecipes.addEventListener('click', e => {
    e.preventDefault();
    homePage.classList.add('hidden');
    recipePage.classList.remove('hidden');
    sideNav.classList.remove('visible');
  });

  // Fetch sensor data every second
  async function fetchStatus() {
    try {
      const res = await fetch('/status');
      const data = await res.json();
      voltageSpan.textContent = data.voltage.toFixed(2);
      currentSpan.textContent = data.current.toFixed(3);
      powerSpan.textContent = data.power.toFixed(2);
      // Update toggles to reflect current speed state if not running recipe
      if(runningRecipeId === null) {
        speedToggles.forEach(t => {
          t.checked = (parseInt(t.value) === data.speedState);
        });
      }
    } catch(e) {
      console.error('Failed to fetch status', e);
    }
  }

  setInterval(fetchStatus, 1000);
  fetchStatus();

  // Handle manual speed toggle changes (only if no recipe running)
  speedToggles.forEach(t => {
    t.addEventListener('change', async () => {
      if(runningRecipeId !== null) {
        alert('Cannot toggle speeds while a recipe is running.');
        // revert toggle to running state
        fetchStatus();
        return;
      }
      if(t.checked){
        loadingSpinner.classList.remove('hidden');
        try {
          const res = await fetch('/toggle', {
            method: 'POST',
            headers: {'Content-Type':'application/x-www-form-urlencoded'},
            body: 'speed=' + encodeURIComponent(t.value)
          });
          const data = await res.json();
          if(!data.success) alert('Failed to set speed');
        } catch(e) {
          alert('Error sending toggle');
        }
        loadingSpinner.classList.add('hidden');
      }
    });
  });

  // Ingredient row template
  function createIngredientRow(name = '', weight = '', calories = '') {
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td><input type="text" class="ing-name" required value="${name}"></td>
      <td><input type="number" class="ing-weight" min="0" required value="${weight}"></td>
      <td><input type="number" class="ing-calories" min="0" required value="${calories}"></td>
      <td><button type="button" class="remove-ingredient">X</button></td>
    `;
    tr.querySelector('.remove-ingredient').addEventListener('click', () => {
      tr.remove();
    });
    return tr;
  }

  addIngredientBtn.addEventListener('click', () => {
    ingredientsBody.appendChild(createIngredientRow());
  });

  // Initialize with one ingredient row
  ingredientsBody.appendChild(createIngredientRow());

  // Load recipes from server
  async function loadRecipes() {
    loadingSpinner.classList.remove('hidden');
    try {
      const res = await fetch('/recipes');
      recipes = await res.json();
      renderRecipes();
    } catch(e) {
      alert('Failed to load recipes');
      console.error(e);
    }
    loadingSpinner.classList.add('hidden');
  }

  // Render recipes list
  function renderRecipes() {
    recipesList.inner
HTML/JS continued (inside index_html string):

// Render recipes list (continued)
function renderRecipes() {
  recipesList.innerHTML = '';
  recipes.forEach((recipe, idx) => {
    const card = document.createElement('div');
    card.className = 'recipe-card collapsed';

    // Calculate total calories
    const totalCalories = recipe.ingredients.reduce((sum, ing) => sum + Number(ing.calories), 0);

    card.innerHTML = `
      <div class="recipe-header" tabindex="0" role="button" aria-expanded="false" aria-controls="details-${idx}">
        <h4>${recipe.name} - ${totalCalories} cal</h4>
        <label class="switch toggle-run">
          <input type="checkbox" data-idx="${idx}" ${runningRecipeId === idx ? 'checked disabled' : ''}>
          <span class="slider"></span>
        </label>
      </div>
      <div class="details" id="details-${idx}" style="display:none;">
        <table>
          <thead><tr><th>Ingredient</th><th>Weight (g)</th><th>Calories</th></tr></thead>
          <tbody>
            ${recipe.ingredients.map(ing => `
              <tr>
                <td>${ing.name}</td>
                <td>${ing.weight}</td>
                <td>${ing.calories}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>
        <p>Serving Size: ${recipe.servingSize}</p>
        <p>Speed Level: ${recipe.speedLevel}</p>
        <p>Timer: ${recipe.timer} seconds</p>
        <button class="delete-btn" data-idx="${idx}">Delete</button>
      </div>
    `;

    // Toggle expand/collapse details
    const header = card.querySelector('.recipe-header');
    const details = card.querySelector('.details');
    header.addEventListener('click', () => {
      const expanded = header.getAttribute('aria-expanded') === 'true';
      header.setAttribute('aria-expanded', !expanded);
      if(expanded){
        details.style.display = 'none';
        card.classList.add('collapsed');
      } else {
        details.style.display = 'block';
        card.classList.remove('collapsed');
      }
    });

    // Run toggle handler
    const runToggle = card.querySelector('input[type="checkbox"]');
    runToggle.addEventListener('change', async () => {
      if(runToggle.checked) {
        if(runningRecipeId !== null) {
          alert('Another recipe is running');
          runToggle.checked = false;
          return;
        }
        loadingSpinner.classList.remove('hidden');
        try {
          const res = await fetch('/toggle', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: 'recipeIndex=' + encodeURIComponent(idx)
          });
          const data = await res.json();
          if(!data.success) alert('Failed to run recipe');
          else {
            runningRecipeId = idx;
            renderRecipes();
          }
        } catch(e) {
          alert('Error running recipe');
          runToggle.checked = false;
        }
        loadingSpinner.classList.add('hidden');
      } else {
        // Disable toggle during run, so uncheck means nothing
        runToggle.checked = true;
      }
    });

    // Delete button handler
    const deleteBtn = card.querySelector('button.delete-btn');
    deleteBtn.addEventListener('click', async () => {
      if(confirm('Delete this recipe?')){
        loadingSpinner.classList.remove('hidden');
        try {
          const res = await fetch('/recipes', {
            method: 'DELETE',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: 'recipeIndex=' + encodeURIComponent(idx)
          });
          const data = await res.json();
          if(data.success) {
            recipes.splice(idx, 1);
            renderRecipes();
          } else {
            alert('Failed to delete recipe');
          }
        } catch(e) {
          alert('Error deleting recipe');
        }
        loadingSpinner.classList.add('hidden');
      }
    });

    recipesList.appendChild(card);
  });
}

// Recipe form submit
recipeForm.addEventListener('submit', async e => {
  e.preventDefault();

  const name = recipeNameInput.value.trim();
  const servingSize = Number(servingSizeInput.value);
  const speedLevel = Number(speedLevelSelect.value);
  const timer = Number(timerInput.value);

  if(!name || !servingSize || !speedLevel || !timer){
    alert('Please fill all required fields');
    return;
  }

  // Gather ingredients
  const ingredients = [];
  const rows = ingredientsBody.querySelectorAll('tr');
  for(const row of rows) {
    const nameEl = row.querySelector('.ing-name');
    const weightEl = row.querySelector('.ing-weight');
    const calEl = row.querySelector('.ing-calories');
    if(!nameEl.value.trim() || !weightEl.value || !calEl.value){
      alert('Fill all ingredient fields');
      return;
    }
    ingredients.push({
      name: nameEl.value.trim(),
      weight: Number(weightEl.value),
      calories: Number(calEl.value)
    });
  }

  const recipeData = {
    name, servingSize, speedLevel, timer, ingredients
  };

  loadingSpinner.classList.remove('hidden');
  try {
    const res = await fetch('/recipes', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(recipeData)
    });
    const data = await res.json();
    if(data.success){
      recipeForm.reset();
      ingredientsBody.innerHTML = '';
      ingredientsBody.appendChild(createIngredientRow());
      loadRecipes();
    } else {
      alert('Failed to save recipe');
    }
  } catch(e){
    alert('Error saving recipe');
  }
  loadingSpinner.classList.add('hidden');
});

// Initial load
loadRecipes();

})();
</script>
</body></html>
)rawliteral";


// ----- Helper functions and web server handlers -----

void sendNotFound(){
  server.send(404, "text/plain", "Not Found");
}

// Control the speed GPIOs ensuring only one active at a time
void setSpeed(uint8_t speed){
  digitalWrite(SPEED1_PIN, speed == 1 ? HIGH : LOW);
  digitalWrite(SPEED2_PIN, speed == 2 ? HIGH : LOW);
  digitalWrite(SPEED3_PIN, speed == 3 ? HIGH : LOW);
  currentSpeedState = speed;
  if(speed == 0) speedRunEndTime = 0; // no timer running if off
}

// Parse and handle toggle POST request
void handleToggle(){
  if(server.method() != HTTP_POST){
    server.send(405, "application/json", "{\"success\":false,\"error\":\"Only POST allowed\"}");
    return;
  }

  if(server.hasArg("speed")){
    // Manual toggle from homepage
    String speedStr = server.arg("speed");
    int spd = speedStr.toInt();
    if(spd < 0 || spd > 3){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid speed\"}");
      return;
    }
    if(speedRunEndTime != 0){ // Running recipe blocks manual toggle
      server.send(409, "application/json", "{\"success\":false,\"error\":\"Recipe running\"}");
      return;
    }
    setSpeed(spd);
    server.send(200, "application/json", "{\"success\":true,\"speed\":" + String(spd) + "}");
    return;
  }

  if(server.hasArg("recipeIndex")){
    // Run recipe command
    String idxStr = server.arg("recipeIndex");
    int idx = idxStr.toInt();
    if(idx < 0 || idx >= recipes.size()){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid recipe index\"}");
      return;
    }
    if(speedRunEndTime != 0){
      server.send(409, "application/json", "{\"success\":false,\"error\":\"Another recipe running\"}");
      return;
    }

    // Start recipe run timer
    auto &r = recipes[idx];
    setSpeed(r.speedLevel);
    speedRunEndTime = millis() + (unsigned long)(r.timer) * 1000UL;
    runningRecipeId = idx;

    server.send(200, "application/json", "{\"success\":true,\"runningRecipe\":" + String(idx) + "}");
    return;
  }

  server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
}

// Handle GET /status
void handleStatus(){
  StaticJsonDocument<256> doc;
  doc["voltage"] = voltage;
  doc["current"] = current;
  doc["power"] = power;
  doc["speedState"] = currentSpeedState;
  doc["runningRecipe"] = runningRecipeId;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// In-memory recipe storage (simple dynamic array)
struct Ingredient {
  String name;
  float weight;
  float calories;
};
struct Recipe {
  String name;
  int servingSize;
  int speedLevel;
  int timer;
  std::vector<Ingredient> ingredients;
};

std::vector<Recipe> recipes;
int runningRecipeId = -1;

// Convert JSON array of ingredients to vector
std::vector<Ingredient> jsonToIngredients(JsonArray arr){
  std::vector<Ingredient> v;
  for(JsonObject obj : arr){
    Ingredient i;
    i.name = obj["name"].as<String>();
    i.weight = obj["weight"];
    i.calories = obj["calories"];
    v.push_back(i);
  }
  return v;
}

// Convert vector to JSON array
void ingredientsToJson(JsonArray arr, const std::vector<Ingredient> &ings){
  for(const auto &i : ings){
    JsonObject obj = arr.createNestedObject();
    obj["name"] = i.name;
    obj["weight"] = i.weight;
    obj["calories"] = i.calories;
  }
}

// Handle /recipes endpoint
void handleRecipes(){
  if(server.method() == HTTP_GET){
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.to<JsonArray>();
    for(const auto &r : recipes){
      JsonObject rec = arr.createNestedObject();
      rec["name"] = r.name;
      rec["servingSize"] = r.servingSize;
      rec["speedLevel"] = r.speedLevel;
      rec["timer"] = r.timer;
      JsonArray ings = rec.createNestedArray("ingredients");
      ingredientsToJson(ings, r.ingredients);
    }
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
    return;
  }
  else if(server.method() == HTTP_POST){
    if(server.hasArg("plain") == false){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"No body\"}");
      return;
    }
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if(error){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
      return;
    }
    Recipe r;
    r.name = doc["name"].as<String>();
    r.servingSize = doc["servingSize"];
    r.speedLevel = doc["speedLevel"];
    r.timer = doc["timer"];
    r.ingredients = jsonToIngredients(doc["ingredients"].as<JsonArray>());

    recipes.push_back(r);
    server.send(200, "application/json", "{\"success\":true}");
    return;
  }
  else if(server.method() == HTTP_DELETE){
    if(server.hasArg("plain") == false){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"No body\"}");
      return;
    }
    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if(error){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
      return;
    }
    int idx = doc["recipeIndex"];
    if(idx < 0 || idx >= (int)recipes.size()){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid recipe index\"}");
      return;
    }
    recipes.erase(recipes.begin() + idx);
    server.send(200, "application/json", "{\"success\":true}");
  }
  else {
    server.send(405, "application/json", "{\"success\":false,\"error\":\"Method not allowed\"}");
  }
}

// Setup pins and start server
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting ESP32 Recipe Controller");

  pinMode(SPEED1_PIN, OUTPUT);
  pinMode(SPEED2_PIN, OUTPUT);
  pinMode(SPEED3_PIN, OUTPUT);
  setSpeed(0); // all off

  analogSetPinAttenuation(VOLTAGE_PIN, ADC_11db);
  analogSetPinAttenuation(CURRENT_PIN, ADC_11db);

  emon1.voltage(VOLTAGE_PIN, V_CALIB, 1.7);
  emon1.current(CURRENT_PIN, I_CALIB);

  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, [](){
    server.send_P(200, "text/html", index_html);
  });

  server.on("/status", HTTP_GET, handleStatus);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/recipes", HTTP_ANY, handleRecipes);
  server.onNotFound(sendNotFound);

  server.begin();
}

// Periodic sensor reading and timer management
void loop() {
  server.handleClient();

  static unsigned long lastSensorRead = 0;
  unsigned long now = millis();

  if(now - lastSensorRead >= SENSOR_READ_INTERVAL){
    lastSensorRead = now;
    emon1.calcVI(1480, 1480); // Increase sample size for better accuracy
    voltage = emon1.Vrms;
    current = emon1.Irms;
    power = voltage * current;
  }

  // Timer auto off for running recipe
  if(speedRunEndTime != 0 && now >= speedRunEndTime){
    setSpeed(0);
    speedRunEndTime = 0;
    runningRecipeId = -1;
  }
}
#include <LittleFS.h>    // Include LittleFS library

const char* RECIPES_FILE = "/recipes.json";

// Load recipes from LittleFS
void loadRecipesFromFS() {
  recipes.clear();

  if (!LittleFS.exists(RECIPES_FILE)) {
    Serial.println("No recipes file found, starting fresh.");
    return;
  }

  File file = LittleFS.open(RECIPES_FILE, "r");
  if (!file) {
    Serial.println("Failed to open recipes file");
    return;
  }

  size_t size = file.size();
  if (size == 0) {
    file.close();
    return;
  }

  std::unique_ptr<char[]> buf(new char[size + 1]);
  file.readBytes(buf.get(), size);
  buf[size] = 0;
  file.close();

  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("Failed to parse recipes JSON");
    return;
  }

  JsonArray arr = doc.as<JsonArray>();
  for (JsonObject obj : arr) {
    Recipe r;
    r.name = obj["name"].as<String>();
    r.servingSize = obj["servingSize"];
    r.speedLevel = obj["speedLevel"];
    r.timer = obj["timer"];
    r.ingredients.clear();

    for (JsonObject ing : obj["ingredients"].as<JsonArray>()) {
      Ingredient i;
      i.name = ing["name"].as<String>();
      i.weight = ing["weight"];
      i.calories = ing["calories"];
      r.ingredients.push_back(i);
    }
    recipes.push_back(r);
  }

  Serial.printf("Loaded %d recipes from LittleFS\n", (int)recipes.size());
}

// Save recipes to LittleFS
void saveRecipesToFS() {
  StaticJsonDocument<4096> doc;
  JsonArray arr = doc.to<JsonArray>();

  for (const Recipe &r : recipes) {
    JsonObject obj = arr.createNestedObject();
    obj["name"] = r.name;
    obj["servingSize"] = r.servingSize;
    obj["speedLevel"] = r.speedLevel;
    obj["timer"] = r.timer;

    JsonArray ings = obj.createNestedArray("ingredients");
    for (const Ingredient &i : r.ingredients) {
      JsonObject ing = ings.createNestedObject();
      ing["name"] = i.name;
      ing["weight"] = i.weight;
      ing["calories"] = i.calories;
    }
  }

  File file = LittleFS.open(RECIPES_FILE, "w");
  if (!file) {
    Serial.println("Failed to open recipes file for writing");
    return;
  }

  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write recipes JSON");
  }
  file.close();
  Serial.println("Recipes saved to LittleFS");
}

// Modify /recipes POST handler to save to FS
void handleRecipes(){
  if(server.method() == HTTP_GET){
    // same as before
  }
  else if(server.method() == HTTP_POST){
    if(server.hasArg("plain") == false){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"No body\"}");
      return;
    }
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if(error){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
      return;
    }
    Recipe r;
    r.name = doc["name"].as<String>();
    r.servingSize = doc["servingSize"];
    r.speedLevel = doc["speedLevel"];
    r.timer = doc["timer"];
    r.ingredients = jsonToIngredients(doc["ingredients"].as<JsonArray>());

    recipes.push_back(r);
    saveRecipesToFS();   // Save after adding
    server.send(200, "application/json", "{\"success\":true}");
    return;
  }
  else if(server.method() == HTTP_DELETE){
    if(server.hasArg("plain") == false){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"No body\"}");
      return;
    }
    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if(error){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
      return;
    }
    int idx = doc["recipeIndex"];
    if(idx < 0 || idx >= (int)recipes.size()){
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid recipe index\"}");
      return;
    }
    recipes.erase(recipes.begin() + idx);
    saveRecipesToFS();   // Save after deleting
    server.send(200, "application/json", "{\"success\":true}");
  }
  else {
    server.send(405, "application/json", "{\"success\":false,\"error\":\"Method not allowed\"}");
  }
}

void setup() {
  Serial.begin(115200);

  if(!LittleFS.begin()){
    Serial.println("Failed to mount LittleFS");
  } else {
    loadRecipesFromFS();
  }

  // rest of setup code ...
}