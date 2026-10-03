#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include "config.h"

// ─── Globals ─────────────────────────────────────────────────────────────────
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
Servo        fingerServo;

volatile bool  pressRequested = false;
volatile int   pressCount     = 1;
unsigned long  lastActivityMs = 0;

// ─── Forward declarations ────────────────────────────────────────────────────
void connectWiFi();
void connectMQTT();
void onMqttMessage(char* topic, byte* payload, unsigned int length);
void doPress(int count);
void publishState(const char* state);
void publishBattery();
void goToSleep();

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[finger-bot] Booting...");

  // Servo: attach but keep at idle
  fingerServo.attach(SERVO_PIN);
  fingerServo.write(SERVO_IDLE);
  delay(300);
  fingerServo.detach();   // detach to save power when idle

  connectWiFi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(256);

  connectMQTT();
  publishState("idle");
  publishBattery();

  lastActivityMs = millis();
  Serial.println("[finger-bot] Ready, waiting for commands...");
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();

  if (pressRequested) {
    pressRequested = false;
    doPress(pressCount);
    lastActivityMs = millis();
  }

  // Deep sleep after inactivity timeout
  if (millis() - lastActivityMs > ACTIVE_TIMEOUT_MS) {
    Serial.println("[finger-bot] Timeout reached, going to deep sleep.");
    goToSleep();
  }
}

// ─── WiFi ────────────────────────────────────────────────────────────────────
void connectWiFi() {
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) {
      Serial.println("\n[WiFi] Timeout — going to sleep.");
      goToSleep();
    }
    delay(250);
    Serial.print(".");
  }
  Serial.printf("\n[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
}

// ─── MQTT ────────────────────────────────────────────────────────────────────
void connectMQTT() {
  int attempts = 0;
  while (!mqtt.connected() && attempts < 5) {
    Serial.print("[MQTT] Connecting...");
    bool ok = (strlen(MQTT_USER) > 0)
      ? mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD,
                     TOPIC_STATE, 1, true, "offline")
      : mqtt.connect(MQTT_CLIENT_ID,
                     TOPIC_STATE, 1, true, "offline");

    if (ok) {
      Serial.println(" connected.");
      mqtt.subscribe(TOPIC_COMMAND, 1);
      Serial.printf("[MQTT] Subscribed to %s\n", TOPIC_COMMAND);
    } else {
      Serial.printf(" failed (rc=%d), retry in 2s\n", mqtt.state());
      delay(2000);
      attempts++;
    }
  }
  if (!mqtt.connected()) {
    Serial.println("[MQTT] Could not connect — going to sleep.");
    goToSleep();
  }
}

// ─── MQTT message handler ────────────────────────────────────────────────────
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String msg = String((char*)payload);
  Serial.printf("[MQTT] Message on %s: %s\n", topic, msg.c_str());
  lastActivityMs = millis();

  if (String(topic) == TOPIC_COMMAND) {
    if (msg == "press") {
      pressCount     = 1;
      pressRequested = true;
    } else if (msg.startsWith("press:")) {
      int n = msg.substring(6).toInt();
      if (n < 1)           n = 1;
      if (n > MAX_PRESSES) n = MAX_PRESSES;
      pressCount     = n;
      pressRequested = true;
    }
  }
}

// ─── Press sequence ──────────────────────────────────────────────────────────
void doPress(int count) {
  Serial.printf("[Servo] Pressing %d time(s)\n", count);
  publishState("pressing");

  fingerServo.attach(SERVO_PIN);

  for (int i = 0; i < count; i++) {
    fingerServo.write(SERVO_PRESS);
    delay(PRESS_DURATION);
    fingerServo.write(SERVO_IDLE);
    if (i < count - 1) delay(RETRACT_DELAY);
  }

  delay(200);
  fingerServo.detach();   // detach saves ~5 mA when idle

  publishState("idle");
  Serial.println("[Servo] Done.");
}

// ─── State + battery publishing ──────────────────────────────────────────────
void publishState(const char* state) {
  mqtt.publish(TOPIC_STATE, state, true);
}

void publishBattery() {
  // Read ADC (12-bit, 0–4095 = 0–3.3V with 11dB attenuation)
  int raw   = analogRead(BAT_ADC_PIN);
  float vAdc = raw / 4095.0f * 3.3f;
  float vBat = vAdc * BAT_DIVIDER;

  float pct = (vBat - BAT_EMPTY_V) / (BAT_FULL_V - BAT_EMPTY_V) * 100.0f;
  if (pct > 100.0f) pct = 100.0f;
  if (pct <   0.0f) pct =   0.0f;

  char buf[8];
  snprintf(buf, sizeof(buf), "%d", (int)pct);
  mqtt.publish(TOPIC_BATTERY, buf, true);
  Serial.printf("[Battery] %.2fV → %s%%\n", vBat, buf);
}

// ─── Deep sleep ──────────────────────────────────────────────────────────────
void goToSleep() {
  mqtt.publish(TOPIC_STATE, "offline", true);
  mqtt.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  fingerServo.detach();

  // ESP32-C3: wake on external signal or timer
  // Here: timer wake every 60s to check for pending commands
  // For truly push-driven use, connect a GPIO to a button and use ext0 wake.
  esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL); // 60s
  Serial.println("[Sleep] Entering deep sleep. Wake in 60s.");
  Serial.flush();
  esp_deep_sleep_start();
}
