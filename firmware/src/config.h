#pragma once

// ─── WiFi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID        "YOUR_WIFI_SSID"
#define WIFI_PASSWORD    "YOUR_WIFI_PASSWORD"
#define WIFI_TIMEOUT_MS  10000

// ─── MQTT ────────────────────────────────────────────────────────────────────
#define MQTT_BROKER      "192.168.1.100"
#define MQTT_PORT        1883
#define MQTT_USER        ""           // leave empty if broker has no auth
#define MQTT_PASSWORD    ""
#define MQTT_CLIENT_ID   "finger-bot-01"

#define TOPIC_COMMAND    "finger-bot/01/command"
#define TOPIC_STATE      "finger-bot/01/state"
#define TOPIC_BATTERY    "finger-bot/01/battery"

// ─── Servo ───────────────────────────────────────────────────────────────────
#define SERVO_PIN        5            // GPIO5 = D5 on XIAO ESP32-C3
#define SERVO_IDLE       0            // degrees — rest position
#define SERVO_PRESS      90           // degrees — press position
#define PRESS_DURATION   500          // ms to hold pressed position
#define RETRACT_DELAY    300          // ms between multiple presses
#define MAX_PRESSES      10           // max presses per command

// ─── Battery ADC ─────────────────────────────────────────────────────────────
#define BAT_ADC_PIN      2            // GPIO2 = A0 on XIAO ESP32-C3
// Voltage divider: 100kΩ + 100kΩ → multiply ADC reading by 2
#define BAT_DIVIDER      2.0f
#define BAT_FULL_V       3.0f         // 2× AA fresh = 3.0V
#define BAT_EMPTY_V      2.0f         // 2× AA depleted = 2.0V

// ─── Deep sleep ──────────────────────────────────────────────────────────────
#define ACTIVE_TIMEOUT_MS  15000      // go to sleep after 15s of inactivity
