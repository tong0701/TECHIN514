#include <Arduino.h>

void setup() {
  // Initialize Serial communication at 115200 baud
  Serial.begin(115200);
  
  // Wait for Serial port to connect (useful for some boards)
  while (!Serial) {
    delay(10);
  }
  
  // Print startup message
  Serial.println("TECHIN 514 Lab 1 – PlatformIO setup successful");
}

void loop() {
  static unsigned long counter = 0;
  static unsigned long lastPrint = 0;
  unsigned long currentTime = millis();
  
  // Print counter every second
  if (currentTime - lastPrint >= 1000) {
    counter++;
    Serial.print("Counter: ");
    Serial.println(counter);
    lastPrint = currentTime;
  }
}

