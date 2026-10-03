# finger-bot

A DIY smart button pusher (like SwitchBot) built with a **XIAO ESP32-C3**, a **SG90 micro servo**, **2× AA batteries**, and controlled via **WiFi + MQTT**. Battery life: approximately **6–8 years** at 7 presses/day.

---

## Table of Contents

- [How it works](#how-it-works)
- [Hardware](#hardware)
- [Wiring](#wiring)
- [Firmware](#firmware)
  - [Option A: ESPHome (recommended)](#option-a-esphome-recommended)
  - [Option B: Arduino / PlatformIO](#option-b-arduino--platformio)
- [MQTT Topics](#mqtt-topics)
- [Home Assistant Integration](#home-assistant-integration)
- [3D Print & Mechanical Assembly](#3d-print--mechanical-assembly)
- [Mounting](#mounting)
- [Power Consumption & Battery Life](#power-consumption--battery-life)
- [Troubleshooting](#troubleshooting)

---

## How it works

The ESP32-C3 sleeps in deep sleep (~10 µA). When an MQTT message arrives on the command topic, it wakes, rotates the SG90 servo arm to press a physical button, retracts, and goes back to sleep. A 3D-printed arm and housing clamp or stick onto any device.

```
MQTT broker  ──►  ESP32-C3  ──►  SG90 Servo  ──►  Physical button
                  (WiFi)          (presses)
```

---

## Hardware

| Component | Part | Price (approx.) |
|---|---|---|
| Microcontroller | Seeed XIAO ESP32-C3 | ~5 € |
| Servo | SG90 Micro Servo (9g) | ~2 € |
| Battery holder | 2× AA with JST/wire leads | ~1 € |
| Boost regulator | Pololu 3.3V Step-Up U1V11F3 | ~4 € |
| Perfboard | 5×7 cm | ~0.50 € |
| Wire | Jumper / hookup wire | ~1 € |
| Adhesive | 3M VHB double-sided tape | ~3 € |
| Enclosure | PLA 3D print | ~1 € filament |
| **Total** | | **~17 €** |
| Batteries | 2× AA alkaline | ~1 € |

> **Note:** The Pololu U1V11F3 boosts 2–5V input to a stable 3.3V. Two AA cells deliver 3.0V fresh (down to ~2.0V depleted) — this regulator handles the full discharge curve efficiently.

---

## Wiring

```
2× AA Batteries
    (+) ──► Pololu IN+
    (−) ──► Pololu GND ──► XIAO GND ──► Servo GND (brown)

Pololu OUT (3.3V) ──► XIAO 3V3 pin
                 └──► Servo VCC (red)

XIAO D5 (GPIO5) ──► Servo Signal (orange/yellow)
```

### Schematic (ASCII)

```
[AA+ ]──────────────────[Pololu IN+]
[AA− ]──┬───────────────[Pololu GND]
        │
        └──[XIAO GND]──────────────[Servo GND (brown)]

[Pololu 3.3V OUT]──[XIAO 3V3]
[Pololu 3.3V OUT]──────────────────[Servo VCC (red)]

[XIAO D5 / GPIO5]──────────────────[Servo Signal (orange)]
```

### Pin Reference (XIAO ESP32-C3)

| XIAO Pin | Label | Connected to |
|---|---|---|
| 3V3 | 3.3V out | Pololu OUT, Servo VCC |
| GND | Ground | Pololu GND, Servo GND |
| D5 | GPIO5 | Servo Signal wire |
| BAT+ / BAT− | (not used) | — (LiPo pads, leave open) |

> The XIAO ESP32-C3 **3V3 pin** can source up to 700 mA — sufficient for the SG90 (peak ~200 mA).

---

## Firmware

### Option A: ESPHome (recommended)

ESPHome handles WiFi, MQTT, OTA, and deep sleep with minimal code. No C++ required.

#### 1. Install ESPHome

```bash
pip install esphome
```

#### 2. Configure

Copy `esphome/finger-bot.yaml` and edit the substitutions at the top:

```yaml
# esphome/finger-bot.yaml
substitutions:
  device_name: finger-bot
  wifi_ssid: "YOUR_WIFI_SSID"
  wifi_password: "YOUR_WIFI_PASSWORD"
  mqtt_broker: "192.168.1.100"       # IP of your MQTT broker
  mqtt_port: "1883"
  mqtt_username: ""                  # leave empty if no auth
  mqtt_password: ""
  servo_pin: GPIO5
  press_duration_ms: "500"           # ms servo holds pressed position
  press_angle: "90"                  # degrees to press (0–180)
  idle_angle: "0"                    # rest position
```

Full config: see [`esphome/finger-bot.yaml`](esphome/finger-bot.yaml)

#### 3. Flash (first time, USB)

Connect the XIAO ESP32-C3 via USB-C. Hold the **BOOT** button, press **RESET**, release BOOT.

```bash
esphome run esphome/finger-bot.yaml
```

#### 4. Flash (OTA, after first flash)

```bash
esphome run esphome/finger-bot.yaml --device finger-bot.local
```

---

### Option B: Arduino / PlatformIO

For full control, use the PlatformIO project in `firmware/`.

#### 1. Install PlatformIO

```bash
pip install platformio
```

Or install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) for VS Code.

#### 2. Configure

Edit `firmware/src/config.h`:

```cpp
// WiFi
#define WIFI_SSID        "YOUR_WIFI_SSID"
#define WIFI_PASSWORD    "YOUR_WIFI_PASSWORD"

// MQTT
#define MQTT_BROKER      "192.168.1.100"
#define MQTT_PORT        1883
#define MQTT_USER        ""           // leave empty if no auth
#define MQTT_PASSWORD    ""
#define MQTT_CLIENT_ID   "finger-bot-01"

// Topics
#define TOPIC_COMMAND    "finger-bot/01/command"
#define TOPIC_STATE      "finger-bot/01/state"
#define TOPIC_BATTERY    "finger-bot/01/battery"

// Servo
#define SERVO_PIN        5            // GPIO5 = D5 on XIAO
#define SERVO_IDLE       0            // degrees, rest position
#define SERVO_PRESS      90           // degrees, press position
#define PRESS_DURATION   500          // ms to hold pressed

// Deep sleep
#define SLEEP_TIMEOUT_MS 10000        // sleep after 10s of inactivity
```

#### 3. Flash

```bash
cd firmware
pio run --target upload
```

Monitor serial output:

```bash
pio device monitor --baud 115200
```

---

## MQTT Topics

| Topic | Direction | Payload | Description |
|---|---|---|---|
| `finger-bot/01/command` | → device | `press` | Triggers one press |
| `finger-bot/01/command` | → device | `press:3` | Triggers 3 presses |
| `finger-bot/01/state` | ← device | `idle` / `pressing` | Current state |
| `finger-bot/01/battery` | ← device | `87` | Battery level in % |

### Test via CLI

```bash
# Trigger a single press
mosquitto_pub -h 192.168.1.100 -t "finger-bot/01/command" -m "press"

# Trigger 3 presses
mosquitto_pub -h 192.168.1.100 -t "finger-bot/01/command" -m "press:3"

# Watch state
mosquitto_sub -h 192.168.1.100 -t "finger-bot/01/#" -v
```

---

## Home Assistant Integration

### Auto-discovery (ESPHome)

ESPHome publishes MQTT discovery messages automatically. After flashing, the device appears in **Settings → Devices & Services → ESPHome**.

### Manual MQTT (Arduino firmware)

Add to `configuration.yaml`:

```yaml
mqtt:
  button:
    - name: "Finger Bot 01"
      unique_id: finger_bot_01
      command_topic: "finger-bot/01/command"
      payload_press: "press"
      retain: false
      qos: 1

  sensor:
    - name: "Finger Bot 01 Battery"
      unique_id: finger_bot_01_battery
      state_topic: "finger-bot/01/battery"
      unit_of_measurement: "%"
      device_class: battery
```

### Example Automation

```yaml
automation:
  - alias: "Press coffee machine button at 7:00"
    trigger:
      - platform: time
        at: "07:00:00"
    action:
      - service: mqtt.publish
        data:
          topic: "finger-bot/01/command"
          payload: "press"
```

---

## 3D Print & Mechanical Assembly

Files are in the `mechanical/` folder (STL + FreeCAD source).

| File | Description |
|---|---|
| `mechanical/housing.stl` | Main enclosure (servo + battery holder) |
| `mechanical/arm.stl` | Servo arm extension (presses the button) |
| `mechanical/mount_clip.stl` | Optional clip for edge mounting |

### Print Settings

| Parameter | Value |
|---|---|
| Material | PLA (PETG for humid environments) |
| Layer height | 0.2 mm |
| Infill | 20% |
| Supports | None needed |
| Estimated filament | ~15 g |

### Assembly

1. Press servo into housing slot (friction fit, add drop of CA glue if needed).
2. Attach servo arm extension to servo horn with M2×6 screw.
3. Route servo cable through cable channel.
4. Snap battery holder into rear compartment.
5. Solder/connect wiring per [Wiring](#wiring) section.
6. Close housing with 2× M2×10 screws.
7. Adjust arm length so it reaches the target button when servo moves to `SERVO_PRESS` angle.

---

## Mounting

**Option 1 – 3M VHB tape** (permanent, very strong):
- Clean surface with isopropyl alcohol.
- Apply VHB tape to housing base.
- Press firmly for 30 seconds, wait 24h before use.

**Option 2 – Hook-and-loop (Velcro):**
- Repositionable, no residue.
- Stick adhesive Velcro to housing base and device surface.

**Option 3 – Clip mount:**
- Print `mount_clip.stl` and slide over the edge of the device.
- No adhesive needed.

---

## Power Consumption & Battery Life

| State | Current draw |
|---|---|
| Deep sleep (WiFi off) | ~10 µA |
| WiFi connecting | ~100 mA (0.8 s) |
| Servo pressing | ~150 mA (0.5 s) |
| Idle / between presses | ~10 µA |

### Estimated battery life (2× AA alkaline, 2500 mAh/cell)

| Presses/day | Battery life |
|---|---|
| 5 | ~9 years |
| 7 | ~6–8 years |
| 10 | ~5–6 years |
| 20 | ~3–4 years |
| 50 | ~1–2 years |

> Calculation: usable energy = 2 × 2500 mAh × 1.5V × 85% boost efficiency = 6375 mWh.  
> At 7 presses/day: ~2.3 mWh/day → ~2770 days ≈ 7.6 years.

### Battery level estimation

The firmware reads the ADC voltage on the battery line (via a voltage divider) and publishes a percentage to `finger-bot/01/battery`. Replace batteries when level drops below 10%.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Device not connecting to WiFi | Wrong SSID/password | Check `config.h` or `finger-bot.yaml` |
| Servo doesn't move | Wrong GPIO / loose wire | Verify D5 → Signal wire |
| Servo jitters | Insufficient current | Check Pololu output voltage (should be 3.3V) |
| MQTT messages not received | Broker IP wrong | Ping broker, check firewall |
| Device not waking up | Deep sleep issue | Check MQTT broker is reachable, check QoS |
| Short battery life | WiFi not sleeping | Verify deep sleep config, check for reconnect loops |

---

## License

MIT License — see [LICENSE](LICENSE)

---

## Contributing

Pull requests welcome. Please open an issue first to discuss major changes.
