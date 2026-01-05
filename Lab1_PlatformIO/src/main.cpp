#include <Arduino.h>
#include <Bounce2.h>

const int SWITCH_PIN = D2;
const int LED_PIN = D10;

Bounce2::Button button = Bounce2::Button();
bool ledState = false;

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  button.attach(SWITCH_PIN, INPUT_PULLUP);
  button.interval(50);
  button.setPressedState(LOW);

  Serial.println("Ready");
}

void loop() {
  button.update();

  if (button.pressed()) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    Serial.println(ledState ? "LED ON" : "LED OFF");
  }

  delay(10);
}
