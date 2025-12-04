#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Servo.h> 
// mpu6050 for esp8266
#include <MPU6050_tockn.h>

/* David's Hotspot */
const char* SSID = "Bittle";
const char* PASS = "password";

ESP8266WebServer server(80);
MPU6050 mpu6050(Wire); 

/* Steering               */
#define servoPWM       13 // D7
Servo steeringServo;

/* L298N - Thrust Motor   */
#define ENThrust       12 // D6 - NOW PWM PIN
#define thrust1        14 // D5
#define thrust2        16 // D0

/* L298N - Reaction Motor */
// WARNING: These pins have been MOVED to non-I2C pins to prevent conflict.
// I2C uses default: SDA=GPIO4 (D2), SCL=GPIO5 (D1)
#define ENReaction     15 // D8
#define reaction1      0  // D3
#define reaction2      2  // D4

/* I2C */      // for MPU6050
// #define SDA // D2
// #define SCL // D1

#define PI 3.14

/* Function to set speed and direction via PWM */
void setThrustSpeed(int speed) {
  // speed ranges from -255 (full reverse) to +255 (full forward)
  
  int pwm_duty = abs(speed);
  if (pwm_duty > 255) pwm_duty = 255;

  if (speed == 0) {
    // Stop motor (active brake for fast halt)
    digitalWrite(thrust1, HIGH);
    digitalWrite(thrust2, HIGH);
    analogWrite(ENThrust, 0); // No power to the enable pin
  } else if (speed > 0) {
    // Forward
    digitalWrite(thrust1, HIGH);
    digitalWrite(thrust2, LOW);
    analogWrite(ENThrust, pwm_duty);
  } else {
    // Reverse
    digitalWrite(thrust1, LOW);
    digitalWrite(thrust2, HIGH);
    analogWrite(ENThrust, pwm_duty);
  }
}

/* HTML UI */
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>E-DUCATI-ON Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    :root {
      --ducati-red: #ff0033;
      --ducati-red-soft: #ff3355;
      --accent-yellow: #ffd60a;
      --dark-bg: #050509;
      --card-bg: #11111a;
      --grid-line: #262635;
    }

    * {
      box-sizing: border-box;
    }

    body {
      font-family: system-ui, -apple-system, BlinkMacSystemFont, sans-serif;
      padding: 16px;
      margin: 0;
      background: radial-gradient(circle at top, #1a0006 0, #06060a 40%, #020205 100%);
      color: #f5f5f5;
      text-align: center;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      justify-content: flex-start;
    }

    body::before {
      content: "";
      position: fixed;
      inset: 0;
      pointer-events: none;
      background-image:
        linear-gradient(rgba(255,255,255,0.035) 1px, transparent 1px),
        linear-gradient(90deg, rgba(255,255,255,0.035) 1px, transparent 1px);
      background-size: 40px 40px;
      opacity: 0.4;
      mix-blend-mode: soft-light;
      z-index: -1;
    }

    h1 {
      margin: 4px 0 0;
      font-size: 2.1rem;
      letter-spacing: 0.28em;
      text-transform: uppercase;
      color: var(--ducati-red);
      text-shadow: 0 0 10px rgba(255,0,51,0.7);
    }

    #subtitle {
      margin-top: 2px;
      font-size: 0.8rem;
      text-transform: uppercase;
      letter-spacing: 0.25em;
      color: #a1a1b5;
    }

    .row {
      display: flex;
      flex-wrap: wrap;
      justify-content: center;
      margin-top: 20px;
      gap: 18px;
    }

    .card {
      background: linear-gradient(135deg, rgba(255,0,51,0.08), rgba(0,0,0,0.7));
      border-radius: 18px;
      padding: 16px 14px 18px;
      min-width: 260px;
      max-width: 320px;
      box-shadow:
        0 0 25px rgba(255,0,51,0.25),
        0 12px 30px rgba(0,0,0,0.85);
      border: 1px solid rgba(255,255,255,0.06);
      position: relative;
      overflow: hidden;
    }

    .card::before {
      content: "";
      position: absolute;
      inset: -40%;
      background:
        radial-gradient(circle at 0 0, rgba(255,255,255,0.08), transparent 55%),
        radial-gradient(circle at 100% 0, rgba(255,0,51,0.13), transparent 50%);
      opacity: 0.45;
      mix-blend-mode: soft-light;
      pointer-events: none;
    }

    .card h3 {
      margin: 0 0 12px;
      font-size: 1rem;
      text-transform: uppercase;
      letter-spacing: 0.18em;
    }

    .card h3::before {
      content: "/// ";
      color: var(--ducati-red-soft);
    }

    button {
      padding: 10px 20px;
      margin: 4px;
      font-size: 0.95rem;
      cursor: pointer;
      border-radius: 999px;
      border: 1px solid rgba(255,255,255,0.1);
      background: radial-gradient(circle at 20% 0, #ffffff22, #ff0033ee);
      color: white;
      font-weight: 700;
      min-width: 120px;
      text-transform: uppercase;
      letter-spacing: 0.1em;
      box-shadow: 0 0 12px rgba(255,0,51,0.7);
      transition: transform 0.08s ease-out, box-shadow 0.08s ease-out, filter 0.08s;
    }

    button:active {
      transform: scale(0.95) translateY(1px);
      box-shadow: 0 0 4px rgba(255,0,51,0.5);
      filter: brightness(0.9);
    }

    button.secondary {
      background: radial-gradient(circle at 20% 0, #ffffff11, #1f2937);
      box-shadow: 0 0 8px rgba(148,163,184,0.4);
      border-color: rgba(148,163,184,0.4);
    }

    button.danger {
      background: radial-gradient(circle at 10% 0, #ffffff22, #ff0000);
      box-shadow: 0 0 16px rgba(255,0,0,0.9);
      border-color: rgba(255,0,0,0.7);
      position: relative;
      overflow: hidden;
    }

    button.danger::after {
      content: "";
      position: absolute;
      inset: 0;
      background: linear-gradient(120deg, transparent 0%, rgba(255,255,255,0.5) 50%, transparent 100%);
      opacity: 0;
      transform: translateX(-100%);
      pointer-events: none;
    }

    button.danger:active::after {
      opacity: 0.85;
      transform: translateX(100%);
      transition: transform 0.15s ease-out, opacity 0.15s ease-out;
    }

    /* Joystick */
    #joystick-container {
      width: 260px;
      height: 120px;
      margin: 0 auto;
      border-radius: 80px;
      background: radial-gradient(circle at 50% 0, #ff335533, #050712 60%);
      position: relative;
      touch-action: none;
      overflow: hidden;
      border: 1px solid rgba(255,255,255,0.15);
      box-shadow: inset 0 0 14px rgba(0,0,0,0.9);
    }

    #joystick-track {
      position: absolute;
      left: 12px;
      right: 12px;
      top: 50%;
      height: 5px;
      background: linear-gradient(90deg, #333647, #f97316, #333647);
      transform: translateY(-50%);
      border-radius: 999px;
      box-shadow: 0 0 10px rgba(249,115,22,0.8);
    }

    #joystick-thumb {
      width: 44px;
      height: 44px;
      border-radius: 50%;
      background: radial-gradient(circle at 30% 20%, #ffffffdd, #f97316);
      position: absolute;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
      box-shadow:
        0 0 14px rgba(249,115,22,0.9),
        0 0 26px rgba(255,0,51,0.8);
      touch-action: none;
      border: 2px solid rgba(0,0,0,0.8);
    }

    #angle-value {
      font-weight: 700;
      color: var(--accent-yellow);
      text-shadow: 0 0 6px rgba(255,214,10,0.8);
    }

    #status {
      margin-top: 14px;
      font-size: 0.78rem;
      color: #9ca3af;
      font-family: "SF Mono", ui-monospace, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
    }

    #status::before {
      content: "ENGINE STATUS: ";
      color: var(--accent-yellow);
    }

    .rpm-bar {
      margin-top: 8px;
      width: 100%;
      height: 6px;
      border-radius: 999px;
      background: linear-gradient(90deg, #22c55e, #eab308, #f97316, #ff0000);
      position: relative;
      overflow: hidden;
    }

    .rpm-bar::after {
      content: "";
      position: absolute;
      inset: 0;
      background: linear-gradient(120deg, transparent 0%, rgba(255,255,255,0.6) 50%, transparent 100%);
      mix-blend-mode: screen;
      opacity: 0.45;
      animation: sweep 1.3s infinite linear;
    }

    @keyframes sweep {
      0% { transform: translateX(-100%); }
      100% { transform: translateX(100%); }
    }

    .label-small {
      font-size: 0.7rem;
      text-transform: uppercase;
      letter-spacing: 0.18em;
      color: #9ca3af;
      margin-top: 6px;
    }

    @media (max-width: 640px) {
      h1 {
        font-size: 1.7rem;
        letter-spacing: 0.22em;
      }
      .card {
        min-width: 260px;
      }
    }
    
    /* New Slider Styles */
    #speed-slider {
        width: 100%;
        height: 8px;
        appearance: none;
        background: linear-gradient(90deg, #ff0000, #ff0000, #333647 49%, #333647 51%, #05a7ff, #05a7ff);
        background-position: 50% 50%;
        background-size: 200% 100%;
        border-radius: 4px;
        margin: 12px 0 10px;
        cursor: pointer;
        box-shadow: 0 0 10px rgba(0,0,0,0.5);
    }
    #speed-slider::-webkit-slider-thumb {
        appearance: none;
        width: 24px;
        height: 24px;
        border-radius: 50%;
        background: radial-gradient(circle at 30% 20%, #ffffff, #05a7ff);
        cursor: pointer;
        border: 2px solid #0a0a0a;
        box-shadow: 0 0 8px rgba(0,167,255,0.8);
        transition: transform 0.1s;
    }
    #speed-slider::-moz-range-thumb {
        width: 24px;
        height: 24px;
        border-radius: 50%;
        background: radial-gradient(circle at 30% 20%, #ffffff, #05a7ff);
        cursor: pointer;
        border: 2px solid #0a0a0a;
        box-shadow: 0 0 8px rgba(0,167,255,0.8);
        transition: transform 0.1s;
    }
    #speed-text {
        font-weight: 700;
        font-size: 1.2rem;
        color: #05a7ff;
        text-shadow: 0 0 6px rgba(5, 167, 255, 0.8);
    }
  </style>
</head>
<body>
  <h1>E-DUCATI-ON</h1>
  <div id="subtitle">SELF-BALANCING ELECTRIC DUCATI PROTOTYPE</div>

  <div class="row">
    <div class="card">
      <h3>Steering</h3>
      <div id="joystick-container">
        <div id="joystick-track"></div>
        <div id="joystick-thumb"></div>
      </div>
      <p style="margin-top:10px;">STEER ANGLE: <span id="angle-value">90</span>°</p>
      <div class="label-small">MODE: MANUAL FRONT-END INPUT</div>
      <div class="rpm-bar"></div>
    </div>

    <div class="card">
      <h3>Thrust PWM Control</h3>
      <input type="range" id="speed-slider" min="-255" max="255" value="0">
      <p>SPEED: <span id="speed-text">0</span> / 255</p>
      <button class="danger" onclick="sendDrive(0)">EMERGENCY STOP (Brake)</button>
      <h3 style="margin-top: 18px;">Reaction Control</h3>
      <button class="secondary" onclick="send('/reaction/fwd')">React Fwd</button>
      <button class="secondary" onclick="send('/reaction/rev')">React Rev</button>
    </div>
  </div>

  <p id="status">IDLE</p>

  <script>
    async function send(path) {
      try {
        const res = await fetch(path);
        const txt = await res.text();
        document.getElementById('status').innerText = txt;
      } catch (e) {
        document.getElementById('status').innerText = 'COMM LINK LOST';
      }
    }
    
    const speedSlider = document.getElementById('speed-slider');
    const speedText = document.getElementById('speed-text');
    let lastSentSpeed = 0;

    async function sendDrive(speed) {
        if (speed === lastSentSpeed) return;
        lastSentSpeed = speed;
        speedText.textContent = speed;
        try {
            await fetch('/drive/set?speed=' + speed);
            document.getElementById('status').innerText = "THRUST: " + (speed > 0 ? 'FWD' : speed < 0 ? 'REV' : 'STOP') + " @ " + Math.abs(speed);
        } catch (e) {
            document.getElementById('status').innerText = 'COMM LINK LOST';
        }
    }

    speedSlider.addEventListener('input', (e) => {
        // Send the value every time the slider moves
        sendDrive(parseInt(e.target.value));
    });
    
    speedSlider.addEventListener('touchend', (e) => {
        // Snap back to 0 on touch release
        e.target.value = 0;
        sendDrive(0);
    });

    // === Steering Joystick ===
    const container = document.getElementById('joystick-container');
    const thumb = document.getElementById('joystick-thumb');
    const angleSpan = document.getElementById('angle-value');

    let isDragging = false;
    let lastSentAngle = 90;
    const minAngle = 0;
    const maxAngle = 180;

    function setThumbFromAngle(angle) {
      const rect = container.getBoundingClientRect();
      const trackWidth = rect.width - 44; // thumb width
      const t = (angle - 90) / 90; // -1 .. 1
      const x = rect.width/2 + t * (trackWidth/2);
      thumb.style.left = x + "px";
    }

    function angleFromPointer(clientX) {
      const rect = container.getBoundingClientRect();
      const halfWidth = rect.width / 2;
      let dx = clientX - (rect.left + halfWidth);

      const maxDx = halfWidth - 22; // leave margins
      if (dx >  maxDx) dx =  maxDx;
      if (dx < -maxDx) dx = -maxDx;

      const t = dx / maxDx; // -1 .. 1
      let angle = 90 + t * 90; // 0..180
      if (angle < minAngle) angle = minAngle;
      if (angle > maxAngle) angle = maxAngle;
      return Math.round(angle);
    }

    async function sendAngle(angle) {
      if (angle === lastSentAngle) return;
      lastSentAngle = angle;
      angleSpan.textContent = angle;
      try {
        await fetch('/servo/set?angle=' + angle);
        document.getElementById('status').innerText = "STEER " + angle + "°";
      } catch (e) {
        document.getElementById('status').innerText = 'COMM LINK LOST';
      }
    }

    function onPointerDown(e) {
      isDragging = true;
      container.setPointerCapture(e.pointerId);
      const angle = angleFromPointer(e.clientX);
      setThumbFromAngle(angle);
      sendAngle(angle);
    }

    function onPointerMove(e) {
      if (!isDragging) return;
      const angle = angleFromPointer(e.clientX);
      setThumbFromAngle(angle);
      sendAngle(angle);
    }

    function onPointerUp(e) {
      isDragging = false;
      container.releasePointerCapture(e.pointerId);
      // snap back to center (90 deg)
      setThumbFromAngle(90);
      sendAngle(90);
    }

    container.addEventListener('pointerdown', onPointerDown);
    container.addEventListener('pointermove', onPointerMove);
    container.addEventListener('pointerup', onPointerUp);
    container.addEventListener('pointercancel', onPointerUp);

    // Initialize thumb in center on load
    window.addEventListener('load', () => {
      setThumbFromAngle(90);
    });
  </script>
</body>
</html>
)rawliteral";

/* Control Functions */

// THRUST MOTOR
  // Thrust Motor now uses PWM control
  void setThrustSpeed(int speed) {
    // speed ranges from -255 (full reverse) to +255 (full forward)
    
    int pwm_duty = abs(speed);
    if (pwm_duty > 255) pwm_duty = 255;

    if (speed == 0) {
      // Active brake
      digitalWrite(thrust1, HIGH);
      digitalWrite(thrust2, HIGH);
      analogWrite(ENThrust, 0); 
    } else if (speed > 0) {
      // Forward
      digitalWrite(thrust1, HIGH);
      digitalWrite(thrust2, LOW);
      analogWrite(ENThrust, pwm_duty);
    } else {
      // Reverse
      digitalWrite(thrust1, LOW);
      digitalWrite(thrust2, HIGH);
      analogWrite(ENThrust, pwm_duty);
    }
  }

// REACTION MOTOR

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

/* STEERING */
  void steer(int angleDeg) {
    if (angleDeg < 0)   angleDeg = 0;
    if (angleDeg > 180) angleDeg = 180;
    steeringServo.write(angleDeg);
  }

/* HTTP Handlers */
void handleRoot() {
  server.send_P(200, "text/html", MAIN_page);
}

// ---- Drive endpoints for UI buttons ----
void handleDriveSpeed() {
  if (!server.hasArg("speed")) {
    server.send(400, "text/plain", "Missing 'speed' parameter");
    return;
  }
  int speed = server.arg("speed").toInt();
  setThrustSpeed(speed);
  
  String msg = "Thrust set to ";
  msg += speed;
  server.send(200, "text/plain", msg);
}

// New Handlers for Reaction Motor (retained simple ON/OFF)
void handleReactionFwd() {
  reactionForward();
  server.send(200, "text/plain", "Reaction: FORWARD");
}
void handleReactionRev() {
  reactionReverse();
  server.send(200, "text/plain", "Reaction: REVERSE");
}


/* Servo angle endpoint for joystick */
void handleServoSet() {
  if (!server.hasArg("angle")) {
    server.send(400, "text/plain", "Missing 'angle' parameter");
    return;
  }
  int angle = server.arg("angle").toInt();
  steer(angle);

  String msg = "Servo angle set to ";
  msg += angle;
  msg += " deg";
  server.send(200, "text/plain", msg);
}

long mpu_timer = 0; // Timer to control MPU update frequency

/* SETUP */
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

  setThrustSpeed(0); // Active brake at startup
  reactionStop();

  // Servo (keep powered correctly & share GND)
  steeringServo.attach(servoPWM);
  steeringServo.write(90);

  // MPU6050
  Wire.begin(); //SCL=D1 and SDA=D2
  mpu6050.begin();
  
  Serial.println("Self balancing initialized");

  // Calibrate Gyro 
  Serial.println("Calibrating gyroscope, hold still");
  mpu6050.calcGyroOffsets(true); 
  Serial.println("Calibration complete.");

  mpu_timer = millis(); // Initialize the MPU timer

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  // print IP address
  Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());

  // Routes
  server.on("/", handleRoot);

  // New PWM Speed Handler
  server.on("/drive/set", handleDriveSpeed); 
  
  // Reaction Motor Handlers
  server.on("/reaction/fwd", handleReactionFwd);
  server.on("/reaction/rev", handleReactionRev);

  // Old Servo Handler
  server.on("/servo/set", handleServoSet);

  server.begin();
  Serial.println("HTTP server started");
}

/* LOOP */
void loop() {
  server.handleClient();
  
  mpu6050.update(); 

  // Print data every 100 milliseconds
  if (millis() - mpu_timer > 100) {

    // Print calculated Euler angles (Roll and Pitch are most common)
    Serial.print("Roll: ");
    Serial.print(mpu6050.getAngleX()); 

    Serial.print(" | Pitch: ");
    Serial.print(mpu6050.getAngleY());
    
    // Yaw is less reliable without a magnetometer (MPU6050 doesn't have one)
    Serial.print(" | Yaw: ");
    Serial.println(mpu6050.getAngleZ());

    mpu_timer = millis();
  }
}