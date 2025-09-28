/*
 *  CarConfigurations.h
 *
 *  Contains a few predefined set of car configurations
 *
 *  This file is part e:\Jeevan\projects\PicoWCar\pico\pico-arduinoIDE\picoW-robot-master\RobotCarPinDefinitionsAndMore.hof PWMMotorControl https://github.com/ArminJo/PWMMotorControl.
 *  This file is part of Arduino-RobotCar https://github.com/ArminJo/Arduino-RobotCar.
 *
 *  PWMMotorControl and Arduino-RobotCar are free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/gpl.html>.
 *
 */

#ifndef _ROBOT_CAR_CONFIGURATIONS_H
#define _ROBOT_CAR_CONFIGURATIONS_H

/*
 * This is the available set of predefined configurations of RobotCarConfigurations.h
 * All configurations (except MECANUM_BASIC_CONFIGURATION) include a HC-SR04 ultrasonic distance sensor mounted on a pan servo.
 */
//////////////////////////////////////////////////////
//#define 
//#define PICOW_4WD_NONSTEER_RUBBERWHL_2XSR04_2xCLIFF           // my current set
//https://github.com/ArminJo/ESP32-Cam-Sewer-inspection-car
//////////////////////////////////////////////////////
//
/*
 * Distinct parameters for car control boards, sensors and extensions
 */
//#define CAR_HAS_VIN_VOLTAGE_DIVIDER     // VIN/11 at A2, e.g. 1MOhm to VIN, 100kOhm to ground. To show and monitor VIN voltage. Can be dynamically detected.
//#define VIN_VOLTAGE_CORRECTION 0.81     // Correction for the series SI-diode in the VIN line of the Uno board.
//#define DISTANCE_SERVO_IS_MOUNTED_HEAD_DOWN // Activate this, if the distance servo is mounted head down to detect small obstacles.
//#define CAR_HAS_US_DISTANCE_SENSOR      // A HC-SR04 ultrasonic distance sensor is mounted (default for most China smart cars).
//#define CAR_HAS_US_DISTANCE_SENSOR_FRONT      // A HC-SR04 ultrasonic distance sensor is mounted (default for most China smart cars).
//#define CAR_HAS_US_DISTANCE_SENSOR_REAR      // A HC-SR04 ultrasonic distance sensor is mounted (default for most China smart cars).
//#define CAR_HAS_IR_CLIFF_SENSOR_FRONT      // A cliff IR sensor at front
//#define CAR_HAS_IR_CLIFF_SENSOR_REAR      // A cliff IR sensor at front //#define CAR_HAS_TOF_DISTANCE_SENSOR     // A VL53L1X TimeOfFlight distance sensor is mounted.
//#define CAR_HAS_ENCODERS                // 2 slot type slot-type photo interrupter are mounted and connected with pin 2 and 3.
//#define CAR_HAS_MPU6050_IMU             // A MPU6050 breakout board like GY-521 is mounted and connected by I2C.
//
// For pan tilt we have 2 servos in total
//#define CAR_HAS_PAN_SERVO
//#define CAR_HAS_TILT_SERVO
//#define CAR_HAS_CAMERA
//#define CAR_HAS_LASER
//#define CAR_HAS_8x8LASER
//
/*
 * Parameters for PWMMotorControl
 */
//#define USE_ENCODER_MOTOR_CONTROL       // Use encoder interrupts attached at pin 2 and 3 and want to use the methods of the EncoderMotor class.
//#define USE_ADAFRUIT_MOTOR_SHIELD       // Use Adafruit Motor Shield v2 with 2xTB6612 connected by I2C instead of external TB6612 or L298 breakout board.
//#define USE_MPU6050_IMU                 // Use GY-521 MPU6050 breakout board connected by I2C for support of precise turning. Connectors point to the rear.
//#define VIN_2_LI_ION                    // Activate this, if you use 2 Li-ion cells (around 7.4 volt) as motor supply.
//#define VIN_1_LI_ION                    // If you use a mosfet bridge (TB6612), 1 Li-ion cell (around 3.7 volt) may be sufficient.
//#define FULL_BRIDGE_INPUT_MILLIVOLT 6000// Default. For 4 x AA batteries (6 volt).
//#define USE_L298_BRIDGE                 // Activate this, if you use a L298 bridge, which has higher losses than a recommended mosfet bridge like TB6612.
//#define DEFAULT_DRIVE_MILLIVOLT   2000  // Drive voltage / motors default speed. Default value is 2.0 volt.
//#define DO_NOT_SUPPORT_RAMP             // Ramps are anyway not used if drive speed voltage (default 2.0 V) is below 2.3 V. Saves 378 bytes program memory.
//#define DO_NOT_SUPPORT_AVERAGE_SPEED    // Disables the encoder function getAverageSpeed(). Saves 44 bytes RAM per motor and 156 bytes program memory.
/*
 * Parameters for CarPWMMotorControl
 */
//#define CAR_HAS_4_WHEELS
//#define CAR_HAS_4_MECANUM_WHEELS  // implies CAR_HAS_4_WHEELS
/*
 * Disable features of RobotCarBlueDisplay to save program memory
 */
//#define NO_PATH_INFO_PAGE         // Saves up to 1400 bytes - is enabled by default
//#define NO_SERIAL_OUTPUT          // Saves up to 2886 bytes - mainly in BlueDisplay library
//#define NO_RTTTL_FOR_CAR          // Saves up to 3654 bytes
/**************************
 * START OF CONFIGURATIONS
 **************************/
/*
 * PICO W basic + 4x encoder 370 motors + rubber wheels + non steered + Pico motor driver+ 2x HCSR04 sensors front/rear + 2x IR cliff sensors 
 * + 5S2P NiMH battery 6V for motors/Pico
 */
#if defined(MASTER_PICOW_SLAVE_ARD_4WD_NONSTEER_RUBBERWHL_RRXSR04_2xCLIFF)
#define MCU_PICOW	//RP2040W
#define SLAVE_ARDNANO    //Nano
#define CAR_HAS_4_DCMOTORS	//
//#define CAR_HAS_UART
#define CAR_HAS_I2C
#define CAR_HAS_4_MOTORENCODERS	//
#define CAR_HAS_RR_SONAR // Activate this if your car has two HCSR04 US distance sensors one at front and at rr
#define CAR_HAS_FRONT_RR_CLIFF_SENSOR // Activate this if your car has IR cliff sensor fr and rr
#define RPM2RADPS 0.10472
#define GRAVITY	981 //cm per sec
#define WHEEL_RAD 3  //6 cm dia
#define RADPS2CMPS 3 //same as wheel radius
#define GEARRATIO 46  //ratio of motor speed to wheel speed
#define MOTOR_RPM_2_CMPS (RPM2RADPS * WHEEL_RAD / GEARRATIO)
#define NO_RTTTL_FOR_CAR                // Saves up to 3654 bytes
#define CONFIG_NAME         "4WD rubber wheel + PICOW master + 5S2P NiMH + IR cliff + US distance" // BASIC_CONFIG_NAME and CONFIG_NAME is printed by printConfigInfo()
#endif


/****************************************************************
 *          B A S I C   C O N F I G U R A T I O N S
 *
 * USE_ADAFRUIT_MOTOR_SHIELD implies 2 Li-ion power supply
 * and servo mounted head down
 ****************************************************************/

/*
 * BASIC CONFIGURATION for full breadboard
 * Nano Breadboard version with Arduino NANO, without shield and with pan/tilt servo and MPU camera and laser
 */
#if defined(BREADBOARD_4WD_FULL_CONFIGURATION)
#define CAR_IS_NANO_BASED               // We have an Arduino NANO instead of an Uno resulting in a different pin layout.
#define CAR_HAS_4_WHEELS
#define CAR_HAS_US_DISTANCE_SENSOR      // A HC-SR04 ultrasonic distance sensor is mounted (default for most China smart cars)
#define CAR_HAS_DISTANCE_SERVO          // Distance sensor is mounted on a pan servo (default for most China smart cars)
#define CAR_HAS_PAN_SERVO
#define CAR_HAS_TILT_SERVO
#define CAR_HAS_CAMERA
#define CAR_HAS_LASER
#define VIN_2_LI_ION                    // Activate this, if you use 2 Li-ion cells (around 7.4 volt) as motor supply.
#define VIN_VOLTAGE_CORRECTION 0.81     // Correction for the series SI-diode in the VIN line of the Uno board
#define CAR_HAS_VIN_VOLTAGE_DIVIDER     // VIN/11 at A2, e.g. 1MOhm to VIN, 100kOhm to ground. Required to show and monitor (for undervoltage) VIN voltage.
#define CAR_HAS_ENCODERS                // Activate this if you have encoder interrupts attached at pin 2 and 3 and want to use the methods of the EncoderMotor class.
#define CAR_HAS_MPU6050_IMU             // Use GY-521 MPU6050 breakout board connected by I2C for support of precise turning. Connectors point to the rear.
#define DISTANCE_SERVO_IS_MOUNTED_HEAD_DOWN // Activate this, if the distance servo is mounted head down to detect small obstacles.
#define NO_PATH_INFO_PAGE               // Saves up to 1400 bytes
#define NO_SERIAL_OUTPUT                // Saves up to 2886 bytes
#define NO_RTTTL_FOR_CAR                // Saves up to 3654 bytes
#define BASIC_CONFIG_NAME   "4WD + Breadboard TB6612  + 2 Li-ion + VIN divider + servo head down + MPU6050"
#endif

/*
 * BASIC CONFIGURATION for MECANUM car
 * Nano Breadboard version with Arduino NANO, TB6612 mosfet bridge and 4 mecanum wheels
 */
#if defined(MECANUM_BASIC_CONFIGURATION)
#define CAR_IS_NANO_BASED               // We have an Arduino NANO instead of an Uno resulting in a different pin layout.
#define CAR_HAS_4_MECANUM_WHEELS
#define VIN_2_LI_ION                    // Activate this, if you use 2 Li-ion cells (around 7.4 volt) as motor supply.
#define CAR_HAS_VIN_VOLTAGE_DIVIDER     // VIN/11 at A2, e.g. 1MOhm to VIN, 100kOhm to ground. Required to show and monitor (for undervoltage) VIN voltage.
#define BASIC_CONFIG_NAME   "Breadboard TB6612  + 2 Li-ion + VIN divider + 4 mecanum wheels"
#endif

/*
 * BASIC CONFIGURATION for ESP32-Cam car
 * Car controlled by an ESP32-Cam module
 */
#if defined(ESP32) && !defined(CAR_IS_ESP32_CAM_BASED) // a temporarily hack
#define CAR_IS_ESP32_CAM_BASED
#endif
#if defined(CAR_IS_ESP32_CAM_BASED)
#define CAR_HAS_US_DISTANCE_SENSOR      // A HC-SR04 ultrasonic distance sensor is mounted (default for most China smart cars)
#define CAR_HAS_DISTANCE_SERVO          // Distance sensor is mounted on a pan servo (default for most China smart cars)
#define BASIC_CONFIG_NAME   "ESP32-Cam based"
#endif

/************************************
 * MOTOR_SHIELD basic configurations
 ************************************/
/*
 * BASIC CONFIGURATION for 4WD Motor Shield car
 * The basic layout of my red smart 4WD robot car with 2 LiPo's instead of 4 AA
 * Shield + 2 LiPo's + VIN voltage divider + servo head down
 */
#if defined(MOTOR_SHIELD_4WD_BASIC_CONFIGURATION)
#define CAR_HAS_4_WHEELS
#define MOTOR_SHIELD_2WD_BASIC_CONFIGURATION // Use further settings of this configuration
#define BASIC_CONFIG_NAME   "4WD + Motor shield,TB6612  + 2 Li-ion + VIN divider + servo head down"
#endif


/*
 * Some rules
 */
#if defined(CAR_HAS_4_MECANUM_WHEELS)
#define CAR_HAS_4_WHEELS
#endif

#if defined(CAR_HAS_US_DISTANCE_SENSOR) || defined(CAR_HAS_IR_DISTANCE_SENSOR) || defined(CAR_HAS_TOF_DISTANCE_SENSOR)
#define CAR_HAS_DISTANCE_SENSOR         // At least one distance sensor mounted on a pan servo is available
#endif

#if defined(CAR_HAS_DISTANCE_SERVO) || defined(CAR_HAS_PAN_SERVO) || defined(CAR_HAS_TILT_SERVO)
#define CAR_HAS_SERVO                   // At least one servo is mounted on the car
#endif

#endif // _ROBOT_CAR_CONFIGURATIONS_H
