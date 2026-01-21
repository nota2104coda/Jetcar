#pragma once

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>
#include <SimpleFOC.h>
#include <Wire.h>

class PCA9685_AWDDriver {
private:
  Adafruit_PWMServoDriver pwm;
  TwoWire *wire_;  // Store the I2C bus pointer
  Encoder encFR, encFL, encRR, encRL;
  uint32_t lastMotorCommandTime[4];  // Individual timestamp for each motor
  uint8_t pulsesPerRev;
  float maxSaneRpm;
  
  // Static instance pointer for interrupt handlers
  static PCA9685_AWDDriver *instance_;
  
  // Static interrupt handlers
  static void handleEncFRA() { if (instance_) instance_->encFR.handleA(); }
  static void handleEncFRB() { if (instance_) instance_->encFR.handleB(); }
  static void handleEncRRA() { if (instance_) instance_->encRR.handleA(); }
  static void handleEncRRB() { if (instance_) instance_->encRR.handleB(); }
  static void handleEncFLA() { if (instance_) instance_->encFL.handleA(); }
  static void handleEncFLB() { if (instance_) instance_->encFL.handleB(); }
  static void handleEncRLA() { if (instance_) instance_->encRL.handleA(); }
  static void handleEncRLB() { if (instance_) instance_->encRL.handleB(); }
  
public:
  PCA9685_AWDDriver(uint8_t addr, TwoWire *theWire,
                     uint8_t frA, uint8_t frB,
                     uint8_t flA, uint8_t flB,
                     uint8_t rrA, uint8_t rrB,
                     uint8_t rlA, uint8_t rlB,
                     uint8_t ppr = 12, float maxRpm = 200.0)
    : pwm(addr, *theWire),  // Pass reference to TwoWire object
      wire_(theWire),
      encFR(frA, frB, ppr),
      encFL(flA, flB, ppr),
      encRR(rrA, rrB, ppr),
      encRL(rlA, rlB, ppr),
      lastMotorCommandTime{0, 0, 0, 0},
      pulsesPerRev(ppr),
      maxSaneRpm(maxRpm) {
    instance_ = this;
  }
  
  bool begin(uint16_t freqHz) {
    // Initialize PWM driver (no return value to check)
    pwm.begin();
    pwm.setPWMFreq(freqHz);
    delay(10);
    
    // Initialize encoders
    encFR.init();
    encFL.init();
    encRR.init();
    encRL.init();
    
    // Enable encoder interrupts
    encFR.enableInterrupts(handleEncFRA, handleEncFRB);
    encFL.enableInterrupts(handleEncFLA, handleEncFLB);
    encRR.enableInterrupts(handleEncRRA, handleEncRRB);
    encRL.enableInterrupts(handleEncRLA, handleEncRLB);
    
    return true;
  }
  
  void initEncoders() {
    encFR.init();
    encFL.init();
    encRR.init();
    encRL.init();
  }
  
  float getRPM(uint8_t motorNum) {
    switch(motorNum) {
      case 0: return (encFR.getVelocity() * 60.0) / (2.0 * PI) / pulsesPerRev;
      case 1: return (encFL.getVelocity() * 60.0) / (2.0 * PI) / pulsesPerRev;
      case 2: return (encRR.getVelocity() * 60.0) / (2.0 * PI) / pulsesPerRev;
      case 3: return (encRL.getVelocity() * 60.0) / (2.0 * PI) / pulsesPerRev;
      default: return 0.0F;
    }
  }
  
  bool isRPMSane(float rpm) {
    return (fabsf(rpm) <= maxSaneRpm);
  }
  
  void setMotor(uint8_t motorNum, float torqueCmd) {
    // Bounds check motor number
    if (motorNum > 3) {
      return;
    }

    const uint8_t pwmCh = static_cast<uint8_t>(motorNum * 3);
    const uint8_t in1Ch = static_cast<uint8_t>(motorNum * 3U + 1);
    const uint8_t in2Ch = static_cast<uint8_t>(motorNum * 3U + 2);

    // Clamp torqueCmd to safe range
    // Concept 2: Validate inputs, Concept 3: Fail-safe defaults
    float clamped = torqueCmd;
    if (clamped > 1.0F) clamped = 1.0F;
    if (clamped < -1.0F) clamped = -1.0F;

    const uint16_t pwm_val = static_cast<uint16_t>(fabsf(clamped) * 4095.0F);
    pwm.setPWM(pwmCh, 0, pwm_val);

    if (clamped > 0.0F) {
      pwm.setPWM(in1Ch, 0, 4095);
      pwm.setPWM(in2Ch, 0, 0);
    } else if (clamped < 0.0F) {
      pwm.setPWM(in1Ch, 0, 0);
      pwm.setPWM(in2Ch, 0, 4095);
    } else {
      pwm.setPWM(in1Ch, 0, 0);
      pwm.setPWM(in2Ch, 0, 0);
    }

    lastMotorCommandTime[motorNum] = millis();
  }
  
  void stopAllMotors() {
    for (uint8_t i = 0; i < 4; i++) {
      setMotor(i, 0.0F);
    }
  }
  
  // Check motor safety timeouts - call this periodically from loop()
  // Stops any motor that hasn't received a command within timeoutMs
  bool checkMotorSafetyTimeouts(uint32_t timeoutMs) {
    const uint32_t now = millis();
    bool anyTimedOut = false;
    
    for (uint8_t i = 0; i < 4; i++) {
      if ((now - lastMotorCommandTime[i]) > timeoutMs) {
        anyTimedOut = true;
      }
    }
    if (anyTimedOut) {
      stopAllMotors();
    }
    
    return anyTimedOut;  // Returns true if any motor was stopped
  }

  uint32_t getLastCommandTime(uint8_t motorNum = 0) {
    if (motorNum > 3) return 0;
    return lastMotorCommandTime[motorNum];
  }
};

// Static instance definition
PCA9685_AWDDriver *PCA9685_AWDDriver::instance_ = nullptr;