#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <TelnetStream.h> // https://github.com/jandrassy/TelnetStream
#include <ArduinoOTA.h>
#include <time.h>
#include "Secrets.h"

/* Secrets.h file should contain data as below: */
#ifndef WIFI_SSID
#define WIFI_SSID "xxxxxxxxxx"
#define WIFI_PASSWORD "xxxxxxxxxx"
#define IFTTT_ERROR_URL "http://maker.ifttt.com/trigger/motor_switch_error/with/key/xxxxxxxxxxxxx" // IFTTT webhook notify url
#define CALLBACK_URL "http://nas2.lan:8989/rebootprocess"
#define TURN_ON_URL "http://nas2.lan:8989/turnon"
#define TURN_OFF_URL "http://nas2.lan:8989/turnoff"
#endif

/* Configurable variables */
#define OTA_HOSTNAME "MotorSwitch"
const int BLINK_INTERVAL = 500; // in milliseconds
const int CALLBACK_INTERVAL = 30 * 60 * 1000; // 30 minutes
const int DST = 0;
const int LED_PIN = 1;
const int TIMEZONE = -5;
const int BUTTON_PIN = 0;
const int SERVER_PORT = 80; // Port number for web server

/* Do not change these unless you know what you are doing */
String logMsg;
String logTime;
String motorState = "";
uint8_t blinkState = 1;
unsigned long lastBlinkMillis = 0;
unsigned long lastCallbackMillis = 0;

HTTPClient http;
ESP8266WebServer server(SERVER_PORT);


void setup() {
  Serial.begin(115200);
  TelnetStream.begin();
  log("I/system: startup");
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setupWifi();
  setupTime();
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.begin();

  server.on("/", []() {
    server.send(200, "text/html", \
      "<a href=\"/log\">/log</a><br>"\
      "<a href=\"/reboot\">/reboot</a><br>"\
      "<a href=\"/signalon\">/signalon</a><br>"\
      "<a href=\"/signaloff\">/signaloff</a><br>"\
      "<br><p><small>"\
      "Powered by: <a href=\"https://github.com/ameer1234567890/MotorSwitch\">MotorSwitch</a> | "\
      "Chip ID: " + String(ESP.getChipId()) + " | " + \
      "Compiled at: " + __DATE__ + " " + __TIME__ + \
      "</small></p>"\
    );
    log("I/server: served / to " + server.client().remoteIP().toString());
  });
  server.on("/log", []() {
    log("I/server: served /log to " + server.client().remoteIP().toString());
    server.send(200, "text/plain", logMsg);
  });
  server.on("/signalon", []() {
    server.send(200, "text/plain", "Signalled on\n");
    digitalWrite(LED_PIN, LOW);
    motorState = "on";
    log("I/system: received signal on from " + server.client().remoteIP().toString());
  });
  server.on("/signaloff", []() {
    server.send(200, "text/plain", "Signalled off\n");
    digitalWrite(LED_PIN, HIGH);
    motorState = "off";
    log("I/system: received signal off from " + server.client().remoteIP().toString());
  });
  server.on("/reboot", []() {
    server.send(200, "text/plain", "Rebooting clock");
    log("I/system: rebooting upon request from " + server.client().remoteIP().toString());
    delay(1000);
    ESP.restart();
  });
  server.begin();
  log("I/system: initiating startup callback");
  callback();
}


void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  switch (TelnetStream.read()) {
    case 'R':
      TelnetStream.println("Rebooting device");
      TelnetStream.stop();
      delay(100);
      ESP.restart();
      break;
    case 'C':
      TelnetStream.println("Closing telnet connection");
      TelnetStream.flush();
      TelnetStream.stop();
      break;
  }

  if (motorState == "" && millis() - lastBlinkMillis > BLINK_INTERVAL) {
    lastBlinkMillis = millis();
    digitalWrite(LED_PIN, blinkState);
    if (blinkState == 1) {
      blinkState = 0;
    } else {
      blinkState = 1;
    }
  }

  if (millis() - lastCallbackMillis > CALLBACK_INTERVAL) {
    lastCallbackMillis = millis();
    motorState = "";
    log("I/system: initiating periodic callback");
    callback();
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    log("I/system: button pressed, toggling state");
    toggleState();
    debounce();
  }
}


void log(String msg) {
  time_t now = time(0);
  logTime = ctime(&now);
  logTime.trim();
  TelnetStream.println("[" + logTime + "] " + msg);
  logMsg = logMsg + "[" + logTime + "] ";
  logMsg = logMsg + msg + "\n";
  Serial.println(msg);
}


void setupWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    delay(50);
  }
  Serial.println("");
  log("I/wifi  : WiFi connected. IP address: " + WiFi.localIP().toString());
  delay(700);
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
}

void setupTime() {
  configTime(TIMEZONE * 3600, DST, "pool.ntp.org", "time.nist.gov", "time.google.com");
  log("I/time  : waiting for time");
  while (!time(nullptr)) {
    delay(100);
  }
  delay(100);
  time_t now = time(0);
  logTime = ctime(&now);
  logTime.trim();
  log("I/time  : time obtained via NTP => " + logTime);
}


void callback() {
  int httpCode;
  for (int i = 1; i < 11; i++) {
    log("I/callbk: callback try " + String(i));
    WiFiClient wClient;
    http.begin(wClient, CALLBACK_URL);
    httpCode = http.GET();
    http.end();
    if (httpCode == HTTP_CODE_OK) {
      log("I/callbk: callback done");
      break;
    }
    delay(100);
  }
  if (httpCode != HTTP_CODE_OK) {
    log("E/callbk: several callbacks failed => " + String(httpCode));
    error("several callbacks failed => " + String(httpCode));
  }
}


void toggleState() {
  WiFiClient wClient;
  if (motorState == "on") {
    http.begin(wClient, TURN_OFF_URL);
  } else {
    http.begin(wClient, TURN_ON_URL);
  }
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    log("I/button: request succeeded");
  } else {
    log("E/button: request failed. HTTP status code => " + String(httpCode));
  }
  http.end();
}


void error(String message) {
  message.replace(" ", "+");
  WiFiClient wClient;
  http.begin(wClient, String(IFTTT_ERROR_URL) + "?value1=" + message);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    log("I/notify : error notified via IFTTT");
  } else {
    log("E/notify: notification via IFTTT failed. HTTP status code => " + String(httpCode));
  }
  http.end();
}


void debounce() {
  // de-bouce by re-setting the pin
  pinMode(BUTTON_PIN, OUTPUT);
  digitalWrite(BUTTON_PIN, HIGH);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  delay(1000);
}
