#include <Arduino.h>

constexpr gpio_num_t LED_PIN = GPIO_NUM_47;  // On-board RGB LED blue channel on Pico M

void setup() {
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(115200);
}

void loop() {
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    Serial.println("hello");
    digitalWrite(LED_PIN, LOW);
    delay(500);
}