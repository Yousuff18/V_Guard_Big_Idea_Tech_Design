/*
 * ESP32 Power Monitoring Calibration Sketch
 * 
 * CALIBRATION PROCEDURE:
 * ======================
 * 1. Install EmonLib library: Tools -> Manage Libraries -> Search "EmonLib" -> Install
 * 2. Wire up ZMPT101B and SCT013 sensors (see main sketch for wiring details)
 * 3. Connect a purely resistive load (incandescent bulb, heater, etc.)
 * 4. Measure actual voltage and current with a multimeter
 * 5. Upload and run this sketch
 * 6. Note the ESP32 readings (Vesp and Iesp)
 * 7. Calculate calibration constants:
 *    - New CV1 = (Vmultimeter * 1000) / Vesp32
 *    - New CI1 = (Imultimeter * 1000) / Iesp32
 * 8. Replace V_CALIB and I_CALIB in main sketch with new values
 * 
 * EXAMPLE:
 * ========
 * If multimeter reads 230V and ESP32 shows 180V:
 * New CV1 = (230 * 1000) / 180 = 1278
 * 
 * If multimeter reads 2.5A and ESP32 shows 3.2A:
 * New CI1 = (2.5 * 1000) / 3.2 = 781
 */

#include "EmonLib.h"

// Pin definitions (must match main sketch)
#define V1 34        // ZMPT101B voltage sensor pin
#define I1 35        // SCT013 current sensor pin

// Initial calibration constants (will be adjusted)
#define CV1 1000     // Voltage calibration constant
#define CI1 1000     // Current calibration constant

// Create EnergyMonitor instance
EnergyMonitor emon1;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 Power Monitoring Calibration ===");
  Serial.println("Connect a purely resistive load and measure with multimeter");
  Serial.println("Compare readings below with your multimeter");
  Serial.println("===============================================\n");
  
  // Configure ADC pins
  analogSetPinAttenuation(V1, ADC_11db);  // 0-3.3V range
  analogSetPinAttenuation(I1, ADC_11db);  // 0-3.3V range
  
  // Initialize energy monitor
  // Parameters: pin, calibration, phase_shift
  emon1.voltage(V1, CV1, 1.732);  // 1.732 is phase shift for voltage
  emon1.current(I1, CI1);         // Current: pin, calibration
  
  delay(2000);  // Wait for stabilization
}

void loop() {
  // Calculate voltage and current
  // Parameters: number_of_half_wavelengths, timeout_ms
  emon1.calcVI(20, 2000);  // 20 half-wavelengths, 2 second timeout
  
  // Display readings
  Serial.print("ESP32 Readings -> ");
  Serial.print("Voltage: ");
  Serial.print(emon1.Vrms, 2);
  Serial.print(" V | Current: ");
  Serial.print(emon1.Irms, 3);
  Serial.print(" A | Power: ");
  Serial.print(emon1.apparentPower, 2);
  Serial.print(" VA | Real Power: ");
  Serial.print(emon1.realPower, 2);
  Serial.println(" W");
  
  // Show raw ADC values for debugging
  int vRaw = analogRead(V1);
  int iRaw = analogRead(I1);
  Serial.print("Raw ADC Values -> Voltage: ");
  Serial.print(vRaw);
  Serial.print(" | Current: ");
  Serial.println(iRaw);
  
  Serial.println("---");
  
  delay(2000);  // Update every 2 seconds
}

/*
 * CALIBRATION CALCULATION EXAMPLES:
 * ==================================
 * 
 * VOLTAGE CALIBRATION:
 * If your multimeter reads 240V and ESP32 shows 190V:
 * New CV1 = (240 * 1000) / 190 = 1263
 * 
 * CURRENT CALIBRATION:
 * If your multimeter reads 1.5A and ESP32 shows 2.1A:
 * New CI1 = (1.5 * 1000) / 2.1 = 714
 * 
 * TROUBLESHOOTING:
 * ================
 * - If voltage reading is 0: Check ZMPT101B wiring and power
 * - If current reading is 0: Check SCT013 connection and load
 * - If readings are unstable: Add more filtering capacitors
 * - If readings are way off: Check burden resistor values
 * 
 * EXPECTED RAW ADC VALUES:
 * ========================
 * - Voltage pin (no load): ~2048 (mid-rail bias)
 * - Current pin (no load): ~2048 (mid-rail bias)
 * - Values should fluctuate around 2048 when AC is present
 * 
 * HARDWARE REQUIREMENTS:
 * ======================
 * ZMPT101B Module:
 * - VCC to 3.3V
 * - GND to GND  
 * - OUT to voltage divider (10kΩ + 10kΩ) for 1.65V bias
 * - Biased output to GPIO 34 through 1kΩ resistor
 * 
 * SCT013 Current Transformer:
 * - Connect across 33Ω burden resistor
 * - Add voltage divider (10kΩ + 10kΩ) for 1.65V bias
 * - Biased output to GPIO 35 through 1kΩ resistor
 * 
 * For higher accuracy, consider using ADS1115 16-bit ADC module
 */