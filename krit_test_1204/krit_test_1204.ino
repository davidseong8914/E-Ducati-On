#include <Wire.h>
#include <math.h>
#include <MPU6050_tockn.h>  // Library for easy IMU handling

// ====================== PIN & IMU SETUP ======================
#define I2C_SDA        4   // D2 on NodeMCU (GPIO4)
#define I2C_SCL        5   // D1 on NodeMCU (GPIO5)
#define MPU_ADDR       0x68

#define MOTOR_IN1      12  // D6 (GPIO12)
#define MOTOR_IN2      13  // D7 (GPIO13)
#define MOTOR_ENA      14  // D5 (GPIO14) - PWM capable

MPU6050 mpu6050(Wire); 

// ====================== MOTOR HELPERS ======================
void motorCW(int speed) {
  speed = constrain(speed, 0, 255);
  int pwm8266 = map(speed, 0, 255, 0, 1023);
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, pwm8266);
}

void motorCCW(int speed) {
  speed = constrain(speed, 0, 255);
  int pwm8266 = map(speed, 0, 255, 0, 1023);
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, HIGH);
  analogWrite(MOTOR_ENA, pwm8266);
}

void motorStop() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);
}

// ====================== CONTROL PARAMS ======================

// IMPROVED for heavy shaky bike
float Kp = 50.0;      // Strong proportional gain
float Kd = 50.0;      // HIGH damping to stop oscillation

#define MAX_TILT_ANGLE  12.0
#define MIN_TILT_ANGLE -12.0
#define PWM_DEADZONE    10

// Gyro filtering - removes mechanical vibrations
float gyro_filtered = 0.0;
float gyro_alpha = 0.75;  // Smooth vibrations, keep responsiveness

// Fixed loop timing - consistent 100Hz control
unsigned long lastLoopTime = 0;
const unsigned long LOOP_INTERVAL = 10000;  // 10ms = 100Hz

// ====================== SETUP ======================

void setup() {
  Serial.begin(9600); 
  delay(2000);

  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  motorStop();

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  Serial.println("\n===========================================");
  Serial.println("   BALANCED REACTION WHEEL CONTROL");
  Serial.println("   (ESP8266 - Heavy Bike Optimized)");
  Serial.println("===========================================\n");

  // Initialize MPU6050 library
  mpu6050.begin();
  
  // Calibrate Gyro - CRITICAL: Keep bike completely still!
  Serial.println("Calibrating gyroscope... KEEP BIKE STILL!");
  mpu6050.calcGyroOffsets(true); 
  Serial.println("Calibration complete.\n");

  Serial.println("Settings:");
  Serial.print("  Kp = "); Serial.print(Kp); Serial.println(" (Strong for heavy mass)");
  Serial.print("  Kd = "); Serial.print(Kd); Serial.println(" (HIGH damping)");
  Serial.println("  Loop: 100Hz (fixed timing)");
  Serial.println("  Gyro Filter: ON (vibration removal)");
  Serial.println("  Range: ±12°");
  Serial.println("===========================================\n");
  
  lastLoopTime = micros();
}

// ====================== MAIN LOOP ======================

void loop() {
  // Fixed timing - maintain consistent 100Hz control loop
  unsigned long now = micros();
  
  if (now - lastLoopTime < LOOP_INTERVAL) {
    return;  // Wait for next cycle
  }
  
  lastLoopTime = now;

  // 1. Update MPU data (library handles complementary filter)
  mpu6050.update();
  
  // 2. Get angle from library (already filtered by library's complementary filter)
  float angle_raw = mpu6050.getAngleX(); 
  
  // 3. Get gyro rate and apply additional filtering for vibrations
  float gyro_raw = mpu6050.getGyroX();
  gyro_filtered = gyro_alpha * gyro_filtered + (1.0 - gyro_alpha) * gyro_raw;
  float angle_rate = gyro_filtered;  // Use filtered gyro
  
  // 4. Control Logic
  bool beyond_limit = (angle_raw > MAX_TILT_ANGLE || angle_raw < MIN_TILT_ANGLE);
  float angle = constrain(angle_raw, MIN_TILT_ANGLE, MAX_TILT_ANGLE);
  
  // PD Control - higher Kd reduces oscillation
  float u = Kp * angle + Kd * angle_rate;
  u = -u;  // Invert to push against tilt

  int pwm = constrain((int)u, -255, 255);

  String motorDir = "STOP";
  
  // 5. Motor Action - instant response, no slew rate
  if (beyond_limit) {
    motorStop();
    motorDir = "LIMIT";
    pwm = 0;
  } else if (abs(pwm) < PWM_DEADZONE) {
    motorStop();
    motorDir = "STOP";
    pwm = 0;
  } else if (pwm > 0) {
    motorCW(pwm);
    motorDir = "CW";
  } else {
    motorCCW(-pwm);
    motorDir = "CCW";
  }

  // 6. Serial Output (Debug)
  Serial.print("θ:");
  Serial.print(angle_raw, 1);
  Serial.print("° ");
  
  if (beyond_limit) {
    Serial.print("[OUT] ");
  }
  
  Serial.print("| PWM:");
  Serial.print(abs(pwm));
  Serial.print(" | ");
  Serial.println(motorDir);
  
}