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
#define LOOP_DEBUG_A 0
#define LOOP_DEBUG_B 1

#if LOOP_DEBUG_A
  #define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...) ((void)0)
  #define DEBUG_PRINTLN(...) ((void)0)
#endif
#if LOOP_DEBUG_B
  #define DEBUG_B_PRINT(...) Serial.print(__VA_ARGS__)
  #define DEBUG_B_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
  #define DEBUG_B_PRINT(...) ((void)0)
  #define DEBUG_B_PRINTLN(...) ((void)0)
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
volatile uint32_t Core1LoopCounter = 0;
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
  loopPeriod = pdMS_TO_TICKS(50); // 50ms loop
  
  static uint32_t lastCore0ChangeTime = 0;
  static uint32_t lastCore1LoopCounter = 0;
  static uint32_t lastCore1ChangeTime = 0;
  // Add a global boot counter (retained in RTC memory if possible)
  volatile uint32_t bootCounter = 0;
  
  
  while (1) {
    
    // Timing diagnostics in core0Task
    uint32_t t_start = millis();
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
    currentSensors.speedFL = motorDriver.getRPM(MOTOR_FL) * MOTOR_RPM_TO_CMPS;
    currentSensors.speedFR = motorDriver.getRPM(MOTOR_FR) * MOTOR_RPM_TO_CMPS;
    currentSensors.speedRL = motorDriver.getRPM(MOTOR_RL) * MOTOR_RPM_TO_CMPS;  
    currentSensors.speedRR = motorDriver.getRPM(MOTOR_RR) * MOTOR_RPM_TO_CMPS;
    // // Debug: print raw RPM readings
    // DEBUG_B_PRINT("Raw RPM FL: "); DEBUG_B_PRINTLN(currentSensors.speedFL);
    // DEBUG_B_PRINT("Raw RPM FR: "); DEBUG_B_PRINTLN(currentSensors.speedFR);
    // DEBUG_B_PRINT("Raw RPM RL: "); DEBUG_B_PRINTLN(currentSensors.speedRL);
    // DEBUG_B_PRINT("Raw RPM RR: "); DEBUG_B_PRINTLN(currentSensors.speedRR);
    currentSensors.cliffFront = frontCliff.getLastState();
    currentSensors.cliffRear = rearCliff.getLastState();
    currentSensors.timestamp = millis();
    // After sensor read and buffer update
    uint32_t t_afterSensors = millis();
    DEBUG_B_PRINT(">[TIMING] Sensors: ");
    DEBUG_B_PRINTLN(t_afterSensors - t_start);

    xSemaphoreGive(sensorMutex);
    
    // B. Check if sonar should be read (non-blocking flag from Core1)
    // Core1 will set a flag when sonar is ready
    if (sonarReadyFlag) {
      if (xSemaphoreTake(sensorMutex, portMAX_DELAY) == pdTRUE) {
        currentSensors.sonarRearcm = sonarDistanceRear; // From Core1
        currentSensors.sonarFrontcm = sonarDistanceFront; // From Core1
        xSemaphoreGive(sensorMutex);
      }
      sonarReadyFlag = false;
    }
    
    // C. Read UART command arbitration
    MotorCommand cmdUART = {0}, cmdFoxglove = {0}, cmdFinal = {0};
    if (xQueueReceive(uartRxQueue, &cmdUART, 0) == pdTRUE) {
      cmdFinal = cmdUART; // UART has priority
    }
    if (xQueueReceive(motorCmdQueue, &cmdFoxglove, 0) == pdTRUE && cmdFinal.tqFR == 0) {
      cmdFinal = cmdFoxglove; // Foxglove secondary
    }
    
    // D. Execute motor commands
     // Check motor safety timeouts (non-blocking)
    switch (motorDriver.checkMotorSafetyTimeouts(kMotorSafetyTimeoutMs)) { 
      case true:{
        if (systemState == SystemState::RUNNING || systemState == SystemState::DEGRADED) {
          DEBUG_PRINTLN("[SAFETY] Motor timeout - stopped inactive motors");
        }
        break;}
      case false:{
        // motorDriver.setMotor(MOTOR_FR, cmdFinal.tqFR);
        // motorDriver.setMotor(MOTOR_FL, cmdFinal.tqFL);
        // motorDriver.setMotor(MOTOR_RR, cmdFinal.tqRR);
        // motorDriver.setMotor(MOTOR_RL, cmdFinal.tqRL);
        motorDriver.setMotor(MOTOR_FR, 0.1); //for testing
        motorDriver.setMotor(MOTOR_FL, 0.1);
        motorDriver.setMotor(MOTOR_RR, 0.1);
        motorDriver.setMotor(MOTOR_RL, 0.1);
        lastMotorCommandTime = millis();
      }
    }

    // After motor command section
    uint32_t t_afterMotors = millis();
    DEBUG_B_PRINT(">[TIMING] Motors: ");
    DEBUG_B_PRINTLN(t_afterMotors - t_afterSensors);

    // Output telemetry to UART
    sendTelemetry(currentSensors);
    // After telemetry
    uint32_t t_afterTelemetry = millis();
    DEBUG_B_PRINT(">[TIMING] Telemetry: ");
    DEBUG_B_PRINTLN(t_afterTelemetry - t_afterMotors);

    // Watchdog feed with timeout-based stall detection
    uint32_t now = millis();
    if (Core1LoopCounter != lastCore1LoopCounter) {
        lastCore1LoopCounter = Core1LoopCounter;
        lastCore1ChangeTime = now;
    }
    // Timeout threshold in ms (e.g., 200ms)
    const uint32_t core1TimeoutMs = 200;
    if ((now - lastCore1ChangeTime) > core1TimeoutMs) {
        // Core1 is stalled, do not feed watchdog
        DEBUG_B_PRINTLN("[WATCHDOG] Core1 stalled (timeout), not feeding watchdog!");
    } else {
        watchdog_update(); // Only feed if Core1 is alive
    }
    
    // Heartbeat print should show up every 100ms
    DEBUG_B_PRINT(">[HEARTBEAT Core0] millis: ");
    DEBUG_B_PRINTLN(millis()-lastCore0ChangeTime);
    lastCore0ChangeTime = millis();    
    
    // At end of loop
    uint32_t t_end = millis();
    DEBUG_B_PRINT("[TIMING] Loop total: ");
    DEBUG_B_PRINTLN(t_end - t_start);

    // Wait until next loop period (blocks if early)
    vTaskDelayUntil(&lastWakeTime, loopPeriod);
  }
}

// Core 1 Task: WiFi + Sonar (blocking I/O tolerant)
void core1Task(void *pvParameters) {
  TickType_t sonarPollPeriod;
  TickType_t lastSonarTime;
  sonarPollPeriod = pdMS_TO_TICKS(50); // 50ms period
  lastSonarTime = xTaskGetTickCount();
  TickType_t lastWakeTime = xTaskGetTickCount();
  static uint32_t lastCore1LoopTime = 0;

  while (1) {
    // Increment heartbeat every loop
    Core1LoopCounter++;
  
    TickType_t now;
    now = xTaskGetTickCount();
    // A. Non-blocking sonar poll (only if time permits)

    if ((now - lastSonarTime) >= sonarPollPeriod) {
      int32_t reading = sonarR.ping_cm();
      DEBUG_B_PRINT(">Raw sonarR.ping_cm(): "); DEBUG_B_PRINTLN(reading);
      if (reading > 0 && reading < kMaxSonarRangecm ) {
        if (xSemaphoreTake(sensorMutex, portMAX_DELAY) == pdTRUE) {
          currentSensors.sonarRearcm = reading;
          xSemaphoreGive(sensorMutex);
        }
        sonarReadyFlag = true;
      }
      lastSonarTime = now;
    }
    delay(min_sonar_delayMs); // Delay to avoid crosstalk between sonars

    int32_t frontReading = sonarF.ping_cm();
    DEBUG_B_PRINT(">Raw sonarF.ping_cm(): "); DEBUG_B_PRINTLN(frontReading);
    // Atomically update both front and rear sonar readings
    if (frontReading > 0 && frontReading < kMaxSonarRangecm ) {
      if (xSemaphoreTake(sensorMutex, portMAX_DELAY) == pdTRUE) {
        currentSensors.sonarFrontcm = frontReading;
        // Use the last valid rear reading (already set above)
        // Optionally, you can re-read rear sonar here if needed
        // currentSensors.sonarRearcm = rearReading;
        xSemaphoreGive(sensorMutex);
      }
      sonarReadyFlag = true;
    }
    lastSonarTime = now;

    // B. WiFi operations (can block here)
    // Read commands from Foxglove, post to motorCmdQueue
    // Send telemetry via WiFi

    // Heartbeat print. should be 50ms each time
    DEBUG_B_PRINT(">[HEARTBEAT Core1] millis: ");
    DEBUG_B_PRINTLN(millis() - lastCore1LoopTime);
    lastCore1LoopTime = millis();
    vTaskDelayUntil(&lastWakeTime, sonarPollPeriod);
  }
}

// Add a global boot counter (retained in RTC memory if possible)
volatile uint32_t bootCounter = 0;

void setup() {
  bootCounter++;
  Serial.print("[BOOT] Count: ");
  Serial.println(bootCounter);

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
