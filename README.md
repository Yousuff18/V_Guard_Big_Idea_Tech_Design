# ESP32 Recipe Controller with Power Monitoring

A comprehensive ESP32-based system that combines speed control, power monitoring, and recipe management through a web interface.

## 🚀 Features

- **WiFi Access Point**: Creates its own network for phone connection
- **Power Monitoring**: Real-time voltage, current, and power measurement
- **Speed Control**: 4-level speed control (OFF, Speed 1, Speed 2, Speed 3)
- **Recipe Management**: Save, edit, and run automated recipes
- **Web Interface**: Mobile-friendly responsive design
- **Persistent Storage**: Recipes stored in LittleFS flash memory
- **Auto-Timer**: Recipes run for specified duration then auto-stop

## 📋 Hardware Requirements

### ESP32 Development Board
- ESP32 Dev Module (or compatible)
- Minimum 4MB flash memory

### Power Monitoring Sensors
- **ZMPT101B AC Voltage Sensor Module**
- **SCT013 Current Transformer** (30A/1V or 100A/50mA)

### Speed Control Outputs
- 3x Relays or motor controller inputs
- Optionally: MOSFETs for PWM control

### Additional Components
- 2x 33Ω burden resistors (for current sensing)
- 4x 10kΩ resistors (for voltage dividers)
- 2x 1kΩ resistors (for ADC protection)
- Capacitors: 2x 10µF, 2x 100nF (for filtering)
- Breadboard or PCB for connections

## 🔌 Wiring Diagram

### Power Monitoring Connections

```
ZMPT101B Voltage Sensor:
┌─────────────┐
│ ZMPT101B    │
│ VCC ──────── 3.3V
│ GND ──────── GND
│ OUT ──────── [Voltage Divider] ── GPIO 34
└─────────────┘

Voltage Divider for ZMPT101B:
ZMPT101B OUT ── 1kΩ ── GPIO 34
                 │
               10kΩ ── 1.65V (bias)
                 │
               10kΩ ── GND
```

```
SCT013 Current Transformer:
┌─────────────┐
│ SCT013      │
│ Terminal 1 ── [33Ω Burden] ── [Voltage Divider] ── GPIO 35
│ Terminal 2 ── [33Ω Burden] ── GND
└─────────────┘

Voltage Divider for SCT013:
CT Output ── 1kΩ ── GPIO 35
              │
            10kΩ ── 1.65V (bias)
              │
            10kΩ ── GND
```

### Speed Control Connections

```
GPIO Connections:
GPIO 16 (SPEED1_PIN) ── Relay 1 or Motor Controller Input 1
GPIO 17 (SPEED2_PIN) ── Relay 2 or Motor Controller Input 2
GPIO 5  (SPEED3_PIN) ── Relay 3 or Motor Controller Input 3

Note: Add flyback diodes if driving relays directly
```

## 💾 Software Installation

### 1. Arduino IDE Setup

```bash
# Install ESP32 board support
File -> Preferences -> Additional Boards Manager URLs:
https://dl.espressif.com/dl/package_esp32_index.json

Tools -> Board Manager -> Search "ESP32" -> Install
```

### 2. Required Libraries

Install these libraries through Arduino IDE Library Manager:

```bash
- ArduinoJson (v6.21.0 or later)
- EmonLib (for calibration sketch)
```

### 3. Upload Process

1. **Upload Main Sketch**:
   - Open the main `.ino` file
   - Select Board: "ESP32 Dev Module"
   - Select correct COM port
   - Upload sketch

2. **No LittleFS Upload Needed**: 
   - Web files are embedded in the sketch
   - LittleFS will auto-format on first boot

### 4. Configuration

Edit these constants in the main sketch as needed:

```cpp
// WiFi Settings
#define AP_SSID "ESP32-RecipeAP"        // Change WiFi name
#define AP_PASSWORD "recipe123"         // Change password

// GPIO Pin Assignments (change if needed)
#define SPEED1_PIN 16    // Speed 1 output
#define SPEED2_PIN 17    // Speed 2 output  
#define SPEED3_PIN 5     // Speed 3 output
#define V_PIN 34         // Voltage sensor
#define I_PIN 35         // Current sensor

// Calibration Constants (replace after calibration)
#define V_CALIB 1000     // Voltage calibration factor
#define I_CALIB 1000     // Current calibration factor
```

## 🔧 Calibration Process

### 1. Upload Calibration Sketch

1. Create folder: `YourSketch/calibration/`
2. Save calibration sketch as `calibration.ino`
3. Upload and run calibration sketch

### 2. Perform Calibration

1. **Setup Test Load**:
   - Use purely resistive load (incandescent bulb, heater)
   - Avoid inductive loads (motors, transformers)

2. **Measure with Multimeter**:
   - Connect true-RMS multimeter
   - Record actual voltage and current

3. **Calculate Calibration Constants**:
   ```
   New CV1 = (Multimeter_Voltage × 1000) ÷ ESP32_Voltage
   New CI1 = (Multimeter_Current × 1000) ÷ ESP32_Current
   ```

4. **Example Calculation**:
   ```
   Multimeter reads: 230V, 2.5A
   ESP32 reads: 180V, 3.2A
   
   New CV1 = (230 × 1000) ÷ 180 = 1278
   New CI1 = (2.5 × 1000) ÷ 3.2 = 781
   ```

5. **Update Main Sketch**:
   - Replace `V_CALIB 1000` with `V_CALIB 1278`
   - Replace `I_CALIB 1000` with `I_CALIB 781`
   - Re-upload main sketch

## 🌐 Web Interface Usage

### 1. Connect to ESP32

1. **WiFi Connection**:
   - Connect phone/computer to "ESP32-RecipeAP"
   - Password: "recipe123" (or your custom password)

2. **Open Web Interface**:
   - Browse to: `http://192.168.4.1`
   - Bookmark for easy access

### 2. Home Page Features

**Power Monitoring Display**:
- Real-time voltage, current, and power
- Updates every second
- Color-coded status indicator

**Speed Control Toggles**:
- OFF, Speed 1, Speed 2, Speed 3 buttons
- Only one can be active at a time
- Smooth visual transitions

### 3. Recipe Editor (Hamburger Menu)

**Create Recipe**:
- Recipe Name: Descriptive name
- Ingredient Name: What you're processing
- Weight: In grams
- Calories: Nutritional information
- Serving Size: Number of servings
- Speed Level: 1, 2, or 3
- Timer: Duration in seconds

**Manage Recipes**:
- Tap recipe card to expand details
- Run button starts automatic sequence
- Delete button removes recipe
- All recipes persist after reboot

## 🔌 API Endpoints

The ESP32 provides a REST API for integration:

### GET /status
Returns current system status
```json
{
  "voltage": 230.5,
  "current": 2.34,
  "power": 539.57,
  "speedState": 2,
  "recipeRunning": false
}
```

### POST /toggle
Set speed level
```json
Request: {"speed": 2}
Response: {"success": true}
```

### GET /recipes
Get all saved recipes
```json
[
  {
    "name": "Smoothie Mix",
    "ingredient": "Fruits",
    "weight": 500,
    "calories": 200,
    "servingSize": 2,
    "speedLevel": 3,
    "timer": 60
  }
]
```

### POST /recipes
Save new recipe
```json
Request: {
  "name": "Protein Shake",
  "ingredient": "Whey Powder",
  "weight": 30,
  "calories": 120,
  "servingSize": 1,
  "speedLevel": 2,
  "timer": 45
}
```

### DELETE /recipes?index=0
Delete recipe by index

### POST /run-recipe
Execute recipe
```json
Request: {"speedLevel": 3, "timer": 60}
Response: {"success": true}
```

## ⚠️ Safety Considerations

### Electrical Safety
- Use appropriate fuses/breakers
- Ensure proper grounding
- Double-check all high-voltage connections
- Test with low power loads first

### Code Safety Features
- Only one speed active at a time
- Recipe conflicts prevented
- Auto-stop when timer expires
- GPIO state validation

### Operational Safety
- Monitor first few recipe runs
- Start with short timer durations
- Keep emergency stop accessible
- Regular calibration checks

## 🛠️ Troubleshooting

### Power Monitoring Issues

**Voltage Reading Zero**:
- Check ZMPT101B power (3.3V, GND)
- Verify voltage divider circuit
- Test with multimeter on GPIO 34
- Expected: ~1.65V DC + AC fluctuation

**Current Reading Zero**:
- Check SCT013 connection
- Verify 33Ω burden resistor
- Ensure current flows through CT primary
- Test voltage across burden resistor

**Unstable Readings**:
- Add filtering capacitors (10µF + 100nF)
- Check for loose connections
- Move away from interference sources
- Consider ADS1115 for better precision

### WiFi/Connection Issues

**Cannot Connect to AP**:
- Check SSID and password in code
- Reset ESP32 and retry
- Look for "ESP32-RecipeAP" in WiFi list
- Try different device (phone, laptop)

**Web Page Won't Load**:
- Confirm IP address: 192.168.4.1
- Clear browser cache
- Try incognito/private mode
- Check ESP32 serial monitor for errors

### Recipe/Control Issues

**Speeds Won't Change**:
- Verify GPIO pin assignments
- Check relay connections
- Monitor serial output for errors
- Test individual GPIO pins

**Recipes Won't Save**:
- Check LittleFS mounting (serial monitor)
- Verify JSON format in browser console
- Try formatting LittleFS (will erase all recipes)
- Ensure sufficient flash memory

**Timer Not Working**:
- Check recipe duration values
- Monitor serial output during recipe run
- Verify system doesn't reset during operation
- Test with short durations first

## 🔄 Advanced Modifications

### PWM Speed Control

To use PWM instead of simple ON/OFF:

```cpp
// Replace digitalWrite with analogWrite
void setSpeedLevel(int speed) {
  analogWrite(SPEED1_PIN, 0);
  analogWrite(SPEED2_PIN, 0);
  analogWrite(SPEED3_PIN, 0);
  
  switch (speed) {
    case 1: analogWrite(SPEED1_PIN, 85); break;   // 33% duty
    case 2: analogWrite(SPEED2_PIN, 170); break;  // 66% duty
    case 3: analogWrite(SPEED3_PIN, 255); break;  // 100% duty
  }
}
```

### Higher Precision ADC

For better accuracy, use ADS1115:

```cpp
#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;

// In setup():
ads.begin();
ads.setGain(GAIN_ONE);  // ±4.096V range

// In readSensors():
int16_t vRaw = ads.readADC_SingleEnded(0);  // A0
int16_t iRaw = ads.readADC_SingleEnded(1);  // A1
```

### Remote Monitoring

Add MQTT or HTTP posting:

```cpp
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient mqtt(espClient);

// Publish power data
void publishPowerData() {
  String payload = "{\"V\":" + String(voltage) + 
                  ",\"I\":" + String(current) + 
                  ",\"P\":" + String(power) + "}";
  mqtt.publish("esp32/power", payload.c_str());
}
```

## 📁 Project Structure

```
ESP32_Recipe_Controller/
├── ESP32_Recipe_Controller.ino     # Main sketch
├── calibration/
│   └── calibration.ino            # Calibration sketch
├── README.md                      # This documentation
└── circuit_diagrams/              # Wiring diagrams (optional)
    ├── power_monitoring.png
    └── speed_control.png
```

## 📖 Additional Resources

- [ESP32 Official Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Arduino ESP32 Core](https://github.com/espressif/arduino-esp32)
- [LittleFS Documentation](https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html)
- [ZMPT101B Datasheet](https://components101.com/sensors/zmpt101b-voltage-sensor)
- [SCT013 Current Transformer Guide](https://openenergymonitor.org/emon/buildingblocks)

## 🤝 Support

For issues and questions:

1. Check troubleshooting section above
2. Review serial monitor output for error messages
3. Verify all hardware connections
4. Test individual components separately
5. Check for library version compatibility

## 📄 License

This project is provided as-is for educational and personal use. Please ensure compliance with local electrical codes and safety regulations when implementing.

---

**⚡ Happy Building! ⚡**