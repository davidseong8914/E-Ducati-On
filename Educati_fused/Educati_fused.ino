#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include <ESP32Servo.h>

// ===== WiFi =====
const char* SSID = "Bittle";
const char* PASS = "password";

WebServer server(80);

// ===== Servo Steering =====
#define servoPWM       13
Servo steeringServo;

// ===== L298N - Thrust Motor (Channel A) =====
#define ENThrust       25
#define thrust1        26
#define thrust2        27

// ===== L298N - Reaction Motor (Channel B) =====
#define ENReaction     32
#define reaction1      33
#define reaction2      14

// ===== BNO085 I2C =====
#define I2C_SCL        22
#define I2C_SDA        21
#define REPORT_HZ      50

Adafruit_BNO08x bno;
sh2_SensorValue_t ev;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ========== HTML UI ==========
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 Thruster & Servo Control</title>
  <style>
    body { font-family: sans-serif; padding: 20px; }
    h1 { margin-bottom: 0.2em; }
    .group { margin: 16px 0; }
    button {
      padding: 8px 14px;
      margin: 4px;
      font-size: 14px;
      cursor: pointer;
    }
  </style>
</head>
<body>
  <h1>ESP32 Control Panel</h1>
  <p>Control thrusters and steering servo.</p>

  <div class="group">
    <h3>Thrust Motor</h3>
    <button onclick="send('/thrust/fwd')">Forward</button>
    <button onclick="send('/thrust/rev')">Reverse</button>
    <button onclick="send('/thrust/stop')">Stop</button>
  </div>

  <div class="group">
    <h3>Reaction Motor</h3>
    <button onclick="send('/reaction/fwd')">Forward</button>
    <button onclick="send('/reaction/rev')">Reverse</button>
    <button onclick="send('/reaction/stop')">Stop</button>
  </div>

  <div class="group">
    <h3>Steering Servo</h3>
    <button onclick="send('/servo/left')">Left</button>
    <button onclick="send('/servo/center')">Center</button>
    <button onclick="send('/servo/right')">Right</button>
  </div>

  <p id="status"></p>

  <script>
    async function send(path) {
      try {
        const res = await fetch(path);
        const txt = await res.text();
        document.getElementById('status').innerText = txt;
      } catch (e) {
        document.getElementById('status').innerText = 'Request failed';
      }
    }
  </script>
</body>
</html>
)rawliteral";

// ========== Utils ==========
static inline uint32_t hzToMicros(uint16_t hz) {
  if (hz < 1) hz = 1;
  return (uint32_t)(1000000UL / hz);
}

static void quatToEuler(float w,float x,float y,float z,
                        float &roll,float &pitch,float &yaw) {
  float sinr_cosp = 2.0f * (w * x + y * z);
  float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
  roll = atan2f(sinr_cosp, cosr_cosp);

  float sinp = 2.0f * (w * y - z * x);
  if (fabsf(sinp) >= 1.0f)
    pitch = copysignf(M_PI / 2.0f, sinp);
  else
    pitch = asinf(sinp);

  float siny_cosp = 2.0f * (w * z + x * y);
  float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
  yaw = atan2f(siny_cosp, cosy_cosp);
}

// ===== Motor Control Helpers =====
void thrustForward() {
  digitalWrite(ENThrust, HIGH);
  digitalWrite(thrust1, HIGH);
  digitalWrite(thrust2, LOW);
}

void thrustReverse() {
  digitalWrite(ENThrust, HIGH);
  digitalWrite(thrust1, LOW);
  digitalWrite(thrust2, HIGH);
}

void thrustStop() {
  digitalWrite(ENThrust, LOW);
  digitalWrite(thrust1, LOW);
  digitalWrite(thrust2, LOW);
}

void reactionForward() {
  digitalWrite(ENReaction, HIGH);
  digitalWrite(reaction1, HIGH);
  digitalWrite(reaction2, LOW);
}

void reactionReverse() {
  digitalWrite(ENReaction, HIGH);
  digitalWrite(reaction1, LOW);
  digitalWrite(reaction2, HIGH);
}

void reactionStop() {
  digitalWrite(ENReaction, LOW);
  digitalWrite(reaction1, LOW);
  digitalWrite(reaction2, LOW);
}

// ===== Servo Positions =====
void servoLeft()   { steeringServo.write(45);  }
void servoCenter() { steeringServo.write(90);  }
void servoRight()  { steeringServo.write(135); }

// ===== HTTP Handlers =====
void handleRoot()          { server.send_P(200, "text/html", MAIN_page); }

void handleThrustFwd()     { thrustForward();    server.send(200, "text/plain", "Thrust: FORWARD"); }
void handleThrustRev()     { thrustReverse();    server.send(200, "text/plain", "Thrust: REVERSE"); }
void handleThrustStop()    { thrustStop();       server.send(200, "text/plain", "Thrust: STOP"); }

void handleReactionFwd()   { reactionForward();  server.send(200, "text/plain", "Reaction: FORWARD"); }
void handleReactionRev()   { reactionReverse();  server.send(200, "text/plain", "Reaction: REVERSE"); }
void handleReactionStop()  { reactionStop();     server.send(200, "text/plain", "Reaction: STOP"); }

void handleServoLeft()     { servoLeft();        server.send(200, "text/plain", "Servo: LEFT"); }
void handleServoCenter()   { servoCenter();      server.send(200, "text/plain", "Servo: CENTER"); }
void handleServoRight()    { servoRight();       server.send(200, "text/plain", "Servo: RIGHT"); }

// ===== Simple I2C Scan (debug) =====
void i2cScan() {
  Serial.println("Scanning I2C...");
  byte count = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Found 0x");
      Serial.println(addr, HEX);
      count++;
    }
  }
  if (!count) Serial.println("  No I2C devices found");
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(300);

  // Motor pins
  pinMode(ENThrust, OUTPUT);
  pinMode(thrust1, OUTPUT);
  pinMode(thrust2, OUTPUT);
  pinMode(ENReaction, OUTPUT);
  pinMode(reaction1, OUTPUT);
  pinMode(reaction2, OUTPUT);

  thrustStop();
  reactionStop();

  // Servo (keep powered correctly & share GND)
  steeringServo.attach(servoPWM);
  steeringServo.write(90);

  // I2C bus
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(50);

  i2cScan();   // check that 0x4A (or 0x4B) shows up

  Serial.println("Init BNO08x...");
  // Explicitly specify address and Wire to match your working setup.
  if (!bno.begin_I2C(0x4A, &Wire)) {
    Serial.println("ERROR: BNO08x not detected at 0x4A. Check scan output.");
    // Don't spam forever; just stop here for debugging.
    while (true) {
      delay(1000);
    }
  }
  Serial.println("BNO08x found!");

  // Only now bump speed
  Wire.setClock(400000);

  if (!bno.enableReport(SH2_GAME_ROTATION_VECTOR, hzToMicros(REPORT_HZ))) {
    Serial.println("ERROR: Failed to enable GRV report");
    while (true) {
      delay(1000);
    }
  }

  Serial.print("BNO08x OK. Output rate = ");
  Serial.print(REPORT_HZ);
  Serial.println(" Hz");

  // WiFi (after IMU is happy)
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(SSID, PASS);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());

  // Routes
  server.on("/", handleRoot);

  server.on("/thrust/fwd",    handleThrustFwd);
  server.on("/thrust/rev",    handleThrustRev);
  server.on("/thrust/stop",   handleThrustStop);

  server.on("/reaction/fwd",  handleReactionFwd);
  server.on("/reaction/rev",  handleReactionRev);
  server.on("/reaction/stop", handleReactionStop);

  server.on("/servo/left",    handleServoLeft);
  server.on("/servo/center",  handleServoCenter);
  server.on("/servo/right",   handleServoRight);

  server.begin();
  Serial.println("HTTP server started");
}

// ========== LOOP ==========
void loop() {
  server.handleClient();

  // IMU reading (non-blocking)
  static uint32_t lastPrint = 0;
  const uint32_t printPeriodMs = 1000UL / REPORT_HZ;

  if (bno.getSensorEvent(&ev) && ev.sensorId == SH2_GAME_ROTATION_VECTOR) {
    uint32_t now = millis();
    if (now - lastPrint < printPeriodMs) return;
    lastPrint = now;

    float w = ev.un.gameRotationVector.real;
    float x = ev.un.gameRotationVector.i;
    float y = ev.un.gameRotationVector.j;
    float z = ev.un.gameRotationVector.k;

    float roll, pitch, yaw;
    quatToEuler(w, x, y, z, roll, pitch, yaw);

    roll  *= 180.0f / M_PI;
    pitch *= 180.0f / M_PI;
    yaw   *= 180.0f / M_PI;

    Serial.printf("GRV quat: w=%.4f x=%.4f y=%.4f z=%.4f | ", w, x, y, z);
    Serial.printf("Euler(deg) roll=%.2f pitch=%.2f yaw=%.2f\n",
                  roll, pitch, yaw);
  }
}
