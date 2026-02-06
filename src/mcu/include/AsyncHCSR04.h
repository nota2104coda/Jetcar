#pragma once

#include <Arduino.h>

// Minimal non-blocking HC-SR04 driver for RP2040/Arduino
// Uses attachInterrupt on the echo pin and a small state machine.
class AsyncHCSR04 {
 public:
  AsyncHCSR04(uint8_t triggerPin, uint8_t echoPin, uint32_t timeoutUs = 30000U);

  void begin();
  bool startPing();            // Returns false if a measurement is already in flight
  void update();               // Call regularly to enforce timeout
  bool readDistanceCm(float &outCm);  // Returns true if a fresh reading was captured
  bool isBusy() const { return measuring_; }

 private:
  void handleEdge(bool levelHigh, uint32_t nowMicros);
  static void onEchoChange();
  static void registerInstance(AsyncHCSR04 *inst);

  static constexpr uint8_t kMaxInstances = 8;
  static AsyncHCSR04 *instances_[kMaxInstances];
  static uint8_t instanceCount_;

  const uint8_t triggerPin_;
  const uint8_t echoPin_;
  const uint32_t timeoutUs_;

  volatile bool measuring_ = false;
  volatile bool risingCaptured_ = false;
  volatile bool ready_ = false;
  volatile uint32_t startMicros_ = 0;
  volatile uint32_t endMicros_ = 0;
  volatile float lastDistanceCm_ = 0.0F;
  uint32_t triggerStartMicros_ = 0;
};
