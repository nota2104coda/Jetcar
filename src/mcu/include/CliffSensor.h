#pragma once

#include <Arduino.h>
#include <Queue.h>

class CliffSensor {
private:
  static constexpr uint8_t kWindowSize = 8;      // number of samples to debounce over
  static constexpr uint8_t kTriggerCount = 5;    // require this many "true" samples in window

  uint8_t pin;
  bool lastState;
  Queue<bool, kWindowSize> window;                          // circular buffer of recent readings
  uint8_t windowIndex = 0;
  uint8_t thisCount = 0;                         // running count of "cliff detected" in window
  
public:
  explicit CliffSensor(uint8_t sensorPin) : pin(sensorPin), lastState(false) {}

  void init() {
    pinMode(pin, INPUT);
    for (uint8_t i = 0; i < kWindowSize; ++i) {
      window.enqueue(true);
    }
    //default tell that cliff is present at start.
    lastState = true;
    //and for this, the count would have reached kTriggerCount
    thisCount = kTriggerCount;  
  }
  bool isCliffDetected() const {
    // IR cliff sensors typically output LOW when cliff is detected
    // (no reflection = no ground detected)
    return (digitalRead(pin) == LOW);
  }
  bool read() {
    // IR cliff sensors typically output LOW when cliff is detected
    // (no reflection = no ground detected)
    bool cliffraw = isCliffDetected();
    Serial.print("Raw cliff reading: ");
    Serial.println(cliffraw ? "DETECTED" : "NOT DETECTED");
    // Add new sample to the back of the queue
    window.dequeue();
    window.enqueue(cliffraw);
    thisCount = 0;
    for(uint8_t i=0; i<kTriggerCount; i++) {
      thisCount += window.at(i);
    }
    switch(lastState){
      case false:
        if(thisCount >= kTriggerCount) {
          lastState = true;
        }
        break;
      case true:
        if(thisCount < 1) {
          lastState = false;
        }
        break;
    }
    
    return lastState;
  }
  
  bool getLastState() const {
    return lastState;
  }

  void printFullWindow() {
    Serial.print("Window: ");
    for (uint8_t i = 0; i < kWindowSize; ++i) {
      bool val = window.at(i);
      Serial.print(val ? "1 " : "0 ");
    }
    Serial.println();
  }
};