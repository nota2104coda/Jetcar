#include <Arduino.h>
#include <Wire.h>
#include <MAVLink_ardupilotmega.h>

#define I2C_SLAVE_ADDR 0x42
#define PICO_I2C_SDA  4   // Change as per your wiring
#define PICO_I2C_SCL  5   // Change as per your wiring

void setup() {
  Serial.begin(115200);
  Wire.setSDA(PICO_I2C_SDA);
  Wire.setSCL(PICO_I2C_SCL);
  Wire.begin(); // Master mode
  delay(1000);
}

void loop() {
  // Construct MAVLink SET_ACTUATOR_CONTROL_TARGET message
  mavlink_set_actuator_control_target_t act = {};
  act.time_usec = micros();
  act.group_mlx = 0;
  act.target_system = 200; // Should match kMavSystemId on slave
  act.target_component = MAV_COMP_ID_ONBOARD_COMPUTER;
  act.controls[0] = 0.1f; // Example actuator values
  act.controls[1] = 0.2f;
  act.controls[2] = -0.3f;
  act.controls[3] = -0.4f;

  mavlink_message_t msg;
  mavlink_msg_set_actuator_control_target_encode(
    act.target_system, act.target_component, &msg, &act);

  uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
  uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);

  // Send over I2C
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(buffer, len);
  Wire.endTransmission();

  Serial.println("Sent MAVLink SET_ACTUATOR_CONTROL_TARGET over I2C");
  delay(1000); // Send every second
}