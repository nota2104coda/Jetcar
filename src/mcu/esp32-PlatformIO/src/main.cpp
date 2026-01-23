#include <cstdint>
#include <array>
#include <limits>
#include <cmath>
#include <SimpleFOC.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <NewPing.h>

// #define MCU_PICOW_RP2040
#define MCU_ESP32S3_40PIN 
#define NONSTEER_4WD_RUBBERWHL_2XSONAR_2xCLIFF

// Set to 1 to enable loop debug output, 0 to disable. Ralph S Bacon from Youtube solution
#define LOOP_DEBUG_A 0
#define LOOP_DEBUG_B 0
#define LOOP_DEBUG_I2C 1

#if LOOP_DEBUG_A
  #define DEBUG_PRINT(...) Serial.print(__VA_ARGS__); Serial.flush()
  #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__); Serial.flush()
#else
  #define DEBUG_PRINT(...) ((void)0)
  #define DEBUG_PRINTLN(...) ((void)0)
#endif
#if LOOP_DEBUG_B
  #define DEBUG_B_PRINT(...) Serial.print(__VA_ARGS__); Serial.flush()
  #define DEBUG_B_PRINTLN(...) Serial.println(__VA_ARGS__); Serial.flush()
#else
  #define DEBUG_B_PRINT(...) ((void)0)
  #define DEBUG_B_PRINTLN(...) ((void)0)
#endif

#if LOOP_DEBUG_I2C
  #define DEBUG_I2C_PRINT(...) Serial.print(__VA_ARGS__); Serial.flush()
  #define DEBUG_I2C_PRINTLN(...) Serial.println(__VA_ARGS__); Serial.flush()
#else
  #define DEBUG_I2C_PRINT(...) ((void)0)
  #define DEBUG_I2C_PRINTLN(...) ((void)0)
#endif

#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <Arduino.h>
#include <MAVLink_ardupilotmega.h>

//follow metric system everywhere except for distance/speed/acceleration are in cm. angles in rad, angular velocity in rad/s

#include "/home/jeevan/PicoWCar/src/mcu/include/CarConfigurations.h"
#include "/home/jeevan/PicoWCar/src/mcu/include/RobotCarPinDefinitionsAndMore.h"
#include "/home/jeevan/PicoWCar/src/mcu/include/CliffSensor.h"
#include "/home/jeevan/PicoWCar/src/mcu/include/PCA9685_AWDDriver.h"
#include "/home/jeevan/PicoWCar/src/mcu/include/stateMachines.h"

CliffSensor frontCliff(PIN_FRONT_CLIFF);
// CliffSensor rearCliff(PIN_REAR_CLIFF);

NewPing sonarF(PIN_TRIG_SONAR_FRONT, PIN_ECHO_SONAR_FRONT, kMaxSonarRangecm); //the lib needs cm as max range

void setup() {
  Serial.begin(115200);
  delay(2500);
  const uint32_t serialStart = millis();
  while ((!Serial) && ((millis() - serialStart) < 500 )) {
    delay(10);
  }

  Serial.println();
  Serial.println("==========================================");
  Serial.println("[DEBUG] MCU BOOT - Watchdog ENABLED");

  Serial.println("[INIT] MCU starting...");

  Serial.println("[INIT] Initializing cliff sensors...");
  frontCliff.init();
  Serial.println("[INIT] Cliff sensors initialized.");

}

void loop() {

  delay(1000);
  frontCliff.printFullWindow();
  frontCliff.read();
  Serial.print("Front Cliff lastState: ");
  Serial.println(frontCliff.getLastState());
}