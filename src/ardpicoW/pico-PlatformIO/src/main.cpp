
#include <cstdint>
#include <SimpleFOC.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <NewPing.h>
#include <hardware/watchdog.h>

#define MASTER_PICOW_4WD_NONSTEER_RUBBERWHL_2XSONAR_2xCLIFF
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>

//follow metric system everywhere. all distances in m, speeds m/s, acceleration m/s^2, angles in rad, angular velocity in rad/s

// System constants
static constexpr float kGearRatio = 46.0;
static constexpr uint32_t kLoopPeriodMs = 100;
static constexpr uint32_t kSerialBaud = 115200;
static constexpr uint32_t kSerialWaitMs = 500; // Wait up to 500ms for Serial to start
static constexpr uint32_t kPwmFreqHz = 1600;
static constexpr uint32_t kSonarPollIntervalMs = 500;
static constexpr uint32_t kSonarMaxWaitMs = 50; // Max wait per sonar reading. if its beyond, it should default to kMaxSonarRangem
static constexpr uint32_t kWatchdogTimeoutMs = 2000; // 2 second watchdog timeout
static constexpr uint32_t kMotorSafetyTimeoutMs = 1000; // 1 second motor command timeout
static constexpr float kMaxSaneRpm = 200.0; // Max sane RPM for encoders
static constexpr int32_t kMaxSonarRangem = 4; // Max range for sonar in m
static constexpr int32_t conv_M_TO_CM = 100; // Conversion factor from meters to centimeters
static constexpr int32_t min_sonar_delayMs = 25; //msec delay between reading from two sonars. otherwise there could be crosstalk. this is limiting the transmission rate from arduino to PicoW. This comes from (SONAR_MAX_DISTANCE*2/speed_of_sound in cm/ms) 
static constexpr int32_t PULSE_PER_REV = 12;    //for encoders 
static constexpr float RPM2RADPS =  0.10472;
static constexpr float GRAVITY = 	9.81; //m per sec2
static constexpr float WHEEL_RAD =  0.03;  //6 cm dia wheels
static constexpr float RADPS2MPS = 0.03; //same as wheel radius, since v = r*w
static constexpr float MOTOR_RPM_TO_MPS = (RPM2RADPS * WHEEL_RAD / kGearRatio);

// System state machine
enum class SystemState : uint8_t {
  INIT = 0,
  RUNNING = 1,
  DEGRADED = 2,  // Running without IMU
  ERROR = 3
};

#include "../../../../libs/arduino/CarConfigurations.h"
#include "../../../../libs/arduino/RobotCarPinDefinitionsAndMore.h"
#include "CliffSensor.h"
#include "PCA9685_AWDDriver.h"

CliffSensor frontCliff(PIN_FRONT_CLIFF);
CliffSensor rearCliff(PIN_REAR_CLIFF);

// NewPing sonarF(PIN_TRIG_SONAR_FRONT, PIN_ECHO_SONAR_FRONT, kMaxSonarRangem*conv_M_TO_CM); //the lib needs cm as max range
NewPing sonarR(PIN_TRIG_SONAR_REAR, PIN_ECHO_SONAR_REAR, kMaxSonarRangem*conv_M_TO_CM);

uint32_t lastPublish = 0;
int32_t sonarDistanceFront = 5;
int32_t sonarDistanceRear = 7;
uint32_t lastSonarFrontPoll = 0;
uint32_t lastSonarRearPoll = 0;
uint32_t lastMotorCommandTime = 0;
SystemState systemState = SystemState::INIT;
bool mpuAvailable = false;

// I2C and peripherals
arduino::MbedI2C picomasteri2c(PICOW_I2C0_SDA, PICOW_I2C0_SCL);
Adafruit_MPU6050 mpu;

// motorDriver will be created in setup() after I2C.begin() to avoid early I2C access
PCA9685_AWDDriver *motorDriver = nullptr;


void setup() {
  // Concept 5: Enable hardware watchdog (2 second timeout). Dont see how this helps. what conditions should I reboot
  // TEMPORARILY DISABLED FOR DEBUGGING
  // watchdog_enable(kWatchdogTimeoutMs, true);

  // Concept 1: Timeout on Serial connection
  Serial.begin(kSerialBaud);
  delay(500);  // Give serial extra time
  const uint32_t serialStart = millis();
  while ((!Serial) && ((millis() - serialStart) < kSerialWaitMs)) {
    delay(10);
    // watchdog_update();  // Disabled for debugging
  }

  Serial.println();
  Serial.println("==========================================");
  Serial.println("[DEBUG] Pico W BOOT - Watchdog DISABLED");
  Serial.flush();

  Serial.println("[INIT] Pico W starting...");
  systemState = SystemState::INIT;

  Serial.println("[INIT] Initializing cliff sensors...");
  frontCliff.init();
  rearCliff.init();
  Serial.println("[INIT] Cliff sensors initialized.");

  Serial.println("[INIT] I2C bus and motor driver...");
  picomasteri2c.begin();
  
  // Create motorDriver AFTER I2C is initialized
  motorDriver = new PCA9685_AWDDriver(
    MOTOR_DRV_ADDR, &picomasteri2c,
    PIN_ENC_FRONT_RIGHT_Y, PIN_ENC_FRONT_RIGHT_G,
    PIN_ENC_FRONT_LEFT_G, PIN_ENC_FRONT_LEFT_Y,
    PIN_ENC_REAR_RIGHT_Y, PIN_ENC_REAR_RIGHT_G,
    PIN_ENC_REAR_LEFT_G, PIN_ENC_REAR_LEFT_Y,
    PULSE_PER_REV, kMaxSaneRpm
  );
  
  // Check PCA9685 presence
  picomasteri2c.beginTransmission(MOTOR_DRV_ADDR);
  int ackStatus = picomasteri2c.endTransmission();
  if (ackStatus != 0) {
    Serial.println("[ERROR] PCA9685 not responding on I2C");
    systemState = SystemState::ERROR;
  }

  // Initialize motor driver (PCA9685 + encoders)
  Serial.println("[INIT] Initializing motor driver and encoders...");
  motorDriver->begin(kPwmFreqHz);
  Serial.println("[INIT] Motor driver and encoders initialized.");
  

  // Concept 3 & 4: Graceful MPU6050 failure handling (no infinite loop)
  if (!mpu.begin(MPU6050_I2CADDR_DEFAULT, &picomasteri2c)) {
    delay(2000);
    Serial.println("[WARN] MPU6050 not detected. Running in DEGRADED mode without IMU.");
    mpuAvailable = false;
    systemState = SystemState::DEGRADED;
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("[INIT] MPU6050 ready.");
    mpuAvailable = true;
    systemState = SystemState::RUNNING;
  }

  Serial.println("[INIT] System ready. Streaming data every 100ms.");
  lastPublish = millis();
  lastMotorCommandTime = millis();

  
}

void loop() {
  // Concept 5: Feed the watchdog to prevent reset
  // watchdog_update();  // Disabled for debugging

  const uint32_t currentTime = millis();
  
  // Check motor safety timeouts (non-blocking)
  if (motorDriver && motorDriver->checkMotorSafetyTimeouts(kMotorSafetyTimeoutMs)) {
    if (systemState == SystemState::RUNNING || systemState == SystemState::DEGRADED) {
      Serial.println("[SAFETY] Motor timeout - stopped inactive motors");
    }
  }
  
  // Non-blocking sonar polling with 50ms timeout per sensor
  delay(50); //delay to avoid crosstalk between two sonars
  const uint32_t sonarRearStart = millis();
  if ((millis() - lastSonarRearPoll) >= kSonarPollIntervalMs) {
    Serial.println("[DEBUG] Rear sonar poll triggered");
    uint32_t attempts = 0;
    while ((millis() - sonarRearStart) < kSonarMaxWaitMs) {
      const int32_t reading = sonarR.ping_cm();
      attempts++;
      Serial.print("[DEBUG] Rear reading: ");
      Serial.print(reading);
      Serial.print(" cm (attempt ");
      Serial.print(attempts);
      Serial.println(")");
      // Concept 2: Validate sonar reading is in sane range
      if ((reading > 0) && (reading < (kMaxSonarRangem * conv_M_TO_CM))) {
        sonarDistanceRear = reading;
        lastSonarRearPoll = millis();
        Serial.println("[DEBUG] Rear sonar valid reading captured");
        break;
      }
    }
    if (attempts > 0 && sonarDistanceRear == 7) {
      Serial.println("[DEBUG] Rear sonar: no valid reading after attempts");
    }
  }

  // Motor control and sensor reading
  float rpmFR = 0.0F, rpmFL = 0.0F, rpmRR = 0.0F, rpmRL = 0.0F;
  if (motorDriver) {
    motorDriver->setMotor(MOTOR_FR, 0.3F); // Front Right
    motorDriver->setMotor(MOTOR_FL, 0.3F); // Front Left
    motorDriver->setMotor(MOTOR_RR, 0.3F); // Rear Right
    motorDriver->setMotor(MOTOR_RL, 0.3F); // Rear Left
    delay(100); // Give encoders time to accumulate counts
    rpmFR = motorDriver->getRPM(MOTOR_FR);
    rpmFL = motorDriver->getRPM(MOTOR_FL);
    rpmRR = motorDriver->getRPM(MOTOR_RR);
    rpmRL = motorDriver->getRPM(MOTOR_RL);
  }
  
  Serial.print(">rpmFR:");
  Serial.println(rpmFR, 1);
  Serial.print(">rpmFL:");
  Serial.println(rpmFL, 1);
  Serial.print(">rpmRR:");
  Serial.println(rpmRR, 1);
  Serial.print(">rpmRL:");
  Serial.println(rpmRL, 1);

  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;

  // Concept 3: Only read IMU if available
  if (mpuAvailable) {
    mpu.getEvent(&accel, &gyro, &temp);
  } 
  else {
    // Safe defaults when IMU unavailable
    accel.acceleration.x = 0.0F;
    accel.acceleration.y = 0.0F;
    accel.acceleration.z = 0.0F;
    gyro.gyro.x = 0.0F;
    gyro.gyro.y = 0.0F;
    gyro.gyro.z = 0.0F;
    temp.temperature = 0.0F;
  }

  Serial.print(">accel_x:");
  Serial.println(accel.acceleration.x, 2);
  Serial.print(">accel_y:");
  Serial.println(accel.acceleration.y, 2);
  Serial.print(">accel_z:");
  Serial.println(accel.acceleration.z, 2);

  Serial.print(">gyro_x:");
  Serial.println(gyro.gyro.x, 2);
  Serial.print(">gyro_y:");
  Serial.println(gyro.gyro.y, 2);
  Serial.print(">gyro_z:");
  Serial.println(gyro.gyro.z, 2);

  Serial.print(">temp_c:");
  Serial.println(temp.temperature, 2);
 
  Serial.print(">sonar_rear_cm:");
  Serial.println(sonarDistanceRear);

  // Read and print cliff sensors
  frontCliff.read();
  rearCliff.read();
  Serial.print(">cliff_front:");
  Serial.println(frontCliff.getLastState() ? 1 : 0);
  Serial.print(">cliff_rear:");
  Serial.println(rearCliff.getLastState() ? 1 : 0);

  lastPublish = currentTime;
}
