#include <WiFi.h>
#include <WebServer.h>

const char* SSID = "Bittle";
const char* PASS = "password";

WebServer server(80);

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(250); Serial.print("."); }
  Serial.printf("\nIP: %s  GW: %s\n",
      WiFi.localIP().toString().c_str(),
      WiFi.gatewayIP().toString().c_str());

  server.on("/", [](){
    server.send(200, "text/plain", "ESP32 web server is alive.");
  });
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
