/*
 *  RobotCarPinDefinitionsAndMore.h
 *
 *  Contains motor pin definitions for direct motor control with PWM and a dual full bridge e.g. TB6612 or L298.
 *  Used for PWMMotorControl examples for various platforms.
 *
 *  Copyright (C) 2021-2024  Armin Joachimsmeyer
 *  armin.joachimsmeyer@gmail.com
 *
 *  This file is part of PWMMotorControl https://github.com/ArminJo/PWMMotorControl.
 *  This file is part of PWMMotorControl https://github.com/ArminJo/Arduino-RobotCar.
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

// ----------------------------------------------------------------------------
//  Board auto-detection helpers
// ----------------------------------------------------------------------------

/*
 * Pin mapping table for different platforms
 *
 * Platform           Left Motor                 Right Motor          Encoder
 *            Forward  Backward  PWM     Forward  Backward  PWM     Left  Right
 * ----------------------------------------------------------------------------
 * AVR (UNO)    9         8       6         4         7      5        3     2
 * Motor shield %         %       %         %         %      %        3     2
 * ESP32-CAM   14        15      13
 * Label for motor control connections on the L298N board
 *            IN1       IN2     ENA       IN4       IN3    ENB
 * Label for motor control connections on the TB6612 breakout board
 *           AIN1      AIN2    PWMA      BIN1      BIN2   PWMB
 *
 * Motor Control
 * PIN  I/O Function
 *   2  I   Right motor encoder interrupt input | Force use of US distance sensor if IR distance sensor is available | Line follower sensor left
 *   3  I   Left motor encoder interrupt input  | Distance tone feedback enable pin | Line follower sensor middle
 *   4  O   Right motor fwd     | Line follower sensor left
 *   5  O   Right motor PWM     | Line follower sensor middle
 *   6  O   Left motor PWM      | Line follower sensor right
 *   7  O   Right motor back    | Force use of US distance sensor enable pin
 *   8  O   Left motor fwd      | Distance tone feedback enable pin
 *   9  O/I Left motor back     | IR remote control signal in - on Adafruit Motor Shield marked as Servo Nr. 2
 *
 * PIN  I/O Function
 *  10  O   Servo for distance sensor - on Adafruit Motor Shield marked as Servo Nr. 1 | Line follower sensor right
 *  11  I/O IR remote control signal in | Servo for laser pan | Line follower sensor right
 *  12  O   Buzzer for Uno board | Servo for laser tilt
 *  13  O   Laser power
 *
 * PIN  I/O Function
 *  A0  O   US trigger (and echo in 1 pin US sensor mode) "URF 01 +" connector on the Arduino Sensor Shield
 *  A1  I   US echo on "URF 01 +" connector | IR distance if motor shield; requires no or 1 pin ultrasonic sensor if motor shield
 *  A2  I   VIN/11, 1MOhm to VIN, 100kOhm to ground - required for readVINVoltage(), camera supply control on NANO, IR in on Mecanum
 *  A3  I   IR distance | Buzzer on NANO
 *  A4  SDA I2C for motor shield | VL35L1X TOF sensor | MPU6050 accelerator and gyroscope
 *  A5  SCL I2C for motor shield | VL35L1X TOF sensor | MPU6050 accelerator and gyroscope
 *  A6  O   Only on NANO - IR distance
 *  A7  O   Only on NANO - VIN/11, 1MOhm to VIN, 100kOhm to ground
 */

#if defined(MCU_ESP32S3_40PIN)
// ---------------------------------------------------------------------------
// Example pin plan for ESP32-S3-Pico based builds
// ---------------------------------------------------------------------------
  #if !defined(CAR_HAS_4_DCMOTORS_WAVESHARE)
    #define CAR_HAS_4_DCMOTORS_WAVESHARE
  #endif
  #define MOTOR_FR 0
  #define MOTOR_RR 1
  #define MOTOR_FL 2
  #define MOTOR_RL 3

  #if defined(CAR_HAS_4_MOTORENCODERS)
    #define PIN_ENC_FRONT_RIGHT_Y 1   // PIN24
    #define PIN_ENC_FRONT_RIGHT_G 2   // PIN25
    #define PIN_ENC_FRONT_LEFT_G 37    // PIN16
    #define PIN_ENC_FRONT_LEFT_Y 38   // PIN17
    #define PIN_ENC_REAR_RIGHT_Y 42   // PIN21
    #define PIN_ENC_REAR_RIGHT_G 41   // PIN22
    #define PIN_ENC_REAR_LEFT_G 39    // PIN19
    #define PIN_ENC_REAR_LEFT_Y 40     // PIN20
  #endif

  #if defined(CAR_HAS_FRT_RR_SONAR)
    #define PIN_TRIG_SONAR_FRONT 7   // PIN31
    #define PIN_ECHO_SONAR_FRONT 8   // PIN32
    #define PIN_TRIG_SONAR_REAR 36    // PIN11
    #define PIN_ECHO_SONAR_REAR 35    // PIN10
  #endif

  #if defined(CAR_HAS_FRONT_RR_CLIFF_SENSOR)
    #define PIN_FRONT_CLIFF 15        // PIN6
    #define PIN_REAR_CLIFF 16         // PIN7
  #endif

  #if defined(CAR_HAS_SPI_DISPLAY)
    #define ESP32_SPI_MOSI 35
    #define ESP32_SPI_MISO 36
    #define ESP32_SPI_SCK 37
    #define ESP32_SPI_CS0 38
    #define MOSI ESP32_SPI_MOSI
    #define MISO ESP32_SPI_MISO
    #define SCK  ESP32_SPI_SCK
    #define CS0  ESP32_SPI_CS0
  #endif

  #if defined(CAR_HAS_MPU6050_IMU)
    #define MCU_I2C0_SDA 4  //PIN26
    #define MCU_I2C0_SCL 5  //PIN27
  #endif
  
  #define MCU_JETSON_UART1_RX 14 //PIN5 to JETSON USB
  #define MCU_JETSON_UART1_TX 13 //PIN4

  #if defined(CAR_HAS_LD2450RADAR)
    #define MCU_LD2450RADAR_UART0_RX 11 //PIN1
    #define MCU_LD2450RADAR_UART0_TX 12 //PIN2
  #endif

  #if defined(CAR_HAS_LD06_LIDAR)
    #define JETSON_LD06_UART0_RX 10 //JETSPIN10
  #endif

#elif defined(MCU_PICOW_RP2040)
	//PCA9685 is on pins SDA 26, SCL 27 of Pico W
  #if defined(CAR_HAS_4_DCMOTORS_WAVESHARE)
    #define CAR_HAS_4_DCMOTORS_WAVESHARE
  #endif
	//DC motors driven by Waveshare 4 motor driver with PCA9685 and TB6612FNG
  #define MOTOR_FR 0 /*white A1, red A2*/
  #define MOTOR_RR 1 /*red B1 white B2*/
  #define MOTOR_FL 2  /*red C1 white C2*/
  #define MOTOR_RL 3	/*white D1 red D2*/
	
	#if defined(CAR_HAS_4_MOTORENCODERS)
	//motor encoders read on Pico
		#define PIN_ENC_FRONT_RIGHT_Y 18
		#define PIN_ENC_FRONT_RIGHT_G 19
		#define PIN_ENC_FRONT_LEFT_G 12
		#define PIN_ENC_FRONT_LEFT_Y 13
		#define PIN_ENC_REAR_RIGHT_Y 16
		#define PIN_ENC_REAR_RIGHT_G 17
		#define PIN_ENC_REAR_LEFT_G 14
		#define PIN_ENC_REAR_LEFT_Y 15
	#endif
	#if defined(CAR_HAS_FRT_RR_SONAR) 		
		#define PIN_TRIG_SONAR_FRONT 26	//white at Pico, grey at US sensor, 
		#define PIN_ECHO_SONAR_FRONT 27	//grey at Pico, brown at US sensor, 	
		#define PIN_TRIG_SONAR_REAR 11  //purple at Pico, blue at US sensor	
		#define PIN_ECHO_SONAR_REAR 10	//blue at pico, brown at US sensor	
		
	#endif
	#if defined(CAR_HAS_FRONT_RR_CLIFF_SENSOR) 
		#define PIN_FRONT_CLIFF 4	//yellow at Pico, white at cliff sensor
		#define PIN_REAR_CLIFF 5	//green at Pico, white at cliff sensor
	#endif
	#if defined(CAR_HAS_SPI_DISPLAY)
	//SPI TX means MOSI
		#define MOSI 7
		#define MISO 8
		#define SCK 6
		#define CS0 9
	#endif
	#if defined(CAR_HAS_I2C)
		#define MCU_I2C0_SDA 20	//also used for PCA9685
		#define MCU_I2C0_SCL 21	//also used for PCA9685
    #define MCU_JETSON_I2C1_SDA 2 
		#define MCU_JETSON_I2C1_SCL 3 
    
	#endif
	#if defined(CAR_HAS_UART)
		#define MCU_LD2450RADAR_UART0_RX 0 
		#define MCU_LD2450RADAR_UART0_TX 1
    #define JETSON_LD06_UART0_RX 10 
		#define JETSON_LD06_UART0_TX 8	
    #define MCU_JETSON_UART1_RX 1
    #define MCU_JETSON_UART1_TX 0
//     1,GP0,43,UART0 TX
// 2,GP1,44,UART0 RX
	#endif
#endif
#ifndef ROBOTCAR_PLATFORM_CONSTANTS_DEFINED
  #define ROBOTCAR_PLATFORM_CONSTANTS_DEFINED
  //system constants
  static constexpr float kGearRatio = 46.0;
  static constexpr uint32_t kLoopPeriodMs = 20; // 50Hz control and telemetry rate
  static constexpr uint32_t kSerialBaud = 115200;
  static constexpr uint32_t kSerialWaitMs = 500; // Wait up to 500ms for Serial to start
  static constexpr uint32_t kPwmFreqHz = 1600;
  static constexpr uint32_t kSonarPollIntervalMs = 200;
  static constexpr uint32_t kSonarMaxWaitMs = 50; // Max wait per sonar reading. if its beyond, it should default to kMaxSonarRangecm
  static constexpr uint32_t kWatchdogTimeoutMs = 2000; // 2 second watchdog timeout
  static constexpr uint32_t kMotorSafetyTimeoutMs = 1000; // 1 second motor command timeout
  static constexpr float kMaxSaneRpm = 200.0; // Max sane RPM for encoders
  static constexpr int16_t kMinSonarRangecm = 10; // Min range for sonar in m
  static constexpr int16_t kMaxSonarRangecm = 400; // Max range for sonar in m
  static constexpr int32_t CONV_M_TO_CM = 100; // Conversion factor from meters to centimeters
  static constexpr float CONV_CM_TO_M = 0.01; // Conversion factor from centimeters to meters
  static constexpr int32_t min_sonar_delayMs = 25; //msec delay between reading from two sonars. otherwise there could be crosstalk. this is limiting the transmission rate from arduino to PicoW. This comes from (SONAR_MAX_DISTANCE*2/speed_of_sound in cm/ms) 
  static constexpr int32_t PULSE_PER_REV = 12;    //for encoders 
  static constexpr float RPM_TO_RADPS =  0.10472;
  static constexpr float GRAVITY = 	9.81; //m per sec2

#endif

#if defined(CAR_HAS_I2C)
  #define I2C_MASTER_MCU_ADDR 0x08
  #define I2C_MASTER_JETSON_ADDR 0x09
  #define I2C_SLAVE_MCU_ADDR 0x42
  #define LIDAR_LD06_ADDR	0x29
  #define RADAR_LD2450_ADDR 0x62
  #define IMU_MPU6050_ADDR 0x68
  #define MOTOR_DRV_ADDR 0x40 //PCA9685 address for Waveshare motor driver
#endif

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

#if LOOP_DEBUG_I2C2
  #define DEBUG_I2C2_PRINT(...) Serial.print(__VA_ARGS__); Serial.flush()
  #define DEBUG_I2C2_PRINTLN(...) Serial.println(__VA_ARGS__); Serial.flush()
#else
  #define DEBUG_I2C2_PRINT(...) ((void)0)
  #define DEBUG_I2C2_PRINTLN(...) ((void)0)
#endif



//UART SCHEMA
/*Line protocol (ASCII, newline-delimited)

Telemetry (outgoing from Pico): prefix TEL
TEL,<ts_ms>,<ax>,<ay>,<az>,<gx>,<gy>,<gz>,<temp_c>,<sonar_f_cm>,<sonar_r_cm>,<cliff_f>,<cliff_r>,<spdFL>,<spdFR>,<spdRL>,<spdRR>
Example: TEL,12345,0.01,-0.02,9.78,0.00,0.01,0.00,24.5,120,255,0,1,12.3,12.0,11.8,11.9
Command (incoming to Pico): prefix CMD
Predefined verbs: CMD,forward | CMD,backward | CMD,left | CMD,right | CMD,stop | CMD,enable | CMD,disable
Direct torque command: CMD,set,<tqFR>,<tqFL>,<tqRR>,<tqRL>
Example: CMD,set,0.3,0.3,0.3,0.3
Parsing rules

Fields are comma-separated, \n terminated.
Floats in decimal; booleans as 0/1; integers for timestamps and sonar/cliff.
Ignore/skip lines that don’t start with the expected prefix.
For robustness, cap torques to safe limits after parsing.
Integration points

Telemetry emit: replace sendTelemetry body to print the TEL,... line once per loop interval.
Command ingest: adjust process_uart_commands to:
Read a line
If it starts with CMD,set, parse four floats
Else map the verb to the preset torques or enable/disable */
