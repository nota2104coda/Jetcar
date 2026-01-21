#pragma once

#include <cstdint>

// Shared telemetry packet for Jetson (packed to keep wire layout deterministic)
struct __attribute__((packed)) JetsonTelemetryPacket {
  uint32_t timestamp_ms;
  float speedFL;
  float speedFR;
  float speedRL;
  float speedRR;
  int16_t sonarFrontcm;
  int16_t sonarRearcm;
  uint8_t cliffFront;
  uint8_t cliffRear;
  float accelX;
  float accelY;
  float gyroZ;
  float tempC;
};

using MotorCommandCallback = void(*)(float fr, float fl, float rr, float rl);

// Initialization (optional)
void mavlinkInit();

// Call frequently from main loop to process incoming UART bytes (parses MOTOR_CMD messages)
void mavlinkHandleUART();

// Update the telemetry snapshot that will be sent back to the Jetson
void mavlinkUpdateTelemetry(const JetsonTelemetryPacket &t);

// Send the current telemetry snapshot right away
void mavlinkSendTelemetry();

// Set a callback that will be invoked when a MOTOR_CMD message is received
void mavlinkSetMotorCallback(MotorCommandCallback cb);

// Helper to directly send a MOTOR_CMD (useful for handshake/testing)
void mavlinkSendMotorCmd(float fr, float fl, float rr, float rl);
