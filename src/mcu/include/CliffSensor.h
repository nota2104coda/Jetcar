#pragma once

#include <Arduino.h>

class CliffSensor {
private:
  static constexpr uint8_t kWindowSize = 8;      // number of samples to debounce over
  static constexpr uint8_t kTriggerCount = 5;    // require this many "true" samples in window

  uint8_t pin;
  bool lastState;
  bool window[kWindowSize]{};                    // circular buffer of recent readings
  uint8_t windowIndex = 0;
  uint8_t windowCount = 0;                       // how many slots are filled (up to kWindowSize)
  uint8_t trueCount = 0;                         // running count of "cliff detected" in window
  
public:
  explicit CliffSensor(uint8_t sensorPin) : pin(sensorPin), lastState(false) {}
  
  void init() {
    pinMode(pin, INPUT);
  }
  
  bool isCliffDetected() const {
    // IR cliff sensors typically output LOW when cliff is detected
    // (no reflection = no ground detected)
    return digitalRead(pin) == LOW;
  }
  
  bool read() {
    const bool cliffraw = isCliffDetected();

    // Remove the value leaving the window from the running count when full
    if (windowCount == kWindowSize) {
      if (window[windowIndex]) {
        trueCount--;
      }
    } else {
      windowCount++;
    }

    // Store new sample and update count
    window[windowIndex] = cliffraw;
    if (cliffraw) {
      trueCount++;
    }

    // advance ring index
    windowIndex = static_cast<uint8_t>((windowIndex + 1U) % kWindowSize);

    // majority/threshold decision
    lastState = (trueCount >= kTriggerCount);
    return lastState;
  }
  
  bool getLastState() const {
    return lastState;
  }
};