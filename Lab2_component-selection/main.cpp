#include <Arduino.h>

// Assuming connection to pin A0 (On XIAO ESP32S3/C3, this usually maps to D0)
// If you connected to a different pin, please change it here
const int analogPin = A0; 

void setup() {
  // Initialize serial communication, baud rate 9600
  Serial.begin(9600);
  
  // Set ADC resolution to 12-bit (Reading range 0-4095)
  analogReadResolution(12);
}

void loop() {
  // Read the analog value
  int adcValue = analogRead(analogPin);
  
  // Convert to voltage (using 3.3V reference voltage)
  float voltage = (adcValue / 4095.0) * 3.3;
  
  // Print results to Serial Monitor
  Serial.print("Raw ADC: ");
  Serial.print(adcValue);
  Serial.print(" | Voltage: ");
  Serial.print(voltage);
  Serial.println(" V");
  
  delay(500); // Refresh every 0.5 seconds
}
