#include <Wire.h>
#include <math.h>

// MPU6050 library is much simpler for filter/fusion
#include <MPU6050_tockn.h> 

// ====================== PIN & IMU SETUP ======================
// ESP8266 Pin Assignments (NodeMCU/Wemos D1 Mini)

#define I2C_SDA        4   // D2 on NodeMCU (GPIO4)
#define I2C_SCL        5   // D1 on NodeMCU (GPIO5)
#define MPU_ADDR       0x68 // Default I2C address for MPU6050

#define MOTOR_IN1      12  // D6 (GPIO12)
#define MOTOR_IN2      13  // D7 (GPIO13)
#define MOTOR_ENA      14  // D5 (GPIO14) - PWM capable

// Initialize MPU6050 object
MPU6050 mpu6050(Wire); 

// --- MANUAL IMU HELPERS REMOVED ---
// (readRegister, readRegister16, writeRegister functions removed)

// ====================== MOTOR HELPERS ======================
// ESP8266 PWM range is 0-1023, not 0-255

void motorCW(int speed) {
  speed = constrain(speed, 0, 255);
  int pwm8266 = map(speed, 0, 255, 0, 1023);  // Convert to ESP8266 range
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, pwm8266);
}

void motorCCW(int speed) {
  speed = constrain(speed, 0, 255);
  int pwm8266 = map(speed, 0, 255, 0, 1023);  // Convert to ESP8266 range
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, HIGH);
  analogWrite(MOTOR_ENA, pwm8266);
}

void motorStop() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);
}

// ====================== BALANCED CONTROL PARAMS ======================

float Kp = 40.0;      
float Kd = 25.0;

#define MAX_TILT_ANGLE  12.0
#define MIN_TILT_ANGLE -12.0
#define PWM_DEADZONE    10

// --- IMU FILTER VARIABLES REMOVED ---
// (alpha, angle_filtered, lastMicros removed)

// ====================== SETUP ======================

void setup() {
  // NOTE: You reduced this to 9600 in the provided code. Using 9600.
  Serial.begin(9600); 
  delay(2000);

  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  motorStop();

  // ESP8266: Specify I2C pins explicitly
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  Serial.println("\n===========================================");
  Serial.println("   BALANCED REACTION WHEEL CONTROL");
  Serial.println("   (ESP8266 - Library-Based IMU)");
  Serial.println("===========================================\n");

  // --- MPU6050_TOCKN Library Initialization ---
  mpu6050.begin();
  
  // Calibrate Gyro (essential for drift compensation)
  Serial.println("Calibrating gyroscope... KEEP ROBOT STILL!");
  mpu6050.calcGyroOffsets(true); 
  Serial.println("Calibration complete.\n");
  // ----------------------------------------------

  Serial.println("Settings:");
  Serial.print("  Kp = "); Serial.print(Kp); Serial.println(" (Proportional)");
  Serial.print("  Kd = "); Serial.print(Kd); Serial.println(" (Damping)");
  Serial.println("  Range: ±12°");
  Serial.println("  CPU: 80MHz (ESP8266)");
  Serial.println("===========================================\n");
}

// ====================== MAIN LOOP ======================

void loop() {
  // 1. Update the MPU data (reads registers, calculates fusion angle)
  mpu6050.update();
  
  // 2. Get the processed angle and angular rate from the library
  // We use AngleX/GyroX because the original filter used X-axis acceleration/gyro for tilt.
  float angle_raw = mpu6050.getAngleX(); 
  float angle_rate = mpu6050.getGyroX();

  // 3. Control Logic
  bool beyond_limit = (angle_raw > MAX_TILT_ANGLE || angle_raw < MIN_TILT_ANGLE);
  
  float angle = constrain(angle_raw, MIN_TILT_ANGLE, MAX_TILT_ANGLE);
  
  // PD Control
  float u = Kp * angle + Kd * angle_rate;
  u = -u; // Control signal inversion (to push against the tilt)

  int pwm = constrain((int)u, -255, 255);

  String motorDir = "STOP";
  
  // 4. Motor Action
  if (beyond_limit) {
    motorStop(); // Emergency Stop
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

  // 5. Serial Output (Debug)
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

  delay(15);
}