#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include "EmonLib.h"
#include <Preferences.h>
#include "driver/ledc.h"

// ================== EMBEDDED WEB UI ==================
// WiFi setup page
const char wifiSetupHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WiFi Setup - Mixer Grinder</title>
<style>
body{font-family:Arial,sans-serif;background:#f8f8f8;}
.container{max-width:350px;margin:auto;margin-top:40px;background:#fff;
padding:20px;border-radius:8px;box-shadow:0 0 5px rgba(0,0,0,0.1);}
input{width:100%;padding:10px;margin:6px 0;box-sizing:border-box;}
button{padding:10px;background:#4CAF50;border:none;color:#fff;width:100%;
border-radius:4px;font-size:16px;}
h2{text-align:center;}
ul{list-style:none;padding:0;margin-top:10px;}
li{padding:8px;cursor:pointer;border-bottom:1px solid #eee;}
li:hover{background:#e1ecff;}
</style>
</head><body>
<div class="container">
<h2>WiFi Setup</h2>

<form action="/savewifi" method="POST" id="wifiForm">
  <input type="text" name="ssid" id="ssidField" placeholder="WiFi SSID" required readonly>
  <input type="password" name="pass" id="passField" placeholder="Password" required>
  <button type="submit">Save & Connect</button>
</form>

<button type="button" id="scanWifiBtn" style="margin-top:15px;width:100%;">Scan for WiFi</button>
<ul id="wifiList"></ul>

</div>
<script>
document.getElementById('scanWifiBtn').addEventListener('click', function(){
  fetch('/scanwifi')
    .then(r => r.json())
    .then(ssids => {
      var list = document.getElementById('wifiList');
      list.innerHTML = '';
      ssids.forEach(function(ssid) {
        var li = document.createElement('li');
        li.innerText = ssid;
        li.onclick = function() {
          document.getElementById('ssidField').value = ssid;
          document.getElementById('ssidField').readOnly = true;
          document.getElementById('passField').focus();
        };
        list.appendChild(li);
      });
    });
});
</script>
</body></html>
)rawliteral";
// ================== MAIN DASHBOARD PAGE ==================
// The main HTML references an external script at /app.js to avoid inline parsing issues
const char indexHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>Smart Mixer Grinder</title>
<link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;500;700&display=swap" rel="stylesheet"/>
<style>
  /* Reset and base */
  * {
    box-sizing: border-box;
  }
  body {
    margin: 0; padding: 0; background: #f8fafc;
    font-family: 'Roboto', sans-serif;
    color: #223344;
    line-height: 1.5;
  }
  header {
    background-color: #0052cc;
    color: white;
    font-weight: 700;
    font-size: 1.8rem;
    padding: 20px 0;
    text-align: center;
    letter-spacing: 1.2px;
    box-shadow: 0 4px 8px rgba(0, 82, 204, 0.3);
  }
  .menu {
    display: flex;
    background-color: #0a4da0;
    box-shadow: 0 2px 6px rgba(0, 82, 204, 0.25);
  }
  .menu button {
    flex: 1;
    background: none;
    border: none;
    color: #cbd3db;
    padding: 16px 0;
    font-size: 1rem;
    font-weight: 500;
    cursor: pointer;
    transition: color 0.3s ease, border-bottom 0.3s ease;
    border-bottom: 3px solid transparent;
  }
  .menu button:hover {
    color: white;
  }
  .menu button.active {
    color: white;
    border-bottom: 3px solid #ffd600;
  }

  main {
    max-width: 900px;
    margin: 25px auto;
    padding: 0 15px;
  }

  .section {
    display: none;
    animation: fadeIn 0.35s ease forwards;
  }
  .section.active {
    display: block;
  }
  @keyframes fadeIn {
    from {opacity: 0;}
    to {opacity: 1;}
  }

  /* Card container */
  .card {
    background: white;
    border-radius: 12px;
    box-shadow: 0 8px 20px rgba(0,0,0,0.08);
    padding: 24px;
    margin-bottom: 30px;
    transition: box-shadow 0.3s ease;
  }
  .card:hover {
    box-shadow: 0 14px 28px rgba(0,0,0,0.12);
  }
  
  /* Headings */
  h2 {
    margin-top: 0; 
    font-weight: 700; 
    color: #003a99;
  }

  /* Inputs and selects */
  input[type=text], input[type=number], select, textarea {
    width: 100%;
    font-size: 1rem;
    padding: 14px 16px;
    margin-top: 10px;
    border: 1.5px solid #cbd3db;
    border-radius: 8px;
    transition: border-color 0.3s ease;
    font-weight: 400;
    color: #223344;
  }
  input[type=text]:focus, input[type=number]:focus, select:focus, textarea:focus {
    border-color: #0052cc;
    outline: none;
  }

  /* Buttons */
  button {
    font-weight: 600;
    border-radius: 30px;
    padding: 14px 30px;
    font-size: 1rem;
    cursor: pointer;
    border: none;
    box-shadow: 0 10px 20px rgba(0, 82, 204, 0.3);
    transition: background 0.3s ease, box-shadow 0.3s ease;
  }

  button.action {
    background: linear-gradient(135deg, #0052cc, #003a99);
    color: white;
    margin-top: 20px;
  }
  button.action:hover {
    background: linear-gradient(135deg, #003a99, #002766);
    box-shadow: 0 14px 28px rgba(0, 58, 153, 0.5);
  }

  button.run-button {
    background: #ffd600;
    color: #003a99;
    padding: 8px 22px;
    border-radius: 20px;
    font-weight: 700;
    box-shadow: 0 6px 14px rgba(255, 214, 0, 0.6);
    border: 2px solid #ffaa00;
  }
  button.run-button:hover {
    background: #ffb700;
    box-shadow: 0 8px 18px rgba(255, 184, 0, 0.8);
  }

  button.delete-button {
    background: #ed2e3e;
    color: white;
    padding: 8px 20px;
    border-radius: 20px;
    font-weight: 700;
    box-shadow: 0 6px 14px rgba(237, 46, 62, 0.6);
    border: none;
    margin-left: 12px;
  }
  button.delete-button:hover {
    background: #c82333;
    box-shadow: 0 8px 18px rgba(200, 35, 51, 0.8);
  }
  
  button.upload-button {
    background: #5a9df9;
    color: white;
    padding: 8px 20px;
    border-radius: 20px;
    font-weight: 700;
    box-shadow: 0 6px 14px rgba(90, 157, 249, 0.6);
    border: none;
    margin-left: 12px;
  }
  button.upload-button:hover {
    background: #3c7ee9;
    box-shadow: 0 8px 18px rgba(60, 126, 233, 0.8);
  }

  /* Toggle style */
  .toggle {
    margin-top: 10px;
    display: flex;
    align-items: center;
  }
  .toggle input[type=checkbox] {
    width: 18px;
    height: 18px;
    margin-left: 12px;
    cursor: pointer;
  }

  /* Stats */
  .stats {
    font-size: 1.1rem;
    color: #33475b;
  }

  /* Recipe cards */
  .recipe {
    background: #ffffff;
    border-radius: 12px;
    padding: 18px 20px;
    margin-bottom: 16px;
    box-shadow: 0 3px 14px rgba(0,0,0,0.06);
    cursor: pointer;
    transition: background-color 0.25s ease;
  }
  .recipe:hover {
    background-color: #e6f0ff;
  }
  .recipe-header {
    font-weight: 700;
    font-size: 1.15rem;
    color: #004bbd;
    line-height: 1.3;
  }
  .recipe-subheader {
    font-size: 0.9rem;
    color: #66788a;
    margin-top: 4px;
  }
  .recipe-details {
    margin-top: 12px;
    padding-top: 12px;
    border-top: 1px solid #dde6f2;
    font-size: 0.95rem;
    color: #4a6785;
  }

  /* Ingredient row */
  .ingredient-row {
    display: flex;
    gap: 12px;
    margin-top: 10px;
  }
  .ingredient-row input {
    flex: 1;
    font-size: 1rem;
  }
  
</style>
</head>
<body>

<header>Smart Mixer Grinder</header>

<div class="menu" role="tablist">
  <button class="active" id="tab-home" data-target="home" role="tab" aria-selected="true" aria-controls="home">Home</button>
  <button id="tab-myrecipes" data-target="myrecipes" role="tab" aria-selected="false" aria-controls="myrecipes">My Recipes</button>
  <button id="tab-publicrecipes" data-target="publicrecipes" role="tab" aria-selected="false" aria-controls="publicrecipes">Public Recipes</button>
  <button id="tab-wifi" data-target="wifi" role="tab" aria-selected="false" aria-controls="wifi">WiFi Settings</button>
</div>

<main>
  <section id="home" class="section active" role="tabpanel" tabindex="0">
    <div class="card">
      <h2>Motor Speed</h2>
      <select id="motorSpeed" onchange="updateMotorSpeed()" aria-label="Select motor speed level">
        <option value="0">Off</option>
        <option value="1">Level 1 (30%)</option>
        <option value="2">Level 2 (60%)</option>
        <option value="3">Level 3 (100%)</option>
      </select>
      <h2>UV Sterilization</h2>
      <label class="toggle" for="uvToggle">Enable UV
        <input type="checkbox" id="uvToggle" onchange="updateUV()" aria-checked="false" />
      </label>
    </div>
    <div class="card stats" aria-live="polite" aria-atomic="true">
      <h2>Power Stats</h2>
      <p>Voltage: <strong><span id="voltage">--</span> V</strong></p>
      <p>Current: <strong><span id="current">--</span> A</strong></p>
      <p>Power: <strong><span id="power">--</span> W</strong></p>
      <p>Recipe Running: <strong><span id="recipeStatus">No</span></strong></p>
    </div>
  </section>

  <section id="myrecipes" class="section" role="tabpanel" tabindex="0" aria-hidden="true">
    <div class="card" aria-label="Create new recipe">
      <h2>Create Recipe</h2>
      <input type="text" id="recipeName" placeholder="Recipe Name" aria-label="Recipe name" />
      <div id="ingredientList" aria-live="polite" aria-relevant="additions removals">
        <div class="ingredient-row">
          <input type="text" placeholder="Ingredient Name" class="ingredientName" required aria-label="Ingredient name" />
          <input type="text" placeholder="Weight" class="ingredientWeight" required aria-label="Ingredient weight in grams" />
          <input type="text" placeholder="Calories" class="ingredientCalories" required aria-label="Calories per ingredient" />
        </div>
      </div>
      <button class="action" type="button" id="addIngredientBtn" aria-label="Add another ingredient">Add Ingredient</button>
      <input type="number" id="servingSize" placeholder="Serving Size" aria-label="Serving size" />
      <select id="recipeMotorSpeed" aria-label="Select motor speed for recipe">
        <option value="1">Level 1 (30%)</option>
        <option value="2">Level 2 (60%)</option>
        <option value="3">Level 3 (100%)</option>
      </select>
      <input type="number" id="recipeTime" placeholder="Run Time (seconds)" aria-label="Recipe run time in seconds" />
      <button class="action" id="saveRecipeBtn" aria-label="Save recipe">Save Recipe</button>
    </div>
    <div id="recipeList" aria-live="polite" aria-atomic="true" aria-label="List of saved recipes"></div>
  </section>

  <section id="publicrecipes" class="section" role="tabpanel" tabindex="0" aria-hidden="true">
    <div class="card">
      <h2>Public Recipes</h2>
      <button class="action" id="loadPublicBtn" aria-label="Load recipes from internet">Load from Internet</button>
      <div id="publicList" aria-live="polite" aria-atomic="true"></div>
    </div>
  </section>
  <section id="wifi" class="section" role="tabpanel" tabindex="0" aria-hidden="true">
  <div class="card" aria-label="WiFi Settings">
    <h2>WiFi Settings</h2>
    <button class="action" id="wifiScanBtn">Scan for WiFi</button>
    <ul id="wifiScanList" style="list-style:none;padding:0;margin-top:10px;"></ul>
    <hr>
    <div>
      <b>Forget WiFi:</b>
      <button class="delete-button" id="forgetWifiBtn">Forget & Reconnect</button>
    </div>
  </div>
</section>

</main>

<!-- Load client JS from external endpoint to avoid parsing surprises -->
<script src="/app.js"></script>

</body>
</html>
)rawliteral";

// ================== CLIENT JS (served at /app.js) ==================
const char appJS[] PROGMEM = R"rawliteral(
/* Client JS purposely avoids template literals/backticks.
   Improvements:
   - After saving a recipe, the form resets and the newly saved recipe is shown.
   - Each recipe card shows bold name, serving size and total calories.
   - Run/Delete/Upload buttons are wired to their functions.
   - Clicking a recipe toggles its details dropdown.
*/

var ws;
var publicRecipes = [];

function showSection(id) {
  var sections = document.querySelectorAll('.section');
  var buttons = document.querySelectorAll('.menu button');
  for (var i = 0; i < sections.length; i++) {
    var section = sections[i];
    if (section.id === id) {
      section.classList.add('active');
      section.setAttribute('aria-hidden', 'false');
      try { section.focus(); } catch (e) {}
    } else {
      section.classList.remove('active');
      section.setAttribute('aria-hidden', 'true');
    }
  }
  for (var j = 0; j < buttons.length; j++) {
    var button = buttons[j];
    var tgt = button.getAttribute('data-target');
    if (tgt === id) {
      button.classList.add('active');
      button.setAttribute('aria-selected', 'true');
    } else {
      button.classList.remove('active');
      button.setAttribute('aria-selected', 'false');
    }
  }
}

function initWebSocket() {
  try {
    ws = new WebSocket('ws://' + window.location.host + '/ws');
    ws.onmessage = function(evt) {
      try {
        var data = JSON.parse(evt.data);
        document.getElementById("voltage").innerText = (data.voltage || 0).toFixed(2);
        document.getElementById("current").innerText = (data.current || 0).toFixed(2);
        document.getElementById("power").innerText = (data.power || 0).toFixed(2);
        document.getElementById("motorSpeed").value = data.motorSpeed || 0;
        document.getElementById("uvToggle").checked = !!data.uvOn;
        document.getElementById("recipeStatus").innerText = data.running ? 'Yes' : 'No';
      } catch (e) {
        console.error('WS parse error', e);
      }
    };
    ws.onopen = function() { console.log('WS open'); };
    ws.onclose = function() { console.log('WS closed'); };
  } catch (e) {
    console.error('WS init failed', e);
  }
}

function updateMotorSpeed() {
  var val = parseInt(document.getElementById('motorSpeed').value);
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({motorSpeed: val}));
  }
}

function updateUV() {
  var on = document.getElementById('uvToggle').checked;
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({uvOn: on}));
  }
}

function createIngredientRow(name, weight, calories) {
  var row = document.createElement('div');
  row.className = 'ingredient-row';

  var in1 = document.createElement('input');
  in1.type = 'text';
  in1.placeholder = 'Ingredient Name';
  in1.className = 'ingredientName';
  in1.setAttribute('aria-label', 'Ingredient name');
  if (name) in1.value = name;

  var in2 = document.createElement('input');
  in2.type = 'text';
  in2.placeholder = 'Weight';
  in2.className = 'ingredientWeight';
  in2.setAttribute('aria-label', 'Ingredient weight in grams');
  if (weight) in2.value = weight;

  var in3 = document.createElement('input');
  in3.type = 'text';
  in3.placeholder = 'Calories';
  in3.className = 'ingredientCalories';
  in3.setAttribute('aria-label', 'Calories per ingredient');
  if (calories) in3.value = calories;

  var btn = document.createElement('button');
  btn.type = 'button';
  btn.innerText = '×';
  btn.style.background = '#ed2e3e';
  btn.style.color = '#fff';
  btn.style.border = 'none';
  btn.style.borderRadius = '20px';
  btn.style.padding = '0 12px';
  btn.style.cursor = 'pointer';
  btn.style.fontWeight = 'bold';
  btn.setAttribute('aria-label', 'Remove ingredient');
  btn.addEventListener('click', function() { row.remove(); });

  row.appendChild(in1);
  row.appendChild(in2);
  row.appendChild(in3);
  row.appendChild(btn);
  return row;
}

function addIngredientRow() {
  var container = document.getElementById('ingredientList');
  var row = createIngredientRow();
  container.appendChild(row);
}

function resetRecipeForm() {
  document.getElementById('recipeName').value = '';
  document.getElementById('servingSize').value = '';
  document.getElementById('recipeMotorSpeed').value = '1';
  document.getElementById('recipeTime').value = '';
  var container = document.getElementById('ingredientList');
  container.innerHTML = '';
  // add a fresh blank row
  container.appendChild(createIngredientRow());
}

function saveRecipe() {
  var btn = document.getElementById('saveRecipeBtn');
  if (btn) btn.disabled = true;
  var name = document.getElementById('recipeName').value.trim();
  if (!name) { alert('Please enter a Recipe Name'); if (btn) btn.disabled = false; return; }
  var names = document.querySelectorAll('.ingredientName');
  var weights = document.querySelectorAll('.ingredientWeight');
  var calories = document.querySelectorAll('.ingredientCalories');
  var ingredients = [];
  for (var i = 0; i < names.length; i++) {
    if (names[i].value.trim() === '') continue;
    ingredients.push({
      name: names[i].value.trim(),
      weight: weights[i].value.trim(),
      calories: calories[i].value.trim()
    });
  }
  var serving = parseInt(document.getElementById('servingSize').value) || 0;
  var speed = parseInt(document.getElementById('recipeMotorSpeed').value) || 1;
  var time = parseInt(document.getElementById('recipeTime').value) || 0;
  fetch('/api/recipes').then(function(r){ return r.json(); }).then(function(recipes){
    if (!Array.isArray(recipes)) recipes = [];
    var newRecipe = { name: name, ingredients: ingredients, serving: serving, speed: speed, time: time };
    recipes.push(newRecipe);
    fetch('/api/recipes', {
     method: 'POST',
     headers: {'Content-Type':'application/x-www-form-urlencoded'},
     body: 'plain=' + encodeURIComponent(JSON.stringify(recipes))
     }).then(function(resp){
      if (!resp.ok) throw new Error('Save failed');
      // After save success, reload recipes and reset the form. Highlight the new one.
      resetRecipeForm();
      loadRecipes(function(){ highlightSavedRecipe(newRecipe.name); });
      alert('Recipe saved!');
    });
  }).catch(function(e){ alert('Error saving recipe: ' + e); })
    .finally(function(){ if (btn) btn.disabled = false; });
}

function highlightSavedRecipe(name) {
  // Try to find the first recipe card whose header matches the name and briefly change bg
  if (!name) return;
  var headers = document.querySelectorAll('.recipe .recipe-header b');
  for (var i = 0; i < headers.length; i++) {
    if (headers[i].innerText === name) {
      var card = headers[i].closest('.recipe');
      if (card) {
        card.style.transition = 'box-shadow 0.2s ease, transform 0.2s ease';
        card.style.transform = 'scale(1.01)';
        setTimeout(function(){ card.style.transform = ''; }, 300);
      }
      break;
    }
  }
}

function loadRecipes(cb) {
  fetch('/api/recipes').then(function(r){ return r.json(); }).then(function(recipes){
    if (!Array.isArray(recipes)) recipes = [];
    var html = '';
    for (var i = 0; i < recipes.length; i++) {
      var r = recipes[i];
      var totalCal = 0;
      if (Array.isArray(r.ingredients)) {
        for (var j = 0; j < r.ingredients.length; j++) {
          totalCal += parseFloat(r.ingredients[j].calories || 0);
        }
      }
      var ingredText = '';
      if (Array.isArray(r.ingredients)) {
        for (var k = 0; k < r.ingredients.length; k++) {
          var ing = r.ingredients[k];
          ingredText += '- ' + (ing.name || '') + ' ' + (ing.weight || '') + 'g, ' + (ing.calories || '') + ' Cal';
          if (k < r.ingredients.length - 1) ingredText += '<br>';
        }
      }
      html += '<div class=\"recipe\" tabindex=\"0\" role=\"button\" aria-expanded=\"false\" data-index=\"' + i + '\">'
           + '<div class=\"recipe-header\"><b>' + (r.name || '') + '</b></div>'
           + '<div class=\"recipe-subheader\">' + totalCal.toFixed(1) + ' Cal • Serving Size: ' + (r.serving || 0) + '</div>'
           + '<div id=\"details-' + i + '\" class=\"recipe-details\" style=\"display:none;\">'
           +   '<button class=\"run-button\" data-action=\"run\" data-index=\"' + i + '\">Run</button>'
           +   '<button class=\"delete-button\" data-action=\"delete\" data-index=\"' + i + '\">Delete</button>'
           +   '<button class=\"upload-button\" data-action=\"upload\" data-index=\"' + i + '\">Upload to Public</button>'
           +   '<br><br>'
           +   '<b>Speed Level:</b> ' + (r.speed || '') + '<br>'
           +   '<b>Run Time:</b> ' + (r.time || '') + ' seconds<br>'
           +   '<b>Ingredients:</b><br>' + ingredText
           + '</div></div>';
    }
    document.getElementById('recipeList').innerHTML = html;

    // Attach handlers
    var recEls = document.querySelectorAll('.recipe');
    for (var m = 0; m < recEls.length; m++) {
      (function(el){
        el.addEventListener('click', function(){
          var idx = el.getAttribute('data-index');
          var details = document.getElementById('details-' + idx);
          if (!details) return;
          var isShown = details.style.display === 'block';
          details.style.display = isShown ? 'none' : 'block';
          el.setAttribute('aria-expanded', !isShown);
        });
        // keyboard accessibility: Enter toggles
        el.addEventListener('keydown', function(e){
          if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault();
            el.click();
          }
        });
      })(recEls[m]);
    }

    var runBtns = document.querySelectorAll('[data-action=\"run\"]');
    for (var n = 0; n < runBtns.length; n++) {
      (function(btn){
        btn.addEventListener('click', function(e){
          e.stopPropagation();
          var idx = parseInt(btn.getAttribute('data-index'));
          runRecipe(idx);
        });
      })(runBtns[n]);
    }

    var delBtns = document.querySelectorAll('[data-action=\"delete\"]');
    for (var p = 0; p < delBtns.length; p++) {
      (function(btn){
        btn.addEventListener('click', function(e){
          e.stopPropagation();
          var idx = parseInt(btn.getAttribute('data-index'));
          deleteRecipe(idx);
        });
      })(delBtns[p]);
    }

    var upBtns = document.querySelectorAll('[data-action=\"upload\"]');
    for (var q = 0; q < upBtns.length; q++) {
      (function(btn){
        btn.addEventListener('click', function(e){
          e.stopPropagation();
          var idx = parseInt(btn.getAttribute('data-index'));
          uploadPublicRecipe(idx);
        });
      })(upBtns[q]);
    }

    if (typeof cb === 'function') cb();
  }).catch(function(err){
    console.error('Failed to load recipes', err);
    document.getElementById('recipeList').innerHTML = '<div class=\"card\">No recipes or failed to load.</div>';
    if (typeof cb === 'function') cb();
  });
}

function uploadPublicRecipe(index) {
  fetch('/api/recipes').then(function(r){ return r.json(); }).then(function(recipes){
    if (!Array.isArray(recipes)) recipes = [];
    if (index >= 0 && index < recipes.length) {
      var recipe = recipes[index];
      return fetch('/api/public', {
        method: 'POST',
        headers: {'Content-Type':'application/json'},
        body: JSON.stringify(recipe)
      });
    }
  }).then(function(){ alert('Recipe uploaded to Public (stub)!'); })
    .catch(function(err){ alert('Error uploading recipe: ' + err); });
}

function deleteRecipe(index) {
  if (!confirm('Delete this recipe?')) return;
  fetch('/api/recipes').then(function(r){ return r.json(); }).then(function(recipes){
    if (!Array.isArray(recipes)) recipes = [];
    recipes.splice(index, 1);
    return fetch('/api/recipes', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify(recipes)
    });
  }).then(function(){ loadRecipes(); }).catch(function(err){ console.error(err); });
}

function runRecipe(index) {
  fetch('/api/recipes').then(function(r){ return r.json(); }).then(function(recipes){
    if (!Array.isArray(recipes)) recipes = [];
    if (index >= 0 && index < recipes.length) {
      var rec = recipes[index];
      fetch('/api/runrecipe', {
  method: 'POST',
  headers: {'Content-Type':'application/x-www-form-urlencoded'},
  body: 'plain=' + encodeURIComponent(JSON.stringify({speed: rec.speed, time: rec.time}))
}).then(function(){ alert('Recipe started! Motor will run for ' + (rec.time || 0) + ' seconds.'); });
    }
  });
}

function loadPublicRecipes() {
  fetch('/api/public').then(function(r){ return r.json(); }).then(function(data){
    if (!Array.isArray(data)) data = [];
    publicRecipes = data;
    var html = '';
    for (var i = 0; i < data.length; i++) {
      var r = data[i];
      var totalCal = 0;
      if (Array.isArray(r.ingredients)) {
        for (var j = 0; j < r.ingredients.length; j++) totalCal += parseFloat(r.ingredients[j].calories || 0);
      }
      html += '<div class=\"recipe\" data-public-index=\"' + i + '\">'
           + '<b>' + (r.name || '') + '</b> - ' + totalCal.toFixed(1) + ' Cal'
           + '<br><button class=\"run-button import-public\" data-idx=\"' + i + '\">Import</button>'
           + '</div>';
    }
    document.getElementById('publicList').innerHTML = html;
    var imBtns = document.querySelectorAll('.import-public');
    for (var k = 0; k < imBtns.length; k++) {
      (function(btn){
        btn.addEventListener('click', function(){
          var idx = parseInt(btn.getAttribute('data-idx'));
          importRecipe(publicRecipes[idx]);
        });
      })(imBtns[k]);
    }
  }).catch(function(err){
    console.error('Failed to load public recipes', err);
    document.getElementById('publicList').innerHTML = '<div class=\"card\">No public recipes available.</div>';
  });
}

function importRecipe(recipe) {
  fetch('/api/recipes').then(function(r){ return r.json(); }).then(function(recipes){
    if (!Array.isArray(recipes)) recipes = [];
    recipes.push(recipe);
    return fetch('/api/recipes', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify(recipes)
    });
  }).then(function(){ loadRecipes(); alert('Imported!'); }).catch(function(err){ console.error(err); });
}

window.onload = function() {
  var menuBtns = document.querySelectorAll('.menu button');
  for (var i = 0; i < menuBtns.length; i++) {
    (function(btn){
      btn.addEventListener('click', function(){
        var tgt = btn.getAttribute('data-target');
        if (tgt) showSection(tgt);
      });
    })(menuBtns[i]);
  }

  var addBtn = document.getElementById('addIngredientBtn');
  if (addBtn) addBtn.addEventListener('click', addIngredientRow);
  var saveBtn = document.getElementById('saveRecipeBtn');
  if (saveBtn) saveBtn.addEventListener('click', saveRecipe);
  var loadPubBtn = document.getElementById('loadPublicBtn');
  if (loadPubBtn) loadPubBtn.addEventListener('click', loadPublicRecipes);

  initWebSocket();
  loadRecipes();
};
var wifiScanBtn = document.getElementById('wifiScanBtn');
  if (wifiScanBtn) wifiScanBtn.addEventListener('click', function() {
    fetch('/scanwifi')
      .then(r => r.json())
      .then(ssids => {
        var list = document.getElementById('wifiScanList');
        list.innerHTML = '';
        ssids.forEach(function(ssid) {
          var li = document.createElement('li');
          li.innerText = ssid;
          li.style.cursor = 'pointer';
          li.style.padding = '8px';
          li.style.borderBottom = '1px solid #eee';
          li.onclick = function() {
            alert('To connect to "' + ssid + '", please go to AP mode setup page.');
          };
          list.appendChild(li);
        });
      });
  });

  var forgetWifiBtn = document.getElementById('forgetWifiBtn');
  if (forgetWifiBtn) forgetWifiBtn.addEventListener('click', function() {
    fetch('/forgetwifi', { method: 'POST' })
      .then(r => r.text())
      .then(t => alert(t + '\nDevice will reboot and enter AP setup mode.'));
  });
)rawliteral";

// ================== PINS & SETTINGS ==================
#define MOTOR_PIN 25
#define UV_PIN 26

#define PWM_FREQ 5000
#define PWM_RES 8

#define V1 34
#define I1 35
#define CV1 1000
#define CI1 1000

Preferences preferences;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
EnergyMonitor emon1;

bool isSTA = false;

uint8_t motorSpeedLevel = 0;
bool uvOn = false;
String recipesJSON = "[]";

bool recipeRunning = false;
unsigned long recipeEndTime = 0;

ledc_timer_config_t ledc_timer;
ledc_channel_config_t ledc_channel;

// ================== FUNCTIONS ==================
void setupPWM() {
  ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_timer.timer_num = LEDC_TIMER_0;
  ledc_timer.duty_resolution = (ledc_timer_bit_t)PWM_RES;
  ledc_timer.freq_hz = PWM_FREQ;
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&ledc_timer);

  ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_channel.channel = LEDC_CHANNEL_0;
  ledc_channel.timer_sel = LEDC_TIMER_0;
  ledc_channel.intr_type = LEDC_INTR_DISABLE;
  ledc_channel.gpio_num = MOTOR_PIN;
  ledc_channel.duty = 0;
  ledc_channel.hpoint = 0;
  ledc_channel_config(&ledc_channel);
}

void setPWMDuty(uint8_t duty) {
  ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, duty);
  ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
}

void setMotorSpeed(uint8_t level) {
  uint8_t duty = 0;
  if (level == 1) duty = 76;
  else if (level == 2) duty = 153;
  else if (level == 3) duty = 255;
  motorSpeedLevel = level;
  setPWMDuty(duty);
}

void setUV(bool on) {
  uvOn = on;
  digitalWrite(UV_PIN, uvOn ? HIGH : LOW);
}

String getPowerDataJSON() {
  emon1.calcVI(20, 2000);
  float voltage = emon1.Vrms;
  float current = emon1.Irms;
  float power = voltage * current;
  StaticJsonDocument<256> doc;
  doc["voltage"] = voltage;
  doc["current"] = current;
  doc["power"] = power;
  doc["motorSpeed"] = motorSpeedLevel;
  doc["uvOn"] = uvOn;
  doc["running"] = recipeRunning;
  String out;
  serializeJson(doc, out);
  return out;
}

void saveRecipesToFlash() {
  preferences.begin("recipes", false);
  preferences.putString("data", recipesJSON);
  preferences.end();
}

void loadRecipesFromFlash() {
  preferences.begin("recipes", true);
  recipesJSON = preferences.getString("data", "[]");
  preferences.end();
}

// ===== WiFi =====
String savedSSID, savedPASS;

void saveWiFiCredentials(String ssid, String pass) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();
}

bool loadWiFiCredentials() {
  preferences.begin("wifi", true);
  savedSSID = preferences.getString("ssid", "");
  savedPASS = preferences.getString("pass", "");
  preferences.end();
  return savedSSID.length() > 0;
}

bool connectToWiFi() {
  if (loadWiFiCredentials()) {
    WiFi.mode(WIFI_STA); // Explicitly set station mode
    WiFi.begin(savedSSID.c_str(), savedPASS.c_str());

    Serial.print("Connecting to WiFi");
    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) { // 30 sec timeout
      delay(500);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return true;
    } else {
      Serial.println("Failed to connect. Restarting AP mode.");
      return false; // Let setup() call startAPMode()
    }
  }
  return false;
}

// ===== Modes =====
void startAPMode() {
  WiFi.softAP("MixerGrinder_Setup", "12345678");
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", wifiSetupHTML);
  });
 server.on("/savewifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if(request->hasParam("ssid", true) && request->hasParam("pass", true)) {
      saveWiFiCredentials(request->getParam("ssid", true)->value(),
                         request->getParam("pass", true)->value());
      request->send(200, "text/html", "<html><body><h2>Saved WiFi credentials.</h2><p>Connecting…</p><p>If the connection is successful, reconnect your phone or PC to the same WiFi network and visit this device's IP address.</p></body></html>");

      delay(1500);
      ESP.restart();
    }
  });
  server.begin();
}

void setupAPIRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", indexHTML);
  });
server.on("/scanwifi", HTTP_GET, [](AsyncWebServerRequest *req){
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i) json += ",";
    json += "\"" + WiFi.SSID(i) + "\"";
  }
  json += "]";
  req->send(200, "application/json", json);
});
  // serve the external JS file
  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "application/javascript", appJS);
  });

  // serve a small blank favicon so browser won't 404 repeatedly
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(204, "image/x-icon", "");
  });

  ws.onEvent([](AsyncWebSocket *, AsyncWebSocketClient *, AwsEventType type,
                void *, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
      String msg((char*)data, len);
      StaticJsonDocument<256> doc;
      if (!deserializeJson(doc, msg)) {
        if (doc.containsKey("motorSpeed")) setMotorSpeed(doc["motorSpeed"]);
        if (doc.containsKey("uvOn")) setUV(doc["uvOn"]);
      }
    }
  });
  server.addHandler(&ws);

  server.on("/api/power", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", getPowerDataJSON());
  });

  server.on("/api/recipes", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", recipesJSON);
  });

  server.on("/api/recipes", HTTP_POST, [](AsyncWebServerRequest *req){
    if(req->hasParam("plain", true)) {
      String body = req->getParam("plain", true)->value();
      StaticJsonDocument<2048> doc;
      if (!deserializeJson(doc, body)) {
        recipesJSON = body;
        saveRecipesToFlash();
        req->send(200, "text/plain", "Saved");
      } else req->send(400, "text/plain", "Invalid JSON");
    } else {
      // If body is not available via plain param, try reading the body differently:
      if (req->hasArg("plain")) {
        String body = req->arg("plain");
        StaticJsonDocument<2048> doc;
        if (!deserializeJson(doc, body)) {
          recipesJSON = body;
          saveRecipesToFlash();
          req->send(200, "text/plain", "Saved");
          return;
        }
      }
      req->send(400, "text/plain", "Invalid JSON or missing body");
    }
  });

  server.on("/api/runrecipe", HTTP_POST, [](AsyncWebServerRequest *req){
    if(req->hasParam("plain", true)) {
      String body = req->getParam("plain", true)->value();
      StaticJsonDocument<512> doc;
      if (!deserializeJson(doc, body) && doc.containsKey("speed") && doc.containsKey("time")) {
        setMotorSpeed(doc["speed"]);
        recipeEndTime = millis() + (doc["time"].as<unsigned long>() * 1000UL);
        recipeRunning = true;
        req->send(200, "text/plain", "Running recipe");
      } else req->send(400, "text/plain", "Invalid JSON");
    } else {
      req->send(400, "text/plain", "Missing body");
    }
  });

  server.on("/api/public", HTTP_GET, [](AsyncWebServerRequest *req){
    // Stub, empty for now
    req->send(200, "application/json", "[]");
  });
server.on("/scanwifi", HTTP_GET, [](AsyncWebServerRequest *req){
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
      if (i) json += ",";
      json += "\"" + WiFi.SSID(i) + "\"";
    }
    json += "]";
    req->send(200, "application/json", json);
  });

  server.on("/forgetwifi", HTTP_POST, [](AsyncWebServerRequest *req){
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    req->send(200, "text/plain", "Forgot WiFi, restarting");
    delay(500);
    ESP.restart();
   });
   // Scan WiFi networks and return JSON list
  server.on("/scanwifi", HTTP_GET, [](AsyncWebServerRequest *req) {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
      if (i) json += ",";
      json += "\"" + WiFi.SSID(i) + "\"";
    }
    json += "]";
    req->send(200, "application/json", json);
  });
  // Forget WiFi credentials and reboot into AP mode
  server.on("/forgetwifi", HTTP_POST, [](AsyncWebServerRequest *req) {
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    req->send(200, "text/plain", "Forgot WiFi! Rebooting to setup mode...");
    delay(500);
    ESP.restart();
  });
  server.begin();
}

// ===== WebSocket notify =====
void notifyClients() {
  if(ws.count()>0){
    ws.textAll(getPowerDataJSON());
  }
}

// ===== Setup & Loop =====
void setup() {
  Serial.begin(115200);

  pinMode(UV_PIN, OUTPUT);
  setUV(false);

  setupPWM();
  setMotorSpeed(0);

  analogSetPinAttenuation(V1, ADC_11db);
  analogSetPinAttenuation(I1, ADC_11db);
  emon1.voltage(V1, CV1, 1.732);
  emon1.current(I1, CI1);

  loadRecipesFromFlash();

  if(connectToWiFi()){
    isSTA = true;
    setupAPIRoutes();
  } else {
    isSTA = false;
    startAPMode();
  }
}

void loop() {
  if(isSTA){
    static unsigned long wifiLostSince = 0;
    const unsigned long wifiLostTimeout = 30000; // 30 seconds

    if(WiFi.status() != WL_CONNECTED) {
      if(wifiLostSince == 0) wifiLostSince = millis();
      else if(millis() - wifiLostSince > wifiLostTimeout) {
        ESP.restart();
      }
    } else {
      wifiLostSince = 0;
    }
    static unsigned long lastNotify = 0;
    unsigned long now = millis();
    if(now-lastNotify > 2000){
      notifyClients();
      lastNotify = now;
    }
    // Stop the motor when recipe time expires
    if(recipeRunning && now >= recipeEndTime){
      setMotorSpeed(0);
      recipeRunning = false;
    }
    // WiFi loss fallback: restart if disconnected for >30sec 
    if(WiFi.status() != WL_CONNECTED){
      if(wifiLostSince == 0) wifiLostSince = millis();
      else if(millis() - wifiLostSince > 30000){ // 30 seconds lost
        ESP.restart(); // will reboot and retry AP/STA logic
      }
    } else {
      wifiLostSince = 0; // reset if reconnected
     }
  }
}
