/*
This is the master code for the robot car

*/
// Main sketch file for the Pico W Robot project.
// This file initializes all the hardware modules and orchestrates their operation.

// Include the header files for our custom classes.

//#include "4MotorMotionCtrl.h"

#define MASTER_PICOW_4WD_NONSTEER_RUBBERWHL_RRXSR04_2xCLIFF
#include <Wire.h>
#include "E:\Jeevan\projects\PicoWCar\include-arduino\CarConfigurations.h" // sets e.g. CAR_HAS_ENCODERS, USE_ADAFRUIT_MOTOR_SHIELD
#include "E:\Jeevan\projects\PicoWCar\include-arduino\RobotCarPinDefinitionsAndMore.h" // Pinout depends on settings like CAR_HAS_ENCODERS etc.
//#include "PWMDcMotor.hpp"

vehstate Car;

vehdemandclass VMC;

SonarIRclass SonarIR;

// Requests SonarIR.data from a slave and returns true if CRC is valid
bool requestSonarIRData(uint8_t slaveAddr, SonarIRclass &data) {
    // Request the struct
    Wire.requestFrom(slaveAddr, (uint8_t)sizeof(SonarIRclass));

    // Check if enough bytes are available
    if (Wire.available() < sizeof(SonarIRclass)) {
        Serial.println("Not enough bytes available from slave");
        return false;
    }
    // Read full struct including CRC
    Wire.readBytes((uint8_t*)&data, sizeof(SonarIRclass));

    // Recompute CRC over all fields except crc
    uint8_t crc = 0;
    uint8_t* p = (uint8_t*)&data;
    for (size_t i = 0; i < sizeof(SonarIRclass) - 1; i++) crc ^= p[i];

    // Verify CRC
    if (crc != data.crc) {
      Serial.println("CRC mismatch!");
      return false;
    }

    // Data valid
    return true;
}

// =====================================================================
// === Global Variables and Class Instances ===
// =====================================================================

// Create instances of our custom classes.
//VehicleMotionRequester; //function to decide which way to move.
//VehicleMotionExec;//function/class to execute the motion. 
// PWMDcMotor motorFR, motorFL, motorRR, motorRL;
// UARTCommunicator comms;

// // A structure to hold all the data we want to send over UART.
// // This structure is defined in UARTCommunicator.h.
// RobotData robotData;

// =====================================================================
// === Setup Function ===
// =====================================================================

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Serial.print("start of setup");
  // // Initialize each of our hardware modules.
  // // The 'begin()' methods handle pin setup and any initial configuration.
  
  // comms.begin();           // Start serial communication first.
  // sensors.begin();         // Initialize the ultrasonic sensors.
  // motors.begin();          // Initialize the motor controllers and encoders.
  
  // You might want to add a small delay here to ensure all peripherals are ready.
  
  delay(100);
  Serial.print("end of setup");
}

// =====================================================================
// === Loop Function ===
// =====================================================================

void loop() {
  Serial.print("start of loop");
  // ReadSonarDist;
  // ReadCliffSens;
  if (requestSonarIRData(SLAVE_ARD_ADDR, receivedData)) {
        Serial.print("FrontDist: "); Serial.println(receivedData.frontDistance);
        Serial.print("RearDist: "); Serial.println(receivedData.rearDistance);
        Serial.print("FrontSonarQF: "); Serial.println(receivedData.frontSonarQF);
        Serial.print("RearSonarQF: "); Serial.println(receivedData.rearSonarQF);
        Serial.println("---");
  } 
  else {
        Serial.println("Failed to read valid data from slave");
  }
  //ReadIMU;  //all of these comms need to be non-blocking
  //ReadLidar;
  //ReadRadar;
  // ReadVehSpd;
  //SendDataOverUSBSerial;
  //ReceiveDataOverUSBSerial;
  //and now comes the real processing
  // VehMotionReq;
  // VehMotionExec;
  // ScreenDisp;
  
  // // === 1. Read Sensor Data ===
  // // Note: For motor encoders, a proper implementation would use interrupts
  // // to prevent missing pulses. This example uses a simplified polling method.
  // motors.readEncoders(robotData.motorEncoders);
  
  // // Read the distances from the front and back ultrasonic sensors.
  // robotData.frontDistance = sensors.getDistance(0);
  // robotData.backDistance = sensors.getDistance(1);
  
  // // === 2. Drive Motors (Example Logic) ===
  // // Replace this with your actual robot's control logic. For example,
  // // you might use the sensor data to avoid obstacles.
  
  // // Here, we just set a constant speed for a simple test.
  // // Replace this with your own logic to control the robot's movement.
  // motors.setSpeed(0, 100); // Set motor 0 to 100 speed
  // motors.setSpeed(1, 100); // Set motor 1 to 100 speed
  // motors.setSpeed(2, 100); // Set motor 2 to 100 speed
  // motors.setSpeed(3, 100); // Set motor 3 to 100 speed
  
  // // === 3. Send Data over UART ===
  // // Package and send all the sensor readings to your receiver (e.g., a PC).
  // comms.sendData(robotData);
  
  // A short delay to prevent the loop from running too fast.
  delay(100);
  Serial.print("end of loop");
}
