# ESP32 Recipe Manager with Power Monitoring

A complete ESP32-based recipe management system with speed control, power monitoring, and web interface. The ESP32 creates its own WiFi access point for direct connection from mobile devices.

## 🚀 Features

- **WiFi Access Point**: ESP32 creates its own hotspot for direct connection
- **Speed Control**: 4 toggle switches (OFF, Speed 1, Speed 2, Speed 3) with exclusive selection
- **Power Monitoring**: Real-time voltage, current, and power measurement using ZMPT101B and SCT013 sensors
- **Recipe Management**: Add, edit, delete recipes with multiple ingredients, weights, and calories
- **Timer Control**: Automatic speed control based on recipe timer settings
- **Mobile-Responsive UI**: Clean, modern interface optimized for smartphone use
- **Persistent Storage**: Recipes stored in LittleFS, survive reboots
- **Real-time Updates**: Live sensor data and status updates every second

## 📋 Hardware Requirements

### Core Components
- **ESP32 Dev Module** (any variant with WiFi)
- **ZMPT101B AC Voltage Sensor Module**
- **SCT013 Current Transformer** (30A/1V or 100A/50mA)
- **3x Relay Modules** or motor controller digital inputs
- **Breadboard and jumper wires**

### Safety Components (CRITICAL)
- **Voltage Divider Resistors** (for ZMPT101B: 2x 10kΩ resistors)
- **Burden Resistor** (for SCT013: 47Ω, 1W resistor)
- **Bias Voltage Circuit** (2x 10kΩ resistors for 1.65V reference)
- **Isolation** (Proper AC mains isolation - USE EXTREME CAUTION)

⚠️ **WARNING**: This project involves AC mains voltage monitoring. Improper wiring can cause electrocution, fire, or death. Only proceed if you have proper electrical safety knowledge.

## 🔌 Wiring Diagram

### GPIO Pin Assignments
```
ESP32 Pin    | Component           | Notes
-------------|--------------------|---------------------------------
GPIO 34      | ZMPT101B Output    | AC Voltage sensor (with divider)
GPIO 35      | SCT013 Output      | Current sensor (with burden resistor)
GPIO 16      | Speed 1 Relay      | Control relay or motor input
GPIO 17      | Speed 2 Relay      | Control relay or motor input
GPIO 5       | Speed 3 Relay      | Control relay or motor input
3.3V         | Sensor Power       | Power for sensor modules
GND          | Common Ground      | All grounds connected
```

### ZMPT101B Connection
```
ZMPT101B    ESP32    Additional Components
VCC     →   3.3V     
GND     →   GND      
OUT     →   GPIO34   Via voltage divider (10kΩ + 10kΩ) + 1.65V bias
```

### SCT013 Connection  
```
SCT013      ESP32    Additional Components
Signal  →   GPIO35   Via 47Ω burden resistor + 1.65V bias
GND     →   GND      
```

### Speed Control Relays
```
Relay Module    ESP32    Load Connection
VCC         →   5V       (or 3.3V depending on module)
GND         →   GND      
IN1         →   GPIO16   Speed 1 control
IN2         →   GPIO17   Speed 2 control  
IN3         →   GPIO5    Speed 3 control
COM/NO      →   Motor    Connect your motor/device here
```

## 🛠️ Installation Guide

### 1. Arduino IDE Setup

1. **Install ESP32 Board Package**:
   - File → Preferences
   - Add to Additional Board Manager URLs: `https://dl.espressif.com/dl/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install

2. **Select Board**:
   - Tools → Board → ESP32 Arduino → ESP32 Dev Module

3. **Configure Upload Settings**:
   ```
   Board: ESP32 Dev Module
   Upload Speed: 921600
   CPU Frequency: 240MHz (WiFi/BT)
   Flash Frequency: 80MHz
   Flash Mode: QIO
   Flash Size: 4MB (32Mb)
   Partition Scheme: Default 4MB with spiffs
   Core Debug Level: None
   ```

### 2. Required Libraries

Install these libraries via **Tools → Manage Libraries**:

1. **ArduinoJson** by Benoit Blanchon
   - Search: "ArduinoJson"
   - Install the latest version

2. **EmonLib** (for calibration only)
   - Search: "EmonLib" 
   - Install for calibration sketch

### 3. Code Installation

1. **Create Project Folder**:
   ```
   ESP32_Recipe_Manager/
   ├── ESP32_Recipe_Manager.ino    (main code)
   └── calibration/
       └── ESP32_Calibration.ino   (calibration code)
   ```

2. **Upload Main Code**:
   - Copy the main ESP32_Recipe_Manager.ino code
   - Upload to ESP32
   - Open Serial Monitor (115200 baud) to see connection details

## ⚡ Sensor Calibration (ESSENTIAL)

### Why Calibration is Needed
The default calibration values (V_CALIB = 1000, I_CALIB = 1000) are generic. For accurate readings, you must calibrate with known loads.

### Calibration Process

1. **Hardware Setup**:
   - Wire sensors as shown in wiring diagram
   - Connect a **purely resistive load** (electric heater, kettle, incandescent bulb)
   - Have a **multimeter** ready for reference measurements

2. **Upload Calibration Sketch**:
   - Open `calibration/ESP32_Calibration.ino`
   - Upload to ESP32
   - Open Serial Monitor (115200 baud)

3. **Take Measurements**:
   - Turn on your resistive load
   - Note ESP32 readings: `Vrms: XXX V | Irms: XXX A`
   - Measure actual values with multimeter
   - Record both sets of values

4. **Calculate Calibration Constants**:
   ```
   New CV1 = (Vr × CVold) ÷ Vesp
   New CI1 = (Ir × CIold) ÷ Iesp
   ```
   
   Where:
   - `Vr` = Real voltage (multimeter)
   - `Ir` = Real current (multimeter)
   - `Vesp` = ESP32 displayed voltage
   - `Iesp` = ESP32 displayed current
   - `CVold` = 1000 (starting value)
   - `CIold` = 1000 (starting value)

5. **Example Calculation**:
   ```
   Multimeter: 230V, 2.5A
   ESP32 shows: 200V, 2.2A
   
   New CV1 = (230 × 1000) ÷ 200 = 1150
   New CI1 = (2.5 × 1000) ÷ 2.2 = 1136
   ```

6. **Update Main Code**:
   - Open main ESP32_Recipe_Manager.ino
   - Replace calibration values:
   ```cpp
   #define V_CALIB 1150  // Your calculated CV1
   #define I_CALIB 1136  // Your calculated CI1
   ```
   - Re-upload main code

## 📱 Usage Guide

### First Time Setup

1. **Power On ESP32**: Connect power via USB or external supply

2. **Connect to WiFi**:
   - Look for network: **ESP32-RecipeAP**
   - Password: **Recipe123456**
   - Wait for connection confirmation

3. **Open Web Interface**:
   - Open browser on connected device
   - Go to: **http://192.168.4.1**
   - Interface should load immediately

### Main Interface

#### Home Screen
- **Sensor Readings**: Real-time voltage, current, power display
- **Speed Controls**: 4 toggle switches (OFF, Speed 1, Speed 2, Speed 3)
- **Timer Display**: Shows when recipe is running with countdown
- **Hamburger Menu**: Access recipe management (☰ button)

#### Speed Control
- Only one speed can be active at a time
- Toggles are disabled during recipe execution
- Manual control available when no recipe running

### Recipe Management

#### Adding New Recipe

1. **Access Recipe Page**: Tap hamburger menu (☰) → Recipe editor loads
2. **Enter Recipe Details**:
   - **Recipe Name**: Descriptive name for your recipe
   - **Ingredients Table**: 
     - Ingredient name (text)
     - Weight in grams (number)
     - Calories per ingredient (number)
   - **Add More Ingredients**: Use "+ Add Ingredient" button
   - **Serving Size**: Number of servings this recipe makes
   - **Speed Level**: Choose 1, 2, or 3 for motor speed
   - **Timer**: Duration in seconds for automatic operation

3. **Save Recipe**: Tap "Save Recipe" button

#### Managing Saved Recipes

- **View Recipes**: Scroll to "Saved Recipes" section
- **Expand Details**: Tap recipe header to see full details
- **Run Recipe**: Tap "Run Recipe" to start automatic operation
- **Delete Recipe**: Tap "Delete" and confirm

#### Running Recipes

When a recipe runs:
- Selected speed activates automatically
- Timer counts down in real-time
- Speed controls are disabled
- Motor stops automatically when timer expires
- Only one recipe can run at a time

## 🔧 Configuration Options

### Network Settings
```cpp
// Change these in the main code
const char* AP_SSID = "ESP32-RecipeAP";           // Your preferred network name
const char* AP_PASSWORD = "Recipe123456";          // Your preferred password (min 8 chars)
```

### GPIO Pin Mapping
```cpp
// Modify if using different pins
#define SPEED1_PIN 16                               // Speed 1 relay control pin
#define SPEED2_PIN 17                               // Speed 2 relay control pin  
#define SPEED3_PIN 5                                // Speed 3 relay control pin
#define VOLTAGE_PIN 34                              // ZMPT101B voltage sensor pin
#define CURRENT_PIN 35                              // SCT013 current sensor pin
```

### Sensor Update Rate
```cpp
#define SENSOR_UPDATE_INTERVAL 1000                 // Update interval in milliseconds
#define ADC_SAMPLES 1000                            // ADC samples for RMS calculation
```

## 🐛 Troubleshooting

### Connection Issues

**Can't see ESP32-RecipeAP network:**
- Check ESP32 power and code upload
- Look in 2.4GHz WiFi networks only
- Reset ESP32 and wait 30 seconds

**Connected but can't access webpage:**
- Verify connection to ESP32-RecipeAP (not home WiFi)
- Try http://192.168.4.1 (not https)
- Clear browser cache
- Try different browser

### Sensor Reading Issues

**Voltage/Current readings are zero:**
- Check sensor wiring and power
- Verify bias voltage circuit (should be 1.65V)
- Ensure proper grounding
- Run calibration sketch to test sensors

**Readings are inaccurate:**
- **MUST run calibration process** with known loads
- Update V_CALIB and I_CALIB values
- Check for loose connections
- Verify burden resistor value (47Ω for SCT013)

**Erratic readings:**
- Add filter capacitors (100nF ceramic + 10µF electrolytic)
- Improve grounding
- Separate analog and digital grounds if possible
- Increase ADC_SAMPLES value for more stable readings

### Speed Control Issues

**Relays not switching:**
- Check relay module power supply
- Verify GPIO connections
- Test with multimeter for 3.3V output
- Some relays need 5V - use level shifter or 5V-tolerant modules

**Multiple speeds active:**
- Software prevents this - check for hardware cross-wiring
- Reset ESP32 to clear any stuck states

### Recipe Management Issues

**Recipes not saving:**
- Check LittleFS initialization in serial monitor
- Ensure sufficient flash memory
- Try simpler recipe names (avoid special characters)

**Timer not working:**
- Verify recipe has non-zero timer value
- Check serial monitor for debug messages
- Manual speed control should work if timer fails

### Serial Monitor Debug

Enable detailed debugging:
```cpp
// Add to main code for more debug info
#define DEBUG_MODE 1
```

Common serial messages:
- `LittleFS mounted successfully` - File system OK
- `HTTP server started` - Web server running
- `V: XXX V, I: XXX A, P: XXX W` - Sensor readings
- `Speed X ON/OFF` - Relay status changes
- `Starting recipe: XXX` - Recipe execution begins

## ⚠️ Safety Warnings

### Electrical Safety
- **AC Mains Voltage**: This project monitors dangerous voltages
- **Use Proper Isolation**: Never connect ESP32 directly to mains
- **GFCI Protection**: Use ground fault circuit interrupter
- **Professional Review**: Have qualified electrician review connections
- **Testing**: Use isolation transformer for initial testing

### Fire Safety  
- **Proper Ventilation**: Ensure adequate airflow around components
- **Heat Management**: Monitor component temperatures
- **Fuse Protection**: Use appropriate fuses in all circuits
- **Emergency Shutoff**: Have manual disconnect readily available

### Personal Safety
- **Turn Off Power**: Always disconnect power when wiring
- **Use PPE**: Wear safety glasses and insulated gloves
- **Work Alone**: Don't work on live circuits alone
- **Know Your Limits**: If unsure, consult professionals

## 📊 Technical Specifications

### Power Requirements
- **ESP32**: 250mA @ 3.3V (active), 20µA (deep sleep)
- **Sensors**: 50mA @ 3.3V combined
- **Relays**: Varies by type (typically 50-200mA @ 5V each)
- **Total**: ~500mA @ 5V recommended supply

### Performance
- **WiFi Range**: ~50m indoor, ~100m outdoor (typical)
- **Sensor Update Rate**: 1Hz (configurable)
- **ADC Resolution**: 12-bit (4096 levels)
- **Current Accuracy**: ±2% after calibration
- **Voltage Accuracy**: ±1% after calibration

### Memory Usage
- **Flash**: ~500KB program + web assets
- **RAM**: ~50KB runtime usage
- **SPIFFS**: ~100KB for recipe storage

## 🔮 Future Enhancements

### Planned Features
- **Data Logging**: Historical power consumption graphs
- **Multiple Timers**: Support for multi-stage recipes  
- **WiFi Client Mode**: Connect to existing network
- **MQTT Integration**: IoT platform connectivity
- **Voice Control**: Integration with smart assistants
- **Mobile App**: Native Android/iOS application

### Hardware Additions
- **LCD Display**: Local status display
- **Rotary Encoder**: Manual speed control
- **Temperature Sensors**: Monitor motor/ambient temperature
- **SD Card**: Extended recipe storage
- **Real-Time Clock**: Scheduled recipe execution

## 📝 License

This project is open-source and available under the MIT License. Feel free to modify, distribute, and use for commercial purposes.

## 🤝 Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create feature branch
3. Test thoroughly
4. Submit pull request with detailed description

## 📞 Support

For technical support:
- Check troubleshooting section first
- Review serial monitor output
- Create detailed issue report with:
  - Hardware configuration
  - Code version
  - Error messages
  - Steps to reproduce

***

**⚡ Remember**: This project involves potentially dangerous voltages. Always prioritize safety and consult professionals when in doubt. Happy making! 🚀

