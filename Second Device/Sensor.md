#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ---------------------------------------------------------
// 1. Sensor Pin Definitions
// ---------------------------------------------------------
#define TRIG_PIN 3  // XIAO D3 (Trig on HC-SR04)
#define ECHO_PIN 2  // XIAO D2 (Echo on HC-SR04)
#define BTN_PIN  5  // XIAO D5 (Push Button to GND)

// ---------------------------------------------------------
// 2. MAC Address of the Display End board
// ---------------------------------------------------------
uint8_t broadcastAddress[] = {0x98, 0x3D, 0xAE, 0xAC, 0xD1, 0x64}; 

// ---------------------------------------------------------
// 3. Core Variables and ESP-NOW Data Structure
// ---------------------------------------------------------
float referenceDistance = 100.0; 
const float detectionThreshold = 10.0; 

// MUST MATCH the Display End structure
typedef struct struct_message {
    bool isBusy;        
    float currentDist;  
} struct_message;

struct_message myData; 

unsigned long lastSendTime = 0;
const int sendInterval = 500; // Send data every 500ms

float measureDistance(); 
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status); 

void setup() {
    Serial.begin(115200);
    
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(BTN_PIN, INPUT_PULLUP); 

    WiFi.mode(WIFI_STA); 

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_register_send_cb(OnDataSent);

    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false; 

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }
}

void loop() {
    // Button Calibration Logic
    if (digitalRead(BTN_PIN) == LOW) { 
        delay(50); 
        if (digitalRead(BTN_PIN) == LOW) {
            Serial.println("Calibrating reference distance...");
            float sum = 0;
            for(int i = 0; i < 5; i++){
                sum += measureDistance();
                delay(100);
            }
            referenceDistance = sum / 5.0;
            Serial.print("New Ref: ");
            Serial.println(referenceDistance);
            delay(1000); 
        }
    }

    float distance = measureDistance();
    
    // Determine State
    if (distance < (referenceDistance - detectionThreshold)) {
        myData.isBusy = true;
    } else {
        myData.isBusy = false;
    }
    
    myData.currentDist = distance; 

    // Send Data
    if (millis() - lastSendTime > sendInterval) {
        lastSendTime = millis();
        esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    }

    delay(100); 
}

float measureDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
    if (duration == 0) return 300.0; 
    return duration / 58.2;
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Optional: Print delivery status
    // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}
