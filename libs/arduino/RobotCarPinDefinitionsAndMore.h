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
	#if defined(CAR_HAS_4_DCMOTORS_WAVESHARE)
	//DC motors driven by Waveshare 4 motor driver with PCA9685 and TB6612FNG
	//PCA9685 is on pins SDA 26, SCL 27 of Pico W
		#define MOTOR_FR 0 /*white A1, red A2*/
		#define MOTOR_RR 1 /*red B1 white B2*/
		#define MOTOR_FL 2  /*red C1 white C2*/
		#define MOTOR_RL 3	/*white D1 red D2*/
	#endif
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
		#define PIN_FRONT_CLIFF 5	//yellow at Pico, white at cliff sensor
		#define PIN_REAR_CLIFF 4	//green at Pico, white at cliff sensor
	#endif
	#if defined(CAR_HAS_SPI_DISPLAY)
	//SPI TX means MOSI
		#define MOSI 7
		#define MISO 8
		#define SCK 6
		#define CS0 9
	#endif
	#if defined(CAR_HAS_I2C)
		#define MASTER_PICOW_ADDR 0x08
		// #define SLAVE_ARDNANO_ADDR 0x09 
		// #define SERVO_CTRL_ADDR 0xA
		#define LIDAR_LD06_ADDR	0x29
		#define RADAR_LD2450_ADDR 0x62
		#define IMU_MPU6050_ADDR 0x68
		#define PICOW_I2C0_SDA 20	//also used for PCA9685
		#define PICOW_I2C0_SCL 21	//also used for PCA9685
    #define PICOW_JETSON_I2C1_RX 2 
		#define PICOW_JETSON_I2C1_TX 3 
		#define MOTOR_DRV_ADDR 0x40 //PCA9685 address for Waveshare motor driver
		// #define ARD_I2C_SDA A4
		// #define ARD_I2C_SCL A5
	#endif
	#if defined(CAR_HAS_UART)
		#define PICOW_LD2450RADAR_UART0_RX 0 
		#define PICOW_LD2450RADAR_UART0_TX 1
    #define JETSON_LD06_UART0_RX 10 
		#define JETSON_LD06_UART0_TX 8
		
	#endif
#endif
//system constants
static constexpr float kGearRatio = 46.0;
static constexpr uint32_t kLoopPeriodMs = 100;
static constexpr uint32_t kSerialBaud = 115200;
static constexpr uint32_t kSerialWaitMs = 500; // Wait up to 500ms for Serial to start
static constexpr uint32_t kPwmFreqHz = 1600;
static constexpr uint32_t kSonarPollIntervalMs = 50;
static constexpr uint32_t kSonarMaxWaitMs = 50; // Max wait per sonar reading. if its beyond, it should default to kMaxSonarRangecm
static constexpr uint32_t kWatchdogTimeoutMs = 2000; // 2 second watchdog timeout
static constexpr uint32_t kMotorSafetyTimeoutMs = 1000; // 1 second motor command timeout
static constexpr float kMaxSaneRpm = 200.0; // Max sane RPM for encoders
static constexpr int32_t kMaxSonarRangecm = 400; // Max range for sonar in m
static constexpr int32_t conv_M_TO_CM = 100; // Conversion factor from meters to centimeters
static constexpr int32_t min_sonar_delayMs = 25; //msec delay between reading from two sonars. otherwise there could be crosstalk. this is limiting the transmission rate from arduino to PicoW. This comes from (SONAR_MAX_DISTANCE*2/speed_of_sound in cm/ms) 
static constexpr int32_t PULSE_PER_REV = 12;    //for encoders 
static constexpr float RPM2RADPS =  0.10472;
static constexpr float GRAVITY = 	9.81; //m per sec2
static constexpr float WHEEL_RAD =  3;  //6 cm dia wheels
static constexpr float RADPS2MPS = 0.03; //same as wheel radius, since v = r*w
static constexpr float MOTOR_RPM_TO_CMPS = RPM2RADPS * WHEEL_RAD / kGearRatio;

/* class to define control of all 4 motors of an AWD car. It inherits from Adafruit's classfor PCA9685 driver. 
It adds a method to check for I2C ACK. It adds Encoder to read the individual speeds and store them. 
Also adds diagnostic states */
/*class PCA9685_AWDDriver {
private:
  Adafruit_PWMServoDriver pwm;
  Encoder encFR, encFL, encRR, encRL;
  uint32_t lastMotorCommandTime;
  
public:
  PCA9685_AWDDriver(uint8_t addr, TwoWire *theWire,
                     uint8_t frA, uint8_t frB,
                     uint8_t flA, uint8_t flB,
                     uint8_t rrA, uint8_t rrB,
                     uint8_t rlA, uint8_t rlB)
    : pwm(addr, theWire),
      encFR(frA, frB, PULSE_PER_REV),
      encFL(flA, flB, PULSE_PER_REV),
      encRR(rrA, rrB, PULSE_PER_REV),
      encRL(rlA, rlB, PULSE_PER_REV),
      lastMotorCommandTime(0) {}
  
  bool begin(uint16_t freqHz) {
    // PCA9685 init with ACK check...
  }
  
  void initEncoders() {
    encFR.init();
    encFL.init();
    encRR.init();
    encRL.init();
  }
  
  void enableEncoderInterrupts(void (*frA)(), void (*frB)(),
                               void (*flA)(), void (*flB)(),
                               void (*rrA)(), void (*rrB)(),
                               void (*rlA)(), void (*rlB)()) {
    encFR.enableInterrupts(frA, frB);
    encFL.enableInterrupts(flA, flB);
    encRR.enableInterrupts(rrA, rrB);
    encRL.enableInterrupts(rlA, rlB);
  }
  
  float getRPM(uint8_t motorNum) {
    switch(motorNum) {
      case 0: return encFR.getRPM();
      case 1: return encFL.getRPM();
      case 2: return encRR.getRPM();
      case 3: return encRL.getRPM();
      default: return 0.0F;
    }
  }
  
  bool isRPMSane(float rpm) {
    return (fabsf(rpm) <= kMaxSaneRpm);
  }
  
  void setMotor(uint8_t motorNum, float torqueCmd) {
    // Motor control logic...
    lastMotorCommandTime = millis();
  }
  
  uint32_t getLastCommandTime() {
    return lastMotorCommandTime;
  }
}; */
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
Else map the verb to the preset torques or enable/disable
*/