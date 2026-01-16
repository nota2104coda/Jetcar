#include <cstdint>
#define __FREERTOS 1
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <SimpleFOC.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <NewPing.h>
#include <hardware/watchdog.h>



#define MASTER_PICOW_4WD_NONSTEER_RUBBERWHL_2XSONAR_2xCLIFF

// Set to 1 to enable loop debug output, 0 to disable. Ralph S Bacon from Youtube solution
#define LOOP_DEBUG 1

#if LOOP_DEBUG
  #define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...) ((void)0)
  #define DEBUG_PRINTLN(...) ((void)0)
#endif

#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>

//follow metric system everywhere. all distances in m, speeds m/s, acceleration m/s^2, angles in rad, angular velocity in rad/s

// System constants
static constexpr float kGearRatio = 46.0;
static constexpr uint32_t kLoopPeriodMs = 100;
static constexpr uint32_t kSerialBaud = 115200;
static constexpr uint32_t kSerialWaitMs = 500; // Wait up to 500ms for Serial to start
static constexpr uint32_t kPwmFreqHz = 1600;
static constexpr uint32_t kSonarPollIntervalMs = 50;
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

#include "/home/jeevan/PicoWCar/libs/arduino/CarConfigurations.h"
#include "/home/jeevan/PicoWCar/libs/arduino/RobotCarPinDefinitionsAndMore.h"
#include "/home/jeevan/PicoWCar/src/ardpicoW/pico-PlatformIO/src/CliffSensor.h"
#include "/home/jeevan/PicoWCar/src/ardpicoW/pico-PlatformIO/src/PCA9685_AWDDriver.h"

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
  int32_t sonarRear;
  bool cliffFront, cliffRear;
  uint32_t timestamp;
};

struct MotorCommand {
  float speedFR, speedFL, speedRR, speedRL;
  uint32_t commandTime;
};

// Thread-safe queues and semaphores
QueueHandle_t sensorQueue = NULL;      // Core1 → Core0: sensor data
QueueHandle_t motorCmdQueue = NULL;    // Core0 → Core1: motor commands
QueueHandle_t uartRxQueue = NULL;      // UART ISR → Core0: commands
SemaphoreHandle_t sensorMutex = NULL;  // Protect shared sensor buffer

// Global sensor buffer (protected by mutex)
static SensorBuffer currentSensors = {0};
static volatile bool sonarReadyFlag = false;

// Forward declaration for telemetry function
void sendTelemetry(const SensorBuffer &sensors);

// Core 0 Task: Time-critical sensor + motor control
void core0Task(void *pvParameters) {
  TickType_t lastWakeTime;
  TickType_t loopPeriod;
  lastWakeTime = xTaskGetTickCount();
  loopPeriod = pdMS_TO_TICKS(100); // 100ms loop
  
  while (1) {
    // A. Read all local sensors (IMU, cliff, encoders)
    sensors_event_t accel, gyro, temp;
    if (mpuAvailable) {
      mpu.getEvent(&accel, &gyro, &temp);
    }
    frontCliff.read();
    rearCliff.read();
    
    // Update shared buffer
    xSemaphoreTake(sensorMutex, portMAX_DELAY);
    currentSensors.accelX = accel.acceleration.x;
    currentSensors.accelY = accel.acceleration.y;
    currentSensors.accelZ = accel.acceleration.z;
    currentSensors.gyroX = gyro.gyro.x;
    currentSensors.gyroY = gyro.gyro.y;
    currentSensors.gyroZ = gyro.gyro.z;
    currentSensors.temp = temp.temperature;
    currentSensors.cliffFront = frontCliff.getLastState();
    currentSensors.cliffRear = rearCliff.getLastState();
    currentSensors.timestamp = millis();
    xSemaphoreGive(sensorMutex);
    
    // B. Check if sonar should be read (non-blocking flag from Core1)
    // Core1 will set a flag when sonar is ready
    if (sonarReadyFlag) {
      if (xSemaphoreTake(sensorMutex, portMAX_DELAY) == pdTRUE) {
        currentSensors.sonarRear = sonarDistanceRear; // From Core1
        xSemaphoreGive(sensorMutex);
      }
      sonarReadyFlag = false;
    }
    
    // C. Read UART command arbitration
    MotorCommand cmdUART = {0}, cmdFoxglove = {0}, cmdFinal = {0};
    if (xQueueReceive(uartRxQueue, &cmdUART, 0) == pdTRUE) {
      cmdFinal = cmdUART; // UART has priority
    }
    if (xQueueReceive(motorCmdQueue, &cmdFoxglove, 0) == pdTRUE && cmdFinal.speedFR == 0) {
      cmdFinal = cmdFoxglove; // Foxglove secondary
    }
    
    // D. Execute motor commands
    if (cmdFinal.speedFR != 0 || cmdFinal.speedFL != 0 || 
        cmdFinal.speedRR != 0 || cmdFinal.speedRL != 0) {
      motorDriver.setMotor(MOTOR_FR, cmdFinal.speedFR);
      motorDriver.setMotor(MOTOR_FL, cmdFinal.speedFL);
      motorDriver.setMotor(MOTOR_RR, cmdFinal.speedRR);
      motorDriver.setMotor(MOTOR_RL, cmdFinal.speedRL);
      lastMotorCommandTime = millis();
    }
    
    // Check motor safety timeout
    motorDriver.checkMotorSafetyTimeouts(kMotorSafetyTimeoutMs);
    
    // Output telemetry to UART
    sendTelemetry(currentSensors);
    
    // Watchdog feed
    watchdog_update();
    
    // Wait until next loop period (blocks if early)
    vTaskDelayUntil(&lastWakeTime, loopPeriod);
  }
}

// Core 1 Task: WiFi + Sonar (blocking I/O tolerant)
void core1Task(void *pvParameters) {
  TickType_t sonarPollPeriod;
  TickType_t lastSonarTime;
  sonarPollPeriod = pdMS_TO_TICKS(500);
  lastSonarTime = xTaskGetTickCount();
  
  while (1) {
    // A. Non-blocking sonar poll (only if time permits)
    TickType_t now;
    now = xTaskGetTickCount();
    if ((now - lastSonarTime) >= sonarPollPeriod) {
      int32_t reading = sonarR.ping_cm();
      if (reading > 0 && reading < (kMaxSonarRangem * conv_M_TO_CM)) {
        if (xSemaphoreTake(sensorMutex, portMAX_DELAY) == pdTRUE) {
          currentSensors.sonarRear = reading;
          xSemaphoreGive(sensorMutex);
        }
        sonarReadyFlag = true;
      }
      lastSonarTime = now;
    }
    
    // B. WiFi operations (can block here)
    // Read commands from Foxglove, post to motorCmdQueue
    // Send telemetry via WiFi
    
    vTaskDelay(pdMS_TO_TICKS(50)); // Yield CPU, check sonar every 50ms
  }
}

void setup() {
  // Concept 5: Enable hardware watchdog (2 second timeout). 
  watchdog_enable(kWatchdogTimeoutMs, true);

  // Concept 1: Timeout on Serial connection
  Serial.begin(kSerialBaud);
  delay(500);  // Give serial extra time
  const uint32_t serialStart = millis();
  while ((!Serial) && ((millis() - serialStart) < kSerialWaitMs)) {
    delay(10);
    watchdog_update(); 
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
  // I2C object already constructed statically; just begin the bus now. blank argument means its master.
  picomasteri2c->setSDA(PICOW_I2C0_SDA);
  picomasteri2c->setSCL(PICOW_I2C0_SCL);
  picomasteri2c->begin();
  
  // Check PCA9685 presence. interesting that the argument used in function defn is integer rather than hex? 
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

  Serial.println("[INIT] System ready. Streaming data every 100ms.");
  lastPublish = millis();
  lastMotorCommandTime = millis();  
  
  // Create synchronization primitives
  sensorMutex = xSemaphoreCreateMutex();
  sensorQueue = xQueueCreate(5, sizeof(SensorBuffer));
  motorCmdQueue = xQueueCreate(5, sizeof(MotorCommand));
  uartRxQueue = xQueueCreate(10, sizeof(MotorCommand));
  
  // Create tasks
  xTaskCreate(
    core0Task,        // Function
    "Core0Task",      // Name
    4096,             // Stack size (bytes)
    NULL,             // Parameters
    3,                // Priority (higher = more priority)
    NULL              // Task handle
  );
  
  xTaskCreate(
    core1Task,
    "Core1Task",
    4096,
    NULL,
    2,                // Lower priority than Core0
    NULL
  );
  
  // FreeRTOS scheduler starts automatically
}

void loop() {
  // Arduino loop() becomes idle when tasks are running
  vTaskDelay(pdMS_TO_TICKS(1000));
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
 
  DEBUG_PRINT(">sonar_rear_cm:");
  DEBUG_PRINTLN(sensors.sonarRear);

  DEBUG_PRINT(">cliff_front:");
  DEBUG_PRINTLN(sensors.cliffFront ? 1 : 0);
  DEBUG_PRINT(">cliff_rear:");
  DEBUG_PRINTLN(sensors.cliffRear ? 1 : 0);
}
