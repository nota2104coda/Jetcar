#include <Arduino.h>

#define PINNUM 16
void setup() {
    pinMode(PINNUM, OUTPUT);
    digitalWrite(PINNUM, HIGH);
    Serial.begin(115200);
}

void loop() {
    // digitalWrite(LED_PIN, HIGH);
    // delay(500);
    Serial.println("hello");
    // digitalWrite(LED_PIN, LOW);
    // delay(500);
}