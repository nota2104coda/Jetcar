#pragma once

#include <WiFi.h>

// Start the web control server
void start_web_control();

// Check for and process web client requests (call from core1 task loop)
void process_web_clients();

// Queue a motor command to the motor control system
void queue_motor_command(float tqFR, float tqFL, float tqRR, float tqRL);

// Getters for status
bool is_robot_enabled();
uint8_t get_control_mode(); // 0=Web, 1=UART
