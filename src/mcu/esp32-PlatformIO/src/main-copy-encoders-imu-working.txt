
#include <SimpleFOC.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define MASTER_PICOW_4WD_NONSTEER_RUBBERWHL_2XSONAR_2xCLIFF
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>

static constexpr float kGearRatio = 46.0f;

#include "../../../../libs/arduino/CarConfigurations.h"
#include "../../../../libs/arduino/RobotCarPinDefinitionsAndMore.h"

arduino::MbedI2C myWire(PICOW_I2C0_SDA, PICOW_I2C0_SCL);
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(MOTOR_DRV_ADDR, myWire);
Adafruit_MPU6050 mpu;
//note how the left encoder sequence is GY vs right encoders are YG
Encoder encFR = Encoder(PIN_ENC_FRONT_RIGHT_Y, PIN_ENC_FRONT_RIGHT_G, PULSE_PER_REV);
Encoder encFL = Encoder(PIN_ENC_FRONT_LEFT_G,PIN_ENC_FRONT_LEFT_Y, PULSE_PER_REV);
Encoder encRR = Encoder(PIN_ENC_REAR_RIGHT_Y, PIN_ENC_REAR_RIGHT_G, PULSE_PER_REV);
Encoder encRL = Encoder(PIN_ENC_REAR_LEFT_G,PIN_ENC_REAR_LEFT_Y,  PULSE_PER_REV);
unsigned long lastPublish = 0;

void encFRA() { encFR.handleA(); }
void encFRB() { encFR.handleB(); }
void encRRA() { encRR.handleA(); }
void encRRB() { encRR.handleB(); }
void encFLA() { encFL.handleA(); }
void encFLB() { encFL.handleB(); }
void encRLA() { encRL.handleA(); }
void encRLB() { encRL.handleB(); }

void setMotor(uint8_t motorNum, float speed) {
  uint8_t pwmCh = motorNum * 3;
  uint8_t in1Ch = motorNum * 3 + 1;
  uint8_t in2Ch = motorNum * 3 + 2;

  if (speed > 1.0) speed = 1.0;
  if (speed < -1.0) speed = -1.0;

  uint16_t pwm_val = (uint16_t)(abs(speed) * 4095);
  pwm.setPWM(pwmCh, 0, pwm_val);

  if (speed > 0) {
    pwm.setPWM(in1Ch, 0, 4095);
    pwm.setPWM(in2Ch, 0, 0);
  } else if (speed < 0) {
    pwm.setPWM(in1Ch, 0, 0);
    pwm.setPWM(in2Ch, 0, 4095);
  } else {
    pwm.setPWM(in1Ch, 0, 0);
    pwm.setPWM(in2Ch, 0, 0);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("Initializing Pico W I2C bus, encoders, and sensors...");
  myWire.begin();
  pwm.begin();
  pwm.setPWMFreq(1600);
  delay(10);

  Serial.println("PCA9685 ready; enabling encoders.");
  encFR.init();
  encFR.enableInterrupts(encFRA, encFRB);
  encRR.init();
  encRR.enableInterrupts(encRRA, encRRB);
  encFL.init();
  encFL.enableInterrupts(encFLA, encFLB);
  encRL.init();
  encRL.enableInterrupts(encRLA, encRLB);

  Serial.println("Encoders initialized.");

  if (!mpu.begin(MPU6050_I2CADDR_DEFAULT, &myWire)) {
    Serial.println("MPU6050 not detected. Check wiring.");
    while (true) {
      delay(1000);
      Serial.println("MPU6050 not found; verify power and I2C.");
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 ready; streaming wheel RPMs and sensor data every 100ms.");
  lastPublish = millis();
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastPublish >= 100) {
    encFR.update();
    encRR.update();
    encFL.update();
    encRL.update();

    float vel1 = encFR.getVelocity() / kGearRatio;
    float vel2 = encRR.getVelocity() / kGearRatio;
    float vel3 = encFL.getVelocity() / kGearRatio;
    float vel4 = encRL.getVelocity() / kGearRatio;

    float rpm1 = (vel1 * 60.0) / (2.0 * PI);
    float rpm2 = (vel2 * 60.0) / (2.0 * PI);
    float rpm3 = (vel3 * 60.0) / (2.0 * PI);
    float rpm4 = (vel4 * 60.0) / (2.0 * PI);

    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    mpu.getEvent(&accel, &gyro, &temp);

    Serial.print(">rpm1:");
    Serial.println(rpm1, 1);
    Serial.print(">rpm2:");
    Serial.println(rpm2, 1);
    Serial.print(">rpm3:");
    Serial.println(rpm3, 1);
    Serial.print(">rpm4:");
    Serial.println(rpm4, 1);

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

    lastPublish = currentTime;
  }
}
