/*
 * Taya Sensor Client — Focus Radar
 * TRIG: GPIO 9, ECHO: GPIO 8
 * 
 * TOO_CLOSE auto-reset: 3s → APPROACHING
 * Cooldown: 5s, then full buffer reset to SAFE
 */
 #include <Arduino.h>
 #include <BLEDevice.h>
 #include <BLEUtils.h>
 #include <BLEScan.h>
 #include <BLEAdvertisedDevice.h>
 #include <BLEClient.h>
 
 // ── BLE ───────────────────────────────────
 #define TARGET_NAME  "Taya_Display_Server"
 #define SERVICE_UUID "6f4e0001-1111-2222-3333-444455556666"
 #define CHAR_UUID    "6f4e0002-1111-2222-3333-444455556666"
 
 // ── Sensor pins ───────────────────────────
 #define TRIG_PIN 9
 #define ECHO_PIN 8
 
 // ── Thresholds ────────────────────────────
 const float THRESH_FAR        = 150.0;
 const float THRESH_NEAR_ENTER = 45.0;
 const float THRESH_NEAR_EXIT  = 60.0;
 const float THRESH_FAR_EXIT   = 130.0;
 const float NOISE_FLOOR       = 8.0;
 
 // ── Timing ────────────────────────────────
 const unsigned long TOO_CLOSE_TIMEOUT_MS = 3000;
 const unsigned long COOLDOWN_MS          = 5000;
 
 // ── Filter ────────────────────────────────
 const int WINDOW_SIZE = 3;
 float readings[WINDOW_SIZE] = {150.0, 150.0, 150.0};
 int readIndex = 0;
 bool bufferFilled = false;
 float lastValidDistance = 150.0;
 
 // ── BLE globals ───────────────────────────
 BLEAdvertisedDevice* targetDevice = nullptr;
 BLEClient* pClient = nullptr;
 BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
 bool doConnect = false;
 bool connected = false;
 BLEScan* pBLEScan = nullptr;
 
 // ── State ─────────────────────────────────
 String currentState         = "SAFE";
 String lastSentState        = "";
 unsigned long tooCloseEnteredAt = 0;
 unsigned long cooldownStartAt   = 0;
 bool inCooldown = false;
 
 // ── Buffer reset ──────────────────────────
 void resetBuffer() {
   for (int i = 0; i < WINDOW_SIZE; i++) readings[i] = 150.0;
   readIndex = 0;
   bufferFilled = false;
   lastValidDistance = 150.0;
 }
 
 // ── Moving average ────────────────────────
 float movingAverage(float newVal) {
   readings[readIndex++] = newVal;
   if (readIndex >= WINDOW_SIZE) { readIndex = 0; bufferFilled = true; }
   int count = bufferFilled ? WINDOW_SIZE : readIndex;
   float sum = 0;
   for (int i = 0; i < count; i++) sum += readings[i];
   return sum / count;
 }
 
 // ── HC-SR04 ───────────────────────────────
 float readDistanceCM() {
   digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
   digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
   digitalWrite(TRIG_PIN, LOW);
   long dur = pulseIn(ECHO_PIN, HIGH, 40000);
   if (dur == 0) return -1.0;
   float d = dur * 0.0343 / 2.0;
   if (d < NOISE_FLOOR) return -1.0;
   return d;
 }
 
 // ── Send state ────────────────────────────
 void sendState(String state) {
   pRemoteCharacteristic->writeValue(
     (uint8_t*)state.c_str(), state.length(), false
   );
   Serial.print("Sent: "); Serial.println(state);
   lastSentState = state;
   currentState  = state;
 }
 
 // ── State machine with hysteresis ─────────
 String computeRawState(float filtered, String prevState) {
   if (prevState == "TOO_CLOSE") {
     if (filtered > THRESH_NEAR_EXIT)
       return filtered > THRESH_FAR ? "SAFE" : "APPROACHING";
     return "TOO_CLOSE";
   }
   if (prevState == "SAFE") {
     if (filtered < THRESH_FAR_EXIT)
       return filtered < THRESH_NEAR_ENTER ? "TOO_CLOSE" : "APPROACHING";
     return "SAFE";
   }
   // APPROACHING
   if (filtered > THRESH_FAR)        return "SAFE";
   if (filtered < THRESH_NEAR_ENTER) return "TOO_CLOSE";
   return "APPROACHING";
 }
 
 // ── BLE scan callback ─────────────────────
 class MyScanCallbacks : public BLEAdvertisedDeviceCallbacks {
   void onResult(BLEAdvertisedDevice dev) override {
     if (String(dev.getName().c_str()) == TARGET_NAME &&
         dev.haveServiceUUID() &&
         dev.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
       Serial.println("Found Taya_Display_Server.");
       targetDevice = new BLEAdvertisedDevice(dev);
       doConnect = true;
       pBLEScan->stop();
     }
   }
 };
 
 // ── Connect ───────────────────────────────
 bool connectToServer() {
   if (!targetDevice) return false;
   pClient = BLEDevice::createClient();
   if (!pClient->connect(targetDevice)) {
     Serial.println("Connection failed."); return false;
   }
   BLERemoteService* svc = pClient->getService(BLEUUID(SERVICE_UUID));
   if (!svc) { pClient->disconnect(); return false; }
   pRemoteCharacteristic = svc->getCharacteristic(BLEUUID(CHAR_UUID));
   if (!pRemoteCharacteristic) { pClient->disconnect(); return false; }
   connected = true;
   Serial.println("Connected and characteristic found.");
   return true;
 }
 
 void startScan() {
   Serial.println("Scanning...");
   pBLEScan = BLEDevice::getScan();
   pBLEScan->setAdvertisedDeviceCallbacks(new MyScanCallbacks());
   pBLEScan->setActiveScan(true);
   pBLEScan->setInterval(100);
   pBLEScan->setWindow(99);
   pBLEScan->start(5, false);
 }
 
 // ── Setup ─────────────────────────────────
 void setup() {
   Serial.begin(115200);
   delay(500);
   pinMode(TRIG_PIN, OUTPUT);
   pinMode(ECHO_PIN, INPUT);
   BLEDevice::init("Taya_Sensor_Client");
   startScan();
 }
 
 // ── Loop ──────────────────────────────────
 void loop() {
   if (!connected) {
     if (doConnect) {
       if (connectToServer()) {
         Serial.println("Ready.");
       } else {
         Serial.println("Retrying...");
         doConnect = false;
         delete targetDevice; targetDevice = nullptr;
         startScan();
       }
     } else {
       delay(500);
     }
     return;
   }
 
   unsigned long now = millis();
 
   // ── Cooldown check ──────────────────────
   if (inCooldown && now - cooldownStartAt >= COOLDOWN_MS) {
     inCooldown = false;
     resetBuffer();
     currentState = "SAFE";
     lastSentState = "SAFE";
     Serial.println("Cooldown ended — full reset to SAFE.");
   }
 
   // ── Read sensor ─────────────────────────
   float raw = readDistanceCM();
   if (raw < 0) {
     raw = lastValidDistance;
   } else {
     lastValidDistance = raw;
   }
   float filtered = movingAverage(raw);
 
   // ── Compute new state ───────────────────
   String rawState = computeRawState(filtered, currentState);
 
   // Block TOO_CLOSE during cooldown
   if (inCooldown && rawState == "TOO_CLOSE") {
     rawState = "APPROACHING";
   }
 
   // ── TOO_CLOSE timeout logic ─────────────
   if (rawState == "TOO_CLOSE") {
     if (currentState != "TOO_CLOSE") {
       tooCloseEnteredAt = now;
       sendState("TOO_CLOSE");
     } else if (now - tooCloseEnteredAt >= TOO_CLOSE_TIMEOUT_MS) {
       Serial.println("TOO_CLOSE timeout → auto reset.");
       resetBuffer();
       sendState("APPROACHING");
       inCooldown = true;
       cooldownStartAt = now;
     }
   } else {
     if (rawState != currentState) {
       sendState(rawState);
       if (rawState == "SAFE") {
         inCooldown = false;
         resetBuffer();
         Serial.println("Person left — full reset to SAFE.");
       }
     }
   }
 
   Serial.printf("Raw: %.1f cm | Filtered: %.1f cm | State: %s%s\n",
                 raw, filtered, currentState.c_str(),
                 inCooldown ? " [cooldown]" : "");
 
   delay(100);
 }