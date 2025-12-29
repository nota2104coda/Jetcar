#include "MotorDriver.h"
#include "./src/SimpleFOC.h"

//DC motors driven by Waveshare 4 motor driver with PCA9685 and TB6612FNG
//PCA9685 is on pins SDA 26, SCL 27 of Pico W
#define MOTOR_FRONT_RIGHT 0 /*red A1, white A2*/
#define MOTOR_REAR_RIGHT 1 /*white B1 red B2*/
#define MOTOR_FRONT_LEFT 2  /*white C1 red C2*/
#define MOTOR_REAR_LEFT 3	/*red D1 white D2*/

//motor encoders read on Pico
#define PIN_ENC_FRONT_RIGHT_Y 18
#define PIN_ENC_FRONT_RIGHT_G 19
#define PIN_ENC_FRONT_LEFT_Y 12
#define PIN_ENC_FRONT_LEFT_G 13
#define PIN_ENC_REAR_RIGHT_Y 16
#define PIN_ENC_REAR_RIGHT_G 17
#define PIN_ENC_REAR_LEFT_Y 14
#define PIN_ENC_REAR_LEFT_G 15


float i=0;
// 370 motors with hall encoders (6 pulses/rev × 4 = 24 counts)
Encoder motorRRenc(PIN_ENC_REAR_RIGHT_Y, PIN_ENC_REAR_RIGHT_G, 24);
Encoder motorRLenc(PIN_ENC_REAR_LEFT_Y, PIN_ENC_REAR_LEFT_G, 24);

// Interrupt callbacks
void isrRRA() { motorRRenc.handleA(); }
void isrRRB() { motorRRenc.handleB(); }
void isrRLA() { motorRLenc.handleA(); }
void isrRLB() { motorRLenc.handleB(); }
void setup()
{
    // Motor_test();
    Serial.begin(115200);
    // Initialize left motor encoder
    motorRLenc.init();
    motorRLenc.enableInterrupts(isrRLA, isrRLB);
    
    // Initialize right motor encoder  
    motorRRenc.init();
    motorRRenc.enableInterrupts(isrRRA, isrRRB);
    
    Serial.println("Encoders ready!");
}

void loop()
{
    // DEV_Delay_ms(1000);
    i+=0.05;
    Serial.print("loop");
    Serial.println(i);    
    // Update encoders (calculates velocity internally)
    motorRLenc.update();
    motorRRenc.update();
    
    // Get velocities in rad/s (direction included: + or -)
    float leftVel = motorRLenc.getVelocity();
    float rightVel = motorRRenc.getVelocity();
    // Convert to RPM
    float leftRPM = leftVel * 9.5493;   // 60/(2π)
    float rightRPM = rightVel * 9.5493;
    
    // Get positions too if needed
    float leftAngle = motorRLenc.getAngle();
    float rightAngle = motorRRenc.getAngle();
    //plot RR encoder counts
    Serial.print(">RightvalueA:"); 
    Serial.println(digitalRead(PIN_ENC_REAR_RIGHT_Y)); 
    Serial.print(">RightvalueB:"); 
    Serial.println(digitalRead(PIN_ENC_REAR_RIGHT_G)); 
    Serial.print(">Rightpulse_counter:"); 
    Serial.println(motorRRenc.pulse_counter); 
    //plot RL encoder counts
    Serial.print(">LeftvalueA:"); 
    Serial.println(motorRLenc.A_active); 
    Serial.print(">LeftvalueB:"); 
    Serial.println(motorRLenc.B_active); 
    Serial.print(">Leftrpm: "); 
    Serial.println(leftRPM); 
    Serial.print(">RightRPM: "); 
    Serial.println(rightRPM);
    Serial.println(" RPM");
    
    // delay(50);
}