/*this code can handle timeouts properly from the hw_mcu_node
yaay*/

#include <cstdint>
#include <array>
#include <limits>
#include <cmath>
#include <SimpleFOC.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <Adafruit_PWMServoDriver.h>
#include <NewPing.h>
#include "driver/uart.h"
#include "esp_intr_alloc.h"

#define UART_NUM UART_NUM_1
#define BUF_SIZE 1024

// #include <hardware/watchdog.h>

// #define MCU_PICOW_RP2040
#define MCU_ESP32S3_40PIN 
#define NONSTEER_4WD_RUBBERWHL_2XSONAR_2xCLIFF

// Set to 1 to enable loop debug output, 0 to disable. Ralph S Bacon from Youtube solution
#define LOOP_DEBUG_A 1
#define LOOP_DEBUG_B 0
#define LOOP_DEBUG_I2C 0
#define LOOP_DEBUG_I2C2 0


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
CliffSensor rearCliff(PIN_REAR_CLIFF);

NewPing sonarF(PIN_TRIG_SONAR_FRONT, PIN_ECHO_SONAR_FRONT, kMaxSonarRangecm); //the lib needs cm as max range
NewPing sonarR(PIN_TRIG_SONAR_REAR, PIN_ECHO_SONAR_REAR, kMaxSonarRangecm);

int16_t sonarDistanceFrontcm = 7;
int16_t sonarDistanceRearcm = 7;
uint32_t lastPublish = 0;
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

// Simple shared state (single core)
static SensorBuffer mcuSensors = {0};
static MotorCommand latestMotorCmd = {0, 0, 0, 0, 0};
static bool robotEnabled = true;
static uint32_t lastMavlinkCommandTime = 0;

// MAVLink + Jetson serial bridge state
static constexpr uint8_t kMavSystemId = 200;
static constexpr uint8_t kMavComponentId = MAV_COMP_ID_ONBOARD_COMPUTER;
static constexpr uint32_t kJetsonSerialBaud = 921600;
static constexpr uint32_t kJetsonSerialReconnectIntervalMs = 1000;
static constexpr size_t kMavlinkFifoSize = 768; // outgoing data buffer size
static std::array<uint8_t, kMavlinkFifoSize> mavlinkTxFifo{}; // outgoing data buffer
static volatile size_t mavlinkTxHead = 0;
static volatile size_t mavlinkTxTail = 0;
/* MAVLink frames are variable length, so we enqueue raw bytes and let the Jetson drain
them via UART whenever bandwidth is available. */
static constexpr size_t kJetsonRxFifoSize = 256;// incoming commands buffer size


static HardwareSerial jetsonSerial(1);
static bool jetsonSerialReady = false;
static uint32_t lastSerialInitAttempt = 0;
static volatile uint32_t lastJetsonActivityMs = 0;
static std::array<uint8_t, kJetsonRxFifoSize> jetsonRxFifo{}; // incoming data buffer
static volatile size_t jetsonRxHead = 0;
static volatile size_t jetsonRxTail = 0;
static mavlink_message_t jetsonRxMessage{};
static mavlink_status_t jetsonRxStatus{};


// Forward declarations
static void ensureJetsonSerialReady(uint32_t now);
static void readJetsonSerial();
static void writeJetsonSerial();
static void enqueueBytes(const uint8_t *data, size_t len);
static void enqueueMavlinkMessage(const mavlink_message_t &message);
static void pushHighresImu(const SensorBuffer &sensors);
static void pushSonars(const SensorBuffer &sensors);
static void pushIRSensors(const SensorBuffer &sensors);
static void pushWheelRpm(const SensorBuffer &sensors);
void queue_motor_command(float tqFR, float tqRR, float tqFL, float tqRL);
static void initI2Cgeneric(TwoWire &bus,
                           int sdaPin,
                           int sclPin,
                           int slaveAddress = -1,
                           uint32_t frequencyHz = 400000U);
static void processJetsonCommandStream();
static void handleJetsonCommand(const mavlink_message_t &message);
static void pushJetsonRxByte(uint8_t value);
static bool popJetsonRxByte(uint8_t &value);

// Forward declaration for telemetry function
void pushTelemetry(const SensorBuffer &sensors);
// Unused UART helper block removed - using readJetsonSerial and writeJetsonSerial instead
// Single-threaded loop bookkeeping
static uint32_t lastLoopStart = 0;

// Add a global boot counter (retained in RTC memory if possible)
volatile uint32_t bootCounter = 0;

void setup() {
// Loop setup
  // Concept 5: Enable hardware watchdog (2 second timeout). 
  // watchdog_enable(kWatchdogTimeoutMs, true);

  // Concept 1: Timeout on USB Serial (for debug/monitoring)
  Serial.begin(115200);
  delay(500);
  const uint32_t serialStart = millis();
  while ((!Serial) && ((millis() - serialStart) < kSerialWaitMs)) {
    delay(10);
    // watchdog_update();
  }

  bootCounter++;
  Serial.print("[BOOT] Count: ");
  Serial.println(bootCounter);


  Serial.println();
  Serial.println("==========================================");
  Serial.println("[DEBUG] MCU BOOT - Watchdog ENABLED");
  Serial.flush();

  Serial.println("[INIT] MCU starting...");
  systemState = SystemState::INIT;

  Serial.println("[INIT] Initializing cliff sensors...");
  frontCliff.init();
  rearCliff.init();
  Serial.println("[INIT] Cliff sensors initialized.");

  Serial.println("[INIT] I2C0 bus and motor driver...");
  // End any previous I2C transmission and re-initialize with proper pins
  initI2Cgeneric(*picomasteri2c, MCU_I2C0_SDA, MCU_I2C0_SCL);
  delay(100);  // Allow I2C to stabilize
  
  // Scan I2C bus to see what devices are present
  Serial.println("[DEBUG] Scanning I2C0 bus...");
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    picomasteri2c->beginTransmission(addr);
    uint8_t result = picomasteri2c->endTransmission();
    if (result == 0) {
      Serial.print("[DEBUG] I2C0 device found at address: 0x");
      Serial.println(addr, HEX);
    }
  }
  
  // Check PCA9685 presence.
  Serial.print("[DEBUG] Checking for PCA9685 at address: 0x");
  Serial.println(MOTOR_DRV_ADDR, HEX);
  picomasteri2c->beginTransmission(MOTOR_DRV_ADDR);
  int ackStatus = picomasteri2c->endTransmission();
  if (ackStatus != 0) {
    Serial.println("[ERROR] PCA9685 not responding on I2C0");
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
  
  Serial.println("[INIT] Configuring Jetson UART telemetry bridge...");
  ensureJetsonSerialReady(millis());

  // Ensure motors are in a known safe state before entering the main loop. This was AI generated, not really required because init of PCA9685 takes care.
  queue_motor_command(0.0f, 0.0f, 0.0f, 0.0f);

  Serial.println("[INIT] System ready. Streaming data every 100ms.");
  lastPublish = millis();
  lastMotorCommandTime = millis();  

}

void loop() {
  // Always pump the serial data as fast as possible to prevent buffer overflows
  // at high baud rates (921600). Do NOT wait for the loop timer.
  ensureJetsonSerialReady(millis());
  readJetsonSerial();
  writeJetsonSerial();
  processJetsonCommandStream();

  const uint32_t now = millis();
  
  // Rate limiting for the rest of the control loop (Sensors, Motors, Telemetry)
  if (now - lastLoopStart < kLoopPeriodMs) {
    // Yield to other RTOS tasks briefly
    delay(1);
    return;
  }
  lastLoopStart = now;

  // // Poll sonar (rear then front with crosstalk delay)
  /*constrain(x,a,b) is interesting in that you can pass float, which could be a function pointer. 
  And this will cause completely incorrect results. This can't be caught by the compiler.
  Hence docs say never pass a function. Store the value returned by a function and pass to constrain(). 
  This is where functions need strong typing to avoid incorrect uses. 
  Other standard arduino functions like fabsf will have the same issue */
  if (now - lastSonarRearPoll >= kSonarPollIntervalMs) {
    delay(min_sonar_delayMs);
    int16_t rear = sonarR.ping_cm(); //cm  
    sonarDistanceRearcm = constrain( rear, kMinSonarRangecm, kMaxSonarRangecm ); //cm
    lastSonarRearPoll = now;
  }

  if (now - lastSonarFrontPoll >= kSonarPollIntervalMs) {
    delay(min_sonar_delayMs);
    int16_t front = sonarF.ping_cm(); //cm
    if (front > 0 && front < kMaxSonarRangecm) {
      sonarDistanceFrontcm = front;
    }
    lastSonarFrontPoll = now;
  }

  // Read IMU and cliffsensor
  sensors_event_t accel = {}, gyro = {}, temp = {};
  if (mpuAvailable) {
    // Check if device is still responding on I2C to avoid timeout errors
    picomasteri2c->beginTransmission(MPU6050_I2CADDR_DEFAULT);
    if (picomasteri2c->endTransmission() == 0) {
      mpu.getEvent(&accel, &gyro, &temp);
    } else {
      Serial.println("[WARN] MPU6050 lost connection. Disabling IMU.");
      mpuAvailable = false;
      systemState = SystemState::DEGRADED;
    }
  }
  frontCliff.read(); //bool
  rearCliff.read();  //bool

  // Update sensor struct
  mcuSensors.accelX = accel.acceleration.x; //mps2
  mcuSensors.accelY = accel.acceleration.y; //mps2
  mcuSensors.gyroZ = gyro.gyro.z; //radps
  mcuSensors.temp = temp.temperature;
  mcuSensors.speedFR = motorDriver.getRPM(MOTOR_FR); //RPM
  mcuSensors.speedRR = motorDriver.getRPM(MOTOR_RR); //RPM
  mcuSensors.speedFL = motorDriver.getRPM(MOTOR_FL); //RPM  
  mcuSensors.speedRL = motorDriver.getRPM(MOTOR_RL); //RPM
  
  //temporary override to test I2C
  // mcuSensors.speedFR = 1;
  // mcuSensors.speedRR = 2;
  // mcuSensors.speedFL = 3;
  // mcuSensors.speedRL = 4;
  

  mcuSensors.sonarFrontcm = sonarDistanceFrontcm; // maintain distance in cm because mavlink DISTANCE_SENSOR uses uint16 for distance
  mcuSensors.sonarFquality = 100U; //sonar library doesn't provide quality metric, intend to make one later based on readings consistency
  mcuSensors.sonarRearcm = sonarDistanceRearcm; // maintain distance in cm because mavlink DISTANCE_SENSOR uses uint16 for distance
  mcuSensors.sonarRquality = 100U; //sonar library doesn't provide quality metric, intend to make one later based on readings consistency
  mcuSensors.cliffFront = frontCliff.getLastState();
  mcuSensors.cliffRear = rearCliff.getLastState();
  mcuSensors.timestamp = now;


  // Safety check
  if (!robotEnabled || (now - lastMavlinkCommandTime > kMotorSafetyTimeoutMs)) {
  // if (!robotEnabled ) {
    motorDriver.setMotor(MOTOR_FR, 0.0f);
    motorDriver.setMotor(MOTOR_RR, 0.0f);
    motorDriver.setMotor(MOTOR_FL, 0.0f);    
    motorDriver.setMotor(MOTOR_RL, 0.0f);
    // Serial.print("[SAFETY] Motors stopped due to safety timeout or robot disabled:");
    // Serial.println(millis());
  } else {
  // Pick motor command (updated when higher level control enqueues new torques)
    motorDriver.setMotor(MOTOR_FR, latestMotorCmd.tqFR);
    motorDriver.setMotor(MOTOR_RR, latestMotorCmd.tqRR);
    motorDriver.setMotor(MOTOR_FL, latestMotorCmd.tqFL);    
    motorDriver.setMotor(MOTOR_RL, latestMotorCmd.tqRL);
    lastMotorCommandTime = now;
  }

  // Telemetry
  /*its job is to print to serial and also to queue data to mavlink queue
   the que is filled every time we go through loop. 
  the draining of queue happens eitehr when jetson requests data over i2c, 
   or when we want to add data and queue is full, we drop oldest data. */
  pushTelemetry(mcuSensors);

  // Feed watchdog
  // watchdog_update();
}

// Telemetry output function
void pushTelemetry(const SensorBuffer &sensors) {
  DEBUG_PRINT(">accel_x:");
  DEBUG_PRINTLN(sensors.accelX, 2);
  DEBUG_PRINT(">accel_y:");
  DEBUG_PRINTLN(sensors.accelY, 2);

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

  // HIGHRES_IMU -> /mcu/imu (sensor_msgs/Imu)
  pushHighresImu(sensors);
  // DISTANCE_SENSOR id=1 -> /mcu/range/front (sensor_msgs/Range, ultrasound)
  // DISTANCE_SENSOR id=2 -> /mcu/range/rear (sensor_msgs/Range, ultrasound)
  pushSonars(sensors);
  // DISTANCE_SENSOR id=3 -> /mcu/cliff/front (sensor_msgs/Range, infrared)
  // DISTANCE_SENSOR id=4 -> /mcu/cliff/rear (sensor_msgs/Range, infrared)
  pushIRSensors(sensors);
  // WHEEL_RPM -> /esc_telemetry (std_msgs/Float32MultiArray, [FR, RR, FL, RL] rpm)
  pushWheelRpm(sensors);
}

void queue_motor_command(float tqFR, float tqRR, float tqFL, float tqRL) {
  latestMotorCmd = {tqFR, tqRR, tqFL, tqRL, millis()};
}


static uint16_t clampDistanceCm(int32_t value) {
  const int32_t clamped = constrain(value, 0, kMaxSonarRangecm);
  return static_cast<uint16_t>(clamped);
}

static void enqueueBytes(const uint8_t *data, size_t len) {
  noInterrupts();
  for (size_t i = 0; i < len; ++i) {
    const size_t next = (mavlinkTxHead + 1U) % kMavlinkFifoSize;
    if (next == mavlinkTxTail) {
      mavlinkTxTail = (mavlinkTxTail + 1U) % kMavlinkFifoSize;
    }
    // The FIFO treats the MAVLink stream as a flat byte queue. I2C chunk boundaries are
    // irrelevant here; the master decides how many bytes to fetch each time it polls us.
    mavlinkTxFifo[mavlinkTxHead] = data[i];
    mavlinkTxHead = next;
  }
  interrupts();
}

static void enqueueMavlinkMessage(const mavlink_message_t &message) {
  uint8_t frame[MAVLINK_MAX_PACKET_LEN];
  const uint16_t frameLen = mavlink_msg_to_send_buffer(frame, &message);
  enqueueBytes(frame, frameLen);
}

static void pushJetsonRxByte(uint8_t value) {
  const size_t next = (jetsonRxHead + 1U) % kJetsonRxFifoSize;
  if (next == jetsonRxTail) {
    jetsonRxTail = (jetsonRxTail + 1U) % kJetsonRxFifoSize;
  }
  jetsonRxFifo[jetsonRxHead] = value;
  jetsonRxHead = next;
}

static bool popJetsonRxByte(uint8_t &value) {
  noInterrupts();
  const bool empty = (jetsonRxTail == jetsonRxHead);
  if (empty) {
    interrupts();
    return false;
  }
  value = jetsonRxFifo[jetsonRxTail];
  jetsonRxTail = (jetsonRxTail + 1U) % kJetsonRxFifoSize;
  interrupts();
  return true;
}

static void processJetsonCommandStream() {
  uint8_t byte = 0U;
  while (popJetsonRxByte(byte)) {
    int parse_result = mavlink_parse_char(MAVLINK_COMM_1, byte, &jetsonRxMessage, &jetsonRxStatus);
    if (parse_result != 0) {
      DEBUG_I2C2_PRINT("[MAVLINK] Parsed message: ");
      DEBUG_I2C2_PRINTLN(jetsonRxMessage.msgid);
      handleJetsonCommand(jetsonRxMessage);
    } else {
      DEBUG_I2C2_PRINT("[MAVLINK] Byte ");
      DEBUG_I2C2_PRINT(byte);
      DEBUG_I2C2_PRINTLN(": No message parsed yet.");
    }
  }
}

static void handleJetsonCommand(const mavlink_message_t &message) {
  switch (message.msgid) {
    case MAVLINK_MSG_ID_SET_ACTUATOR_CONTROL_TARGET: {
      lastMavlinkCommandTime = millis();
      mavlink_set_actuator_control_target_t act = {};
      mavlink_msg_set_actuator_control_target_decode(&message, &act);
      // Map actuators[0-3] to FR, FL, RR, RL
      float tqFR = constrain(act.controls[0], -1.0f, 1.0f);
      float tqRR = constrain(act.controls[1], -1.0f, 1.0f);
      float tqFL = constrain(act.controls[2], -1.0f, 1.0f);
      float tqRL = constrain(act.controls[3], -1.0f, 1.0f);
      DEBUG_PRINT(">tqFR: ");
      DEBUG_PRINTLN(tqFR,2);
      DEBUG_PRINT(">tqRR: ");
      DEBUG_PRINTLN(tqRR,2);
      DEBUG_PRINT(">tqFL: ");
      DEBUG_PRINTLN(tqFL,2);
      DEBUG_PRINT(">tqRL: ");
      DEBUG_PRINTLN(tqRL,2);
    
      
      queue_motor_command(tqFR, tqRR, tqFL, tqRL);
      break;
    }
    default:
      break;
  }
}


static void pushHighresImu(const SensorBuffer &sensors) {
  // MISRA: Static buffer eliminates stack allocation per call (Rule 8.9 - minimize scope)
  // Thread-safe in single-core architecture
  static uint8_t frame[MAVLINK_MAX_PACKET_LEN];
  
  mavlink_highres_imu_t imu{};
  imu.time_usec = static_cast<uint64_t>(sensors.timestamp) * 1000ULL;
  imu.xacc = sensors.accelX;
  imu.yacc = sensors.accelY;
  imu.zacc = 0.0f;
  imu.xgyro = 0.0f;
  imu.ygyro = 0.0f;
  imu.zgyro = sensors.gyroZ;
  const float nanValue = std::numeric_limits<float>::quiet_NaN();
  imu.xmag = nanValue;
  imu.ymag = nanValue;
  imu.zmag = nanValue;
  imu.abs_pressure = nanValue;
  imu.diff_pressure = nanValue;
  imu.pressure_alt = nanValue;
  imu.temperature = sensors.temp;
  imu.fields_updated = static_cast<uint16_t>((1U << 0) | (1U << 1) | (1U << 2) |
                                             (1U << 3) | (1U << 4) | (1U << 5) |
                                             (1U << 12));
  imu.id = 0;

  mavlink_message_t message;
  mavlink_msg_highres_imu_encode(kMavSystemId, kMavComponentId, &message, &imu);
  
  const uint16_t frameLen = mavlink_msg_to_send_buffer(frame, &message);
  // write to FIFO buffer
  enqueueBytes(frame, frameLen);  
}

static void pushDistanceReading(uint8_t id,
                                   uint8_t type,
                                   uint8_t orientation,
                                   uint16_t currentDistance,
                                   uint16_t minDistance,
                                   uint16_t maxDistance,
                                   uint8_t signalQuality,
                                   uint32_t timestampMs) {
  // MISRA: Static buffer reduces stack pressure - called 4x per loop (Rule 8.9)
  static uint8_t frame[MAVLINK_MAX_PACKET_LEN];
  mavlink_distance_sensor_t distance{};
  distance.time_boot_ms = timestampMs;
  distance.min_distance = minDistance;
  distance.max_distance = maxDistance;
  distance.current_distance = currentDistance;
  distance.type = type;
  distance.id = id;
  distance.orientation = orientation;
  distance.covariance = 0xFF;
  distance.horizontal_fov = 0.0f;
  distance.vertical_fov = 0.0f;
  distance.signal_quality = signalQuality;
  // MISRA: Explicit unsigned literal '4U' for type safety (Rule 10.8)
  for (size_t i = 0; i < 4U; ++i) {
    distance.quaternion[i] = 0.0f;
  }

  mavlink_message_t message;
  mavlink_msg_distance_sensor_encode(kMavSystemId, kMavComponentId, &message, &distance);
  const uint16_t frameLen = mavlink_msg_to_send_buffer(frame, &message);
  enqueueBytes(frame, frameLen);
}

static void pushSonars(const SensorBuffer &sensors) {
  // MISRA: Function-scope constants eliminate magic numbers (Rule 2.5)
  // and reduce runtime recalculation overhead (Rule 8.9)..and i deleted them in favour of global    
  //clamp no longer reqd, already done at source

  
  // id=1: front sonar, id=2: rear sonar (matches /pico/range/front and /pico/range/rear)
  pushDistanceReading(1, MAV_DISTANCE_SENSOR_ULTRASOUND, MAV_SENSOR_ROTATION_NONE,
                         sensors.sonarFrontcm, kMinSonarRangecm, kMaxSonarRangecm, sensors.sonarFquality, sensors.timestamp);
  pushDistanceReading(2, MAV_DISTANCE_SENSOR_ULTRASOUND, MAV_SENSOR_ROTATION_YAW_180,
                         sensors.sonarRearcm, kMinSonarRangecm, kMaxSonarRangecm, sensors.sonarRquality, sensors.timestamp);
}

// Cliff sensor telemetry: Binary sensors reporting presence/absence of floor
// Sensor semantics:
//   cliffFront/cliffRear = TRUE  -> Cliff edge detected (no floor below, danger!)
//   cliffFront/cliffRear = FALSE -> Floor detected (safe to proceed)
// MAVLink DISTANCE_SENSOR mapping for binary cliff detection:
//   current_distance = max_distance -> No floor detected (cliff present)
//   current_distance = min_distance -> Floor detected (no cliff, safe)
//   signal_quality:  Low (25%) when cliff detected, High (100%) when floor present
static void pushIRSensors(const SensorBuffer &sensors) {
  // MISRA: Named constants eliminate magic numbers (Rule 2.5)
  // MISRA: Function-scope static const avoids repeated runtime initialization (Rule 8.9)
  static constexpr uint16_t kCliffMinRange = 7U;     // Floor directly below sensor, 7cm
  static constexpr uint16_t kCliffMaxRange = 10U;  // No floor (cliff), 10cm is arbitrary beyond expected max range
  static constexpr uint8_t kCliffQualityDanger = 25U;  // Low quality when cliff detected
  static constexpr uint8_t kCliffQualitySafe = 100U;   // High quality when floor present
  
  // MISRA: Separate variable assignments avoid ternary in function calls (Rule 17.8)
  // Binary mapping: TRUE (cliff) -> max height, FALSE (floor) -> min height (0cm = floor present)
  const uint16_t frontHeight = sensors.cliffFront ? kCliffMaxRange : kCliffMinRange;
  const uint16_t rearHeight = sensors.cliffRear ? kCliffMaxRange : kCliffMinRange;
  const uint8_t frontQuality = sensors.cliffFront ? kCliffQualityDanger : kCliffQualitySafe;
  const uint8_t rearQuality = sensors.cliffRear ? kCliffQualityDanger : kCliffQualitySafe;

  // id=3: front cliff IR, id=4: rear cliff IR (matches /pico/cliff/front and /pico/cliff/rear)
  pushDistanceReading(3, MAV_DISTANCE_SENSOR_INFRARED, MAV_SENSOR_ROTATION_PITCH_270,
                         frontHeight, kCliffMinRange, kCliffMaxRange, frontQuality, sensors.timestamp);
  pushDistanceReading(4, MAV_DISTANCE_SENSOR_INFRARED, MAV_SENSOR_ROTATION_PITCH_270,
                         rearHeight, kCliffMinRange, kCliffMaxRange, rearQuality, sensors.timestamp);
}

static void pushWheelRpm(const SensorBuffer &sensors) {
  static uint8_t frame[MAVLINK_MAX_PACKET_LEN];
  mavlink_wheel_rpm_t wheel_rpm{};
  wheel_rpm.rpm_fl = sensors.speedFL;
  wheel_rpm.rpm_fr = sensors.speedFR;
  wheel_rpm.rpm_rl = sensors.speedRL;
  wheel_rpm.rpm_rr = sensors.speedRR;

  mavlink_message_t message;
  mavlink_msg_wheel_rpm_encode(kMavSystemId, kMavComponentId, &message, &wheel_rpm);
  const uint16_t frameLen = mavlink_msg_to_send_buffer(frame, &message);
  DEBUG_I2C_PRINT(">[UART] Framelen:");
  DEBUG_I2C_PRINTLN(frameLen);
  DEBUG_I2C_PRINT(">[UART] Frame: ");
  for (size_t i = 0; i < frameLen; ++i) {
    DEBUG_I2C_PRINT(frame[i], HEX);
    DEBUG_I2C_PRINT(" ");
  }
  DEBUG_I2C_PRINTLN();
  enqueueBytes(frame, frameLen);
}

static void readJetsonSerial() {
  if (!jetsonSerialReady) {
    return;
  }
  DEBUG_I2C_PRINT("[UART RX] ");
  while (jetsonSerial.available() > 0) {
    const uint8_t value = static_cast<uint8_t>(jetsonSerial.read());    
    DEBUG_I2C_PRINT(value, HEX);    
    pushJetsonRxByte(value);
    lastJetsonActivityMs = millis();
  }
  DEBUG_I2C_PRINTLN(" ");
}

static void writeJetsonSerial() {
  if (!jetsonSerialReady) {
    return;
  }

  static uint8_t chunk[64];
  while ((mavlinkTxTail != mavlinkTxHead) && (jetsonSerial.availableForWrite() > 0)) {
    size_t bytesToSend = 0;
    while ((bytesToSend < sizeof(chunk)) && (mavlinkTxTail != mavlinkTxHead)) {
      chunk[bytesToSend++] = mavlinkTxFifo[mavlinkTxTail];
      mavlinkTxTail = (mavlinkTxTail + 1U) % kMavlinkFifoSize;
    }

    if (bytesToSend > 0) {
      DEBUG_I2C_PRINT("[UART TX] ");
      for (size_t i = 0; i < bytesToSend; i++) {
        DEBUG_I2C_PRINT(chunk[i], HEX);
        DEBUG_I2C_PRINT(" ");
      }
      DEBUG_I2C_PRINTLN();
      jetsonSerial.write(chunk, bytesToSend);
      lastJetsonActivityMs = millis();
    }
  }
}

static void ensureJetsonSerialReady(uint32_t now) {
  if (jetsonSerialReady) {
    return;
  }

  const uint32_t sinceAttempt = now - lastSerialInitAttempt;
  if ((lastSerialInitAttempt == 0U) || (sinceAttempt >= kJetsonSerialReconnectIntervalMs)) {
    lastSerialInitAttempt = now;
    jetsonSerial.end();
    jetsonSerial.begin(kJetsonSerialBaud, SERIAL_8N1, MCU_JETSON_UART1_RX, MCU_JETSON_UART1_TX);
    jetsonSerialReady = true;
    lastJetsonActivityMs = now;
    Serial.print("[UART] Jetson telemetry ready on RX:");
    Serial.print(MCU_JETSON_UART1_RX);
    Serial.print(" TX:");
    Serial.println(MCU_JETSON_UART1_TX);
  }
}

static void initI2Cgeneric(TwoWire &bus,
                           int sdaPin,
                           int sclPin,
                           int slaveAddress,
                           uint32_t frequencyHz) {
#if defined(MCU_PICOW_RP2040)
  bus.end();
  bus.setSDA(sdaPin);
  bus.setSCL(sclPin);
  if (slaveAddress >= 0) {
    bus.begin(slaveAddress);
  } else {
    bus.begin();
  }
#elif defined(MCU_ESP32S3_40PIN)
  bus.end();
  if (slaveAddress >= 0) {
    bus.begin(static_cast<uint8_t>(slaveAddress), sdaPin, sclPin, frequencyHz);
  } else {
    bus.begin(sdaPin, sclPin, frequencyHz);
  }
#else
  #error "Unsupported MCU for I2C init"
#endif
}