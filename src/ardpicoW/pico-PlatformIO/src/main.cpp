#include <cstdint>
#include <SimpleFOC.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <NewPing.h>
#include <hardware/watchdog.h>


#define MASTER_PICOW_4WD_NONSTEER_RUBBERWHL_2XSONAR_2xCLIFF

// Set to 1 to enable loop debug output, 0 to disable. Ralph S Bacon from Youtube solution
#define LOOP_DEBUG_A 0
#define LOOP_DEBUG_B 0
#define LOOP_DEBUG_UART 1

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

#if LOOP_DEBUG_UART
  #define DEBUG_UART_PRINT(...) Serial.print(__VA_ARGS__); Serial.flush()
  #define DEBUG_UART_PRINTLN(...) Serial.println(__VA_ARGS__); Serial.flush()
#else
  #define DEBUG_UART_PRINT(...) ((void)0)
  #define DEBUG_UART_PRINTLN(...) ((void)0)
#endif

#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <Arduino.h>

//follow metric system everywhere. all distances in m, speeds m/s, acceleration m/s^2, angles in rad, angular velocity in rad/s

// System constants
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

// System state machine
enum class SystemState : uint8_t {
  INIT = 0,
  RUNNING = 1,
  DEGRADED = 2,  // Running without IMU
  ERROR = 3
};

#include "/home/jeevan/PicoWCar/libs/arduino/CarConfigurations.h"
#include "/home/jeevan/PicoWCar/libs/arduino/RobotCarPinDefinitionsAndMore.h"
#include "/home/jeevan/PicoWCar/src/ardpicoW/pico-PlatformIO/src/CliffSensor.h"
#include "/home/jeevan/PicoWCar/src/ardpicoW/pico-PlatformIO/src/PCA9685_AWDDriver.h"

CliffSensor frontCliff(PIN_FRONT_CLIFF);
CliffSensor rearCliff(PIN_REAR_CLIFF);

NewPing sonarF(PIN_TRIG_SONAR_FRONT, PIN_ECHO_SONAR_FRONT, kMaxSonarRangecm); //the lib needs cm as max range
NewPing sonarR(PIN_TRIG_SONAR_REAR, PIN_ECHO_SONAR_REAR, kMaxSonarRangecm);

uint32_t lastPublish = 0;
int32_t sonarDistanceFront = 5;
int32_t sonarDistanceRear = 7;
uint32_t lastSonarFrontPoll = 0;
uint32_t lastSonarRearPoll = 0;
uint32_t lastMotorCommandTime = 0;
SystemState systemState = SystemState::INIT;
bool mpuAvailable = false;

// I2C and peripherals - construct statically, initialize in setup()
TwoWire *picomasteri2c = &Wire;  // or &Wire1 depending on which I2C bus
Adafruit_MPU6050 mpu;

// Motor driver object lives in static storage for deterministic lifetime
static PCA9685_AWDDriver motorDriver(
  MOTOR_DRV_ADDR, picomasteri2c,
  PIN_ENC_FRONT_RIGHT_Y, PIN_ENC_FRONT_RIGHT_G,
  PIN_ENC_FRONT_LEFT_G, PIN_ENC_FRONT_LEFT_Y,
  PIN_ENC_REAR_RIGHT_Y, PIN_ENC_REAR_RIGHT_G,
  PIN_ENC_REAR_LEFT_G, PIN_ENC_REAR_LEFT_Y,
  PULSE_PER_REV, kMaxSaneRpm
);


// Shared data structures with synchronization
struct SensorBuffer {
  float accelX, accelY, accelZ;
  float gyroX, gyroY, gyroZ;
  float temp;
  int32_t sonarFrontcm;
  int32_t sonarRearcm;
  bool cliffFront, cliffRear;
  float speedFL, speedFR, speedRL, speedRR;
  uint32_t timestamp;
};

struct MotorCommand {
  float tqFR, tqFL, tqRR, tqRL;
  uint32_t commandTime;
};

// Simple shared state (single core)
static SensorBuffer currentSensors = {0};
static MotorCommand latestMotorCmd = {0, 0, 0, 0, 0};
static bool robotEnabled = true;

// Forward declarations
void process_uart_commands();

// Forward declaration for telemetry function
void sendTelemetry(const SensorBuffer &sensors);

// Single-threaded loop bookkeeping
static uint32_t lastLoopStart = 0;

// Add a global boot counter (retained in RTC memory if possible)
volatile uint32_t bootCounter = 0;

void setup() {
  bootCounter++;
  Serial.print("[BOOT] Count: ");
  Serial.println(bootCounter);

  // Concept 5: Enable hardware watchdog (2 second timeout). 
  watchdog_enable(kWatchdogTimeoutMs, true);

  // Concept 1: Timeout on USB Serial (for debug/monitoring)
  Serial.begin(115200);
  delay(500);
  const uint32_t serialStart = millis();
  while ((!Serial) && ((millis() - serialStart) < kSerialWaitMs)) {
    delay(10);
    watchdog_update();
  }


  Serial.println();
  Serial.println("==========================================");
  Serial.println("[DEBUG] Pico W BOOT - Watchdog ENABLED");
  Serial.flush();

  Serial.println("[INIT] Pico W starting...");
  systemState = SystemState::INIT;


  Serial.println("[INIT] Initializing cliff sensors...");
  frontCliff.init();
  rearCliff.init();
  Serial.println("[INIT] Cliff sensors initialized.");

  Serial.println("[INIT] I2C bus and motor driver...");
  // End any previous I2C transmission and re-initialize with proper pins
  picomasteri2c->end();  // Reset the I2C bus if it was already initialized
  picomasteri2c->setSDA(PICOW_I2C0_SDA);
  picomasteri2c->setSCL(PICOW_I2C0_SCL);
  picomasteri2c->begin();
  delay(100);  // Allow I2C to stabilize
  
  // Scan I2C bus to see what devices are present
  Serial.println("[DEBUG] Scanning I2C bus...");
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    picomasteri2c->beginTransmission(addr);
    uint8_t result = picomasteri2c->endTransmission();
    if (result == 0) {
      Serial.print("[DEBUG] I2C device found at address: 0x");
      Serial.println(addr, HEX);
    }
  }
  
  // Check PCA9685 presence.
  Serial.print("[DEBUG] Checking for PCA9685 at address: 0x");
  Serial.println(MOTOR_DRV_ADDR, HEX);
  picomasteri2c->beginTransmission(MOTOR_DRV_ADDR);
  int ackStatus = picomasteri2c->endTransmission();
  if (ackStatus != 0) {
    Serial.println("[ERROR] PCA9685 not responding on I2C");
    systemState = SystemState::ERROR;
  }

  // Initialize motor driver (PCA9685 + encoders)
  Serial.println("[INIT] Initializing motor driver and encoders...");
  motorDriver.begin(kPwmFreqHz);
  Serial.println("[INIT] Motor driver and encoders initialized.");
  

  // Concept 3 & 4: Graceful MPU6050 failure handling (no infinite loop)
  if (!mpu.begin(MPU6050_I2CADDR_DEFAULT, picomasteri2c)) {
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
  // --- UART handshake: Wait for Jetson to send CMD,stop before proceeding ---

  // UART1 for Jetson comms
  Serial.println("[INIT] setting up UART as slave");
  Serial1.setTX(PICOW_JETSON_UART_TX);
  Serial1.setRX(PICOW_JETSON_UART_RX);
  Serial1.begin(kSerialBaud);
  delay(500);
  Serial.println("[UART] Waiting for Jetson handshake (CMD,stop) before setup...");
  uint32_t uartWaitStart = millis();
  bool gotJetson = false;
  while ((millis() - uartWaitStart) < 10000) { // Wait up to 10 seconds
    if (Serial1.available()) {
      String jetsonMsg = Serial1.readStringUntil('\n');
      jetsonMsg.trim();
      if (jetsonMsg.equalsIgnoreCase("CMD,stop")) {
        gotJetson = true;
        Serial.println("[UART] Jetson handshake received (CMD,stop), proceeding with setup.");
        break;
      }
    }
    delay(10);
    watchdog_update();
  }
  if (!gotJetson) {
    Serial.println("[UART] No Jetson handshake (CMD,stop) received. Will keep waiting in main loop.");
  }

  // Only proceed with hardware setup if Jetson handshake (CMD,stop) was received
  while (!gotJetson) {
    if (Serial1.available()) {
      String jetsonMsg = Serial1.readStringUntil('\n');
      jetsonMsg.trim();
      if (jetsonMsg.equalsIgnoreCase("CMD,stop")) {
        gotJetson = true;
        Serial.println("[UART] Jetson handshake received (CMD,stop), proceeding with setup.");
        break;
      }
    }
    delay(10);
    watchdog_update();
  }

  Serial.println("[INIT] System ready. Streaming data every 100ms.");
  lastPublish = millis();
  lastMotorCommandTime = millis();  


}

void loop() {
  const uint32_t now = millis();
  if (now - lastLoopStart < kLoopPeriodMs) {
    delay(1);
    return;
  }
  lastLoopStart = now;

  // Handle incoming UART commands
  process_uart_commands();

  // Poll sonar (rear then front with crosstalk delay)
  if (now - lastSonarRearPoll >= kSonarPollIntervalMs) {
    int32_t rear = sonarR.ping_cm();
    if (rear > 0 && rear < kMaxSonarRangecm) {
      sonarDistanceRear = rear;
    }
    lastSonarRearPoll = now;
  }

  if (now - lastSonarFrontPoll >= kSonarPollIntervalMs) {
    delay(min_sonar_delayMs);
    int32_t front = sonarF.ping_cm();
    if (front > 0 && front < kMaxSonarRangecm) {
      sonarDistanceFront = front;
    }
    lastSonarFrontPoll = now;
  }

  // Read IMU and cliffs
  sensors_event_t accel = {}, gyro = {}, temp = {};
  if (mpuAvailable) {
    mpu.getEvent(&accel, &gyro, &temp);
  }
  frontCliff.read();
  rearCliff.read();

  // Update sensor struct
  currentSensors.accelX = accel.acceleration.x;
  currentSensors.accelY = accel.acceleration.y;
  currentSensors.accelZ = accel.acceleration.z;
  currentSensors.gyroX = gyro.gyro.x;
  currentSensors.gyroY = gyro.gyro.y;
  currentSensors.gyroZ = gyro.gyro.z;
  currentSensors.temp = temp.temperature;
  currentSensors.speedFL = motorDriver.getRPM(MOTOR_FL) * MOTOR_RPM_TO_CMPS;
  currentSensors.speedFR = motorDriver.getRPM(MOTOR_FR) * MOTOR_RPM_TO_CMPS;
  currentSensors.speedRL = motorDriver.getRPM(MOTOR_RL) * MOTOR_RPM_TO_CMPS;
  currentSensors.speedRR = motorDriver.getRPM(MOTOR_RR) * MOTOR_RPM_TO_CMPS;
  currentSensors.sonarFrontcm = sonarDistanceFront;
  currentSensors.sonarRearcm = sonarDistanceRear;
  currentSensors.cliffFront = frontCliff.getLastState();
  currentSensors.cliffRear = rearCliff.getLastState();
  currentSensors.timestamp = now;

  // Pick motor command (latestMotorCmd set by UART commands)
  MotorCommand cmdFinal = latestMotorCmd;

  // Safety check
  if (!robotEnabled || motorDriver.checkMotorSafetyTimeouts(kMotorSafetyTimeoutMs)) {
    motorDriver.setMotor(MOTOR_FR, 0.0f);
    motorDriver.setMotor(MOTOR_FL, 0.0f);
    motorDriver.setMotor(MOTOR_RR, 0.0f);
    motorDriver.setMotor(MOTOR_RL, 0.0f);
  } else {
    motorDriver.setMotor(MOTOR_FR, cmdFinal.tqFR);
    motorDriver.setMotor(MOTOR_FL, cmdFinal.tqFL);
    motorDriver.setMotor(MOTOR_RR, cmdFinal.tqRR);
    motorDriver.setMotor(MOTOR_RL, cmdFinal.tqRL);
    lastMotorCommandTime = now;
  }

  // Telemetry
  sendTelemetry(currentSensors);

  // Feed watchdog
  watchdog_update();
}

// Telemetry output function
void sendTelemetry(const SensorBuffer &sensors) {
  DEBUG_PRINT(">accel_x:");
  DEBUG_PRINTLN(sensors.accelX, 2);
  DEBUG_PRINT(">accel_y:");
  DEBUG_PRINTLN(sensors.accelY, 2);
  DEBUG_PRINT(">accel_z:");
  DEBUG_PRINTLN(sensors.accelZ, 2);

  DEBUG_PRINT(">gyro_x:");
  DEBUG_PRINTLN(sensors.gyroX, 2);
  DEBUG_PRINT(">gyro_y:");
  DEBUG_PRINTLN(sensors.gyroY, 2);
  DEBUG_PRINT(">gyro_z:");
  DEBUG_PRINTLN(sensors.gyroZ, 2);

  DEBUG_PRINT(">temp_c:");
  DEBUG_PRINTLN(sensors.temp, 2);
 
  DEBUG_PRINT(">sonar_frt_cm:");
  DEBUG_PRINTLN(sensors.sonarFrontcm);
  DEBUG_PRINT(">sonar_rear_cm:");
  DEBUG_PRINTLN(sensors.sonarRearcm);

  DEBUG_PRINT(">speedFR:");
  DEBUG_PRINTLN(sensors.speedFR);
  DEBUG_PRINT(">speedFL:");
  DEBUG_PRINTLN(sensors.speedFL);
  DEBUG_PRINT(">speedRR:");
  DEBUG_PRINTLN(sensors.speedRR);
  DEBUG_PRINT(">speedRL:");
  DEBUG_PRINTLN(sensors.speedRL);

  DEBUG_PRINT(">cliff_front:");
  DEBUG_PRINTLN(sensors.cliffFront ? 1 : 0);
  DEBUG_PRINT(">cliff_rear:");
  DEBUG_PRINTLN(sensors.cliffRear ? 1 : 0);
}

void queue_motor_command(float tqFR, float tqFL, float tqRR, float tqRL) {
  latestMotorCmd = {tqFR, tqFL, tqRR, tqRL, millis()};
}

// Simple UART command parser
// Commands: forward, backward, left, right, stop
// Or: cmd <tqFR> <tqFL> <tqRR> <tqRL>
void process_uart_commands() {
  if (!Serial1.available()) {
    DEBUG_UART_PRINTLN("[UART] No Serial1 data available");
    return;
  }
  String line = Serial1.readStringUntil('\n');
  line.trim();
  DEBUG_UART_PRINTLN("[UART] Received: " + line);
  if (line.length() == 0) return;

  if (line.equalsIgnoreCase("forward")) {
    queue_motor_command(0.5f, 0.5f, 0.5f, 0.5f);
  } else if (line.equalsIgnoreCase("backward")) {
    queue_motor_command(-0.5f, -0.5f, -0.5f, -0.5f);
  } else if (line.equalsIgnoreCase("left")) {
    queue_motor_command(-0.3f, 0.3f, -0.3f, 0.3f);
  } else if (line.equalsIgnoreCase("right")) {
    queue_motor_command(0.3f, -0.3f, 0.3f, -0.3f);
  } else if (line.equalsIgnoreCase("stop")) {
    queue_motor_command(0.0f, 0.0f, 0.0f, 0.0f);
  } else if (line.equalsIgnoreCase("enable")) {
    robotEnabled = true;
    DEBUG_UART_PRINTLN("[UART] Robot ENABLED");
  } else if (line.equalsIgnoreCase("disable")) {
    robotEnabled = false;
    queue_motor_command(0.0f, 0.0f, 0.0f, 0.0f);
    DEBUG_UART_PRINTLN("[UART] Robot DISABLED");
  } else if (line.startsWith("cmd")) {
    float fr, fl, rr, rl;
    if (sscanf(line.c_str(), "cmd %f %f %f %f", &fr, &fl, &rr, &rl) == 4) {
      queue_motor_command(fr, fl, rr, rl);
    } else {
      DEBUG_UART_PRINTLN("[UART] cmd parse error. Use: cmd fr fl rr rl");
    }
  } else {
    DEBUG_UART_PRINT("[UART] Unknown command ");
  }

}
