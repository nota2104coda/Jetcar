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

#if defined(MCU_PICOW)
	#if defined(CAR_HAS_4_DCMOTORS)
	//DC motors driven by Pico
		#define PIN_MOTOR_VPLUS_FRONT_RIGHT 4
		#define PIN_MOTOR_VMINUS_FRONT_RIGHT 5
		#define PIN_MOTOR_VPLUS_FRONT_LEFT 6
		#define PIN_MOTOR_VMINUS_FRONT_LEFT 7
		#define PIN_MOTOR_VPLUS_REAR_RIGHT 9
		#define PIN_MOTOR_VMINUS_REAR_RIGHT 10
		#define PIN_MOTOR_VPLUS_REAR_LEFT 11 
		#define PIN_MOTOR_VMINUS_REAR_LEFT 12
	#endif
	#if defined(CAR_HAS_4_MOTORENCODERS)
	//motor encoders read on Pico
		#define PIN_ENC_FRONT_RIGHT_Y 14
		#define PIN_ENC_FRONT_RIGHT_G 15
		#define PIN_ENC_FRONT_LEFT_Y 16
		#define PIN_ENC_FRONT_LEFT_G 17
		#define PIN_ENC_REAR_RIGHT_Y 19
		#define PIN_ENC_REAR_RIGHT_G 20
		#define PIN_ENC_REAR_LEFT_Y 26
		#define PIN_ENC_REAR_LEFT_G 27
	#endif
	#if defined(CAR_HAS_RR_SONAR) && defined(SLAVE_ARDNANO)
		#define PIN_TRIG_SONAR_REAR 
		#define PIN_ECHO_SONAR_REAR 		
		#define SONAR_MAX_DISTANCE 350 //cm max distance. anything beyond is clipped to max.
		#define MIN_SONAR_DELAY 25 //msec delay between reading from two sonars. otherwise there could be crosstalk. this is limiting the transmission rate from arduino to PicoW. This comes from (SONAR_MAX_DISTANCE*2/speed_of_sound in cm/ms) 
	#endif
	#if defined(CAR_HAS_FRONT_RR_CLIFF_SENSOR) && defined(SLAVE_ARDNANO)
		#define PIN_FRONT_CLIFF 6
		#define PIN_REAR_CLIFF 7
	#endif

	// #if defined(CAR_HAS_SPI_DISPLAY)
	// //use SPI0 on GP16-GP19 on Pico W
	// 	#define MOSI 25
	// 	#define MISO 21
	// 	#define SCK 24
	// 	#define CS0 22
	// #endif
	#if defined(CAR_HAS_I2C)
		#define MASTER_PICOW_ADDR 0x08
		#define SLAVE_ARDNANO_ADDR 0x09 
		#define SERVO_CTRL_ADDR 0xA
		#define LIDAR_ADDR	0x29
		#define LD2450_RADAR_ADDR 0x62
		#define MPU6050_ADDR 0x68
		#define PICOW_I2C1_SDA 31
		#define PICOW_I2C1_SCL 32
		#define ARD_I2C_SDA A4
		#define ARD_I2C_SCL A5
		#define PICOW_UART_RX 0 //GP0
		#define PICOW_UART_TX 1 //GP1 
	#endif
	#if defined(CAR_HAS_UART)
		#define PICOW_UART_RX 0 //GP0
		#define PICOW_UART_TX 1 //GP1
#endif

/*VL53L5X is 3.3V
MPU6050 is 
LD2450 needs 5V supply but 3.3V logic for I2C*/
//define the I2C broadcast format
struct __attribute__((packed)) SonarIRclass {
  short int frontDistance;
  bool frontSonarQF; //0 = poor, 1 = ok for all QFs
  short int rearDistance;
  bool rearSonarQF;
  bool frontCliffStatus;
  bool frontCliffQF;
  bool rearCliffStatus;
  bool rearCliffQF;
  uint8_t crc;
};

class vehstate {
  public:
    float vxActual = 0; 
    float vyActual = 0; //filtered velocity in x(front) and y(sideways) directions , cm/s
    float wFrontR = 0;
    float wFrontL = 0;
    float vRearR = 0;
    float vRearL = 0; //individual wheel speeds , rad/s
    float ax = 0;
    float ay = 0; //acceleration after all filtering etc, cm/s^2
    float wZ = 0;  //angular velocity in Z direction, rad/s
    float frontObsDist = 30;
    float rearObsDist = 30;  //front and rear obstacle distances
    bool frontcliff = false;
    bool rearcliff = false; //TRUE means cliff detected, FALSE means not detected

    void readUSdist();
    void readCliffsens();
    void readvXActual();
	void readvYActual();
	void readWhlAngSpdFR();
	void readWhlAngSpdFL();
	void readWhlAngSpdRR();
	void readWhlAngSpdRL();
};

class vehdemandclass {
  public:
    float vxReq = 0;
    float vyReq = 0;
    float frontObsDistReq = 30;
    float rearObsDistReq = 30;  //front and rear obstacle distances requested
    void motionReq ();  //to calculate demanded speed for vehicle, using vehstate struct values as input
    void motionExec();//to execute demands
};


