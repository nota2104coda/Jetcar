/*
This is the slave code for the robot car

*/
// Sketch file for the Arduino Mega slave
// This module will read the ultrasonic and IR obstacle sensors as provided and package the data into the I2C bus towards the master module

// Include the header files for our custom classes.

#include <NewPing.h>
#include <Wire.h>
//#include <MsgPacketizer.h>
//#include <vector.h>
//#include "cliffsensor.h"

#define MASTER_PICOW_SLAVE_ARD_4WD_NONSTEER_RUBBERWHL_2XSR04_2xCLIFF
//#define MEGA2560_4WD_NONSTEER_RUBBERWHL_2XSR04_2xCLIFF
#include "E:\Jeevan\projects\PicoWCar\include-arduino\CarConfigurations.h" // sets e.g. CAR_HAS_ENCODERS, USE_ADAFRUIT_MOTOR_SHIELD
#include "E:\Jeevan\projects\PicoWCar\include-arduino\RobotCarPinDefinitionsAndMore.h" // Pinout depends on settings like CAR_HAS_ENCODERS etc.
#if defined(CAR_HAS_FRONT_RR_CLIFF_SENSOR)
	//CliffSensor frontCliff(PIN_FRONT_CLIFF);
	//CliffSensor rearCliff(PIN_REAR_CLIFF);
#endif
#if defined(CAR_HAS_FRONT_RR_SONAR)
	NewPing sonarF(PIN_TRIG_SONAR_FRONT, PIN_ECHO_SONAR_FRONT, SONAR_MAX_DISTANCE);
	NewPing sonarR(PIN_TRIG_SONAR_REAR, PIN_ECHO_SONAR_REAR, SONAR_MAX_DISTANCE);
#endif

SonarIRData myData;

// --- Signal Configuration ---
#define FREQUENCY 0.1    // Frequency of the sine/cosine waves in Hz
#define AMPLITUDE 100.0  // Amplitude of the sine/cosine waves
#define OFFSET 127.0     // Offset to keep values positive

long startTime; // To track time for sine wave calculation
long elapsedTime;
float sineValue;
float cosValue;

// set up interrupt response for I2C
void onRequest() {
  myData.crc = 0;
  uint8_t* p = (uint8_t*)&myData;
  size_t len = sizeof(SonarIRData);

  uint8_t crc = 0;
  for (size_t i = 0; i < len-1; i++) myData.crc ^= p[i];
  
  // Send entire struct including CRC
  Wire.write((uint8_t*)&myData, sizeof(SonarIRData));
}

// =====================================================================
// === Setup Function ===
// =====================================================================

void setup() {
  Serial.begin(115200);
  Serial.print("start of setup");
  Wire.begin(SLAVE_ARD_ADDR); 
  Wire.onRequest(onRequest);       // register callback for incoming data
  delay(100);
  startTime = millis();
  Serial.print("end of setup");
}

// =====================================================================
// === Loop Function ===
// =====================================================================

void loop() {
  Serial.println("start of loop");
  Serial.println("front sonar:");
  Serial.print(sonarF.ping_cm());
  delay(MIN_SONAR_DELAY);
  Serial.println("rear sonar:");
  Serial.print(sonarR.ping_cm());
  delay(MIN_SONAR_DELAY);
  //Read Sonar values
  //read cliff sensor values
  //send values over I2C and msgpacketizer
   // Calculate elapsed time in seconds
  elapsedTime = (millis() - startTime) / 1000.0;
  // Calculate the sine wave value
  sineValue = AMPLITUDE * sin(2 * PI * FREQUENCY * elapsedTime) + OFFSET;
  cosValue = AMPLITUDE * cos(2 * PI * FREQUENCY * elapsedTime) + OFFSET;
  myData.frontDistance = sineValue;
  myData.rearDistance = cosValue;
  myData.frontSonarQF = 1;
  myData.rearSonarQF = 0;
  myData.frontCliffStatus = 0;
  myData.frontCliffQF = 1;
  myData.rearCliffStatus = 1;
  myData.rearCliffQF = 0;

  delay(50);
  Serial.println("end of loop");
}
