#include "AsyncHCSR04.h"

AsyncHCSR04 *AsyncHCSR04::instances_[AsyncHCSR04::kMaxInstances] = {nullptr};
uint8_t AsyncHCSR04::instanceCount_ = 0;

AsyncHCSR04::AsyncHCSR04(uint8_t triggerPin, uint8_t echoPin, uint32_t timeoutUs)
    : triggerPin_(triggerPin), echoPin_(echoPin), timeoutUs_(timeoutUs) {}

void AsyncHCSR04::begin() {
  pinMode(triggerPin_, OUTPUT);
  digitalWrite(triggerPin_, LOW);
  pinMode(echoPin_, INPUT);

  registerInstance(this);
  attachInterrupt(digitalPinToInterrupt(echoPin_), AsyncHCSR04::onEchoChange, CHANGE);
}

bool AsyncHCSR04::startPing() {
  if (measuring_) {
    return false;
  }
  ready_ = false;
  risingCaptured_ = false;
  startMicros_ = 0;
  endMicros_ = 0;
  triggerStartMicros_ = micros();

  // 10us trigger pulse
  digitalWrite(triggerPin_, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin_, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin_, LOW);

  measuring_ = true;
  return true;
}

void AsyncHCSR04::update() {
  if (measuring_) {
    const uint32_t now = micros();
    if ((now - triggerStartMicros_) > timeoutUs_) {
      measuring_ = false;
      risingCaptured_ = false;
      lastDistanceCm_ = 0.0F;  // timeout
      ready_ = true;
    }
  }
}

bool AsyncHCSR04::readDistanceCm(float &outCm) {
  if (!ready_) {
    return false;
  }
  // Capture values atomically
  noInterrupts();
  float dist = lastDistanceCm_;
  ready_ = false;
  interrupts();

  outCm = dist;
  return true;
}

void AsyncHCSR04::handleEdge(bool levelHigh, uint32_t nowMicros) {
  if (!measuring_) {
    return;
  }

  if (levelHigh) {
    // Rising edge marks echo start
    risingCaptured_ = true;
    startMicros_ = nowMicros;
  } else {
    // Falling edge marks echo end
    if (risingCaptured_) {
      endMicros_ = nowMicros;
      const uint32_t pulse = (endMicros_ >= startMicros_) ? (endMicros_ - startMicros_) : 0U;
      lastDistanceCm_ = static_cast<float>(pulse) / 58.0F;  // HC-SR04 constant
      measuring_ = false;
      ready_ = true;
      risingCaptured_ = false;
    }
  }
}

void AsyncHCSR04::onEchoChange() {
  const uint32_t now = micros();
  for (uint8_t i = 0; i < instanceCount_; i++) {
    AsyncHCSR04 *inst = instances_[i];
    if (inst == nullptr) {
      continue;
    }
    const bool levelHigh = digitalRead(inst->echoPin_) != 0;
    inst->handleEdge(levelHigh, now);
  }
}

void AsyncHCSR04::registerInstance(AsyncHCSR04 *inst) {
  if (instanceCount_ >= kMaxInstances) {
    return;
  }
  instances_[instanceCount_++] = inst;
}
