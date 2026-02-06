#pragma once

enum class SystemState : uint8_t {
  INIT = 0,
  RUNNING = 1,
  DEGRADED = 2,  // Running without IMU
  ERROR = 3
};

// Shared data structures with synchronization
struct SensorBuffer {
  float accelX, accelY;
  float gyroZ;
  float temp;
  int32_t sonarFrontm;
  int8_t sonarFquality;
  int32_t sonarRearm;
  int8_t sonarRquality;
  bool cliffFront, cliffRear;
  float speedFL, speedFR, speedRL, speedRR;
  uint32_t timestamp;
};

struct MotorCommand {
  float tqFR, tqFL, tqRR, tqRL;
  uint32_t commandTime;
};
