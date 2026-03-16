/*
 * Taya Display Server — Focus Radar
 */
 #include <Arduino.h>
 #include <BLEDevice.h>
 #include <BLEServer.h>
 #include <BLEUtils.h>
 #include <BLE2902.h>
 
 // ── Pins ──────────────────────────────────
 const int IN1 = 2, IN2 = 4, IN3 = 3, IN4 = 5;
 const int LED_BLUE   = 5;
 const int LED_RED    = 6;
 const int BUTTON_PIN = 10;
 
 // ── BLE ───────────────────────────────────
 #define DEVICE_NAME  "Taya_Display_Server"
 #define SERVICE_UUID "6f4e0001-1111-2222-3333-444455556666"
 #define CHAR_UUID    "6f4e0002-1111-2222-3333-444455556666"
 BLECharacteristic *pCharacteristic;
 
 // ── States ────────────────────────────────
 enum ProximityState { SAFE, APPROACHING, TOO_CLOSE };
 ProximityState currentState = SAFE;
 bool pauseMode = false;
 
 // ── Motor ─────────────────────────────────
 const int stepSequence[8][4] = {
   {1,0,0,0},{1,1,0,0},{0,1,0,0},{0,1,1,0},
   {0,0,1,0},{0,0,1,1},{0,0,0,1},{1,0,0,1}
 };
 int stepIndex = 0;
 const int POS_SAFE        = 0;
 const int POS_APPROACHING = 900;
 const int POS_TOO_CLOSE   = 1800;
 int currentPosition = 0;
 int targetPosition  = 0;
 
 unsigned long lastStepTime = 0;
 const uint32_t STEP_US = 800;
 
 // ── Button ────────────────────────────────
 unsigned long lastDebounceTime = 0;
 const unsigned long DEBOUNCE_MS = 100;
 
 // ── Blink ─────────────────────────────────
 bool blinkOn = false;
 unsigned long lastBlinkTime = 0;
 const unsigned long BLINK_INTERVAL = 400;
 
 // ── LED ───────────────────────────────────
 void setBlue() {
   digitalWrite(LED_BLUE, HIGH);
   digitalWrite(LED_RED, LOW);
 }
 void setRed() {
   digitalWrite(LED_BLUE, LOW);
   digitalWrite(LED_RED, HIGH);
 }
 void setOff() {
   digitalWrite(LED_BLUE, LOW);
   digitalWrite(LED_RED, LOW);
 }
 
 // ── Motor helpers ─────────────────────────
 int posForState(ProximityState s) {
   switch (s) {
     case SAFE:        return POS_SAFE;
     case APPROACHING: return POS_APPROACHING;
     case TOO_CLOSE:   return POS_TOO_CLOSE;
     default:          return POS_SAFE;
   }
 }
 
 void releaseMotor() {
   digitalWrite(IN1,0); digitalWrite(IN2,0);
   digitalWrite(IN3,0); digitalWrite(IN4,0);
 }
 
 // ── Apply state ───────────────────────────
 void applyState(ProximityState newState) {
   if (pauseMode) {
     currentState = newState;
     return;
   }
   if (newState == currentState) return;
   currentState = newState;
   targetPosition = posForState(currentState);
   if (currentState == SAFE)      setBlue();
   if (currentState == TOO_CLOSE) setRed();
   Serial.print("State -> ");
   Serial.println(
     newState == SAFE ? "SAFE" :
     newState == APPROACHING ? "APPROACHING" : "TOO_CLOSE"
   );
 }
 
 // ── BLE callbacks ─────────────────────────
 class MyCallbacks : public BLECharacteristicCallbacks {
   void onWrite(BLECharacteristic *pChar) override {
     std::string value = pChar->getValue();
     if (value.empty()) return;
     String msg = String(value.c_str());
     Serial.print("Received: "); Serial.println(msg);
     if      (msg == "SAFE")        applyState(SAFE);
     else if (msg == "APPROACHING") applyState(APPROACHING);
     else if (msg == "TOO_CLOSE")   applyState(TOO_CLOSE);
   }
 };
 
 class MyServerCallbacks : public BLEServerCallbacks {
   void onConnect(BLEServer*) override {
     Serial.println("Client connected.");
   }
   void onDisconnect(BLEServer*) override {
     Serial.println("Disconnected. Restarting advertising.");
     pauseMode = false;
     currentState = SAFE;
     targetPosition = POS_SAFE;
     setBlue();
     BLEDevice::startAdvertising();
   }
 };
 
 // ── Setup ─────────────────────────────────
 void setup() {
   Serial.begin(115200);
   pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT);
   pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
   pinMode(LED_BLUE, OUTPUT);
   pinMode(LED_RED, OUTPUT);
   pinMode(BUTTON_PIN, INPUT_PULLUP);
   releaseMotor();
 
   currentPosition = 0;
   targetPosition  = 0;
   setBlue();
 
   BLEDevice::init(DEVICE_NAME);
   BLEServer *pServer = BLEDevice::createServer();
   pServer->setCallbacks(new MyServerCallbacks());
   BLEService *pService = pServer->createService(SERVICE_UUID);
   pCharacteristic = pService->createCharacteristic(
     CHAR_UUID, BLECharacteristic::PROPERTY_WRITE
   );
   pCharacteristic->setCallbacks(new MyCallbacks());
   pCharacteristic->addDescriptor(new BLE2902());
   pService->start();
   BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
   BLEDevice::getAdvertising()->start();
   Serial.println("BLE ready: " DEVICE_NAME);
 }
 
 // ── Loop ──────────────────────────────────
 void loop() {
   // Stepper tick
   unsigned long now = micros();
   if (now - lastStepTime >= STEP_US) {
     lastStepTime = now;
     if (currentPosition != targetPosition) {
       int dir = (targetPosition > currentPosition) ? 1 : -1;
       stepIndex = (stepIndex - dir + 8) % 8;
       digitalWrite(IN1, stepSequence[stepIndex][0]);
       digitalWrite(IN2, stepSequence[stepIndex][1]);
       digitalWrite(IN3, stepSequence[stepIndex][2]);
       digitalWrite(IN4, stepSequence[stepIndex][3]);
       currentPosition += dir;
       if (currentPosition == targetPosition) releaseMotor();
     }
   }
 
   // APPROACHING blink
   if (currentState == APPROACHING && !pauseMode) {
     unsigned long ms = millis();
     if (ms - lastBlinkTime >= BLINK_INTERVAL) {
       lastBlinkTime = ms;
       blinkOn = !blinkOn;
       blinkOn ? setRed() : setOff();
     }
   }
 
   // Button
   if (digitalRead(BUTTON_PIN) == LOW) {
     unsigned long ms = millis();
     if (ms - lastDebounceTime > DEBOUNCE_MS) {
       lastDebounceTime = ms;
       pauseMode = !pauseMode;
       if (pauseMode) {
         Serial.println("Pause ON.");
         setBlue();
         targetPosition = POS_SAFE;
       } else {
         Serial.println("Pause OFF.");
         if (currentState == SAFE)      setBlue();
         if (currentState == TOO_CLOSE) setRed();
         targetPosition = posForState(currentState);
       }
     }
   }
 }