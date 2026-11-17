<style>
h2:not(:first-of-type) { break-before: auto !important; }
h2 { margin-top: 16px; }
table { font-size: 9.3pt; break-inside: auto; }
</style>

# Tower Controller: System Wiring and Firmware

ESP32 NodeMCU-32S on the tower controller board (PCB v1.1). Sensors in this build: pH, TDS, HC-SR04 (tank level), soil moisture, DHT11, ADS1115. Loads: tower pump AD20P, nutrient pumps A and B, Jetson Orin Nano.

## 1. Power wiring

```
 SOLAR PANEL 50 W ──(solar cable)──► CHARGE CONTROLLER  ☀ + / −
                                     CHARGE CONTROLLER  🔋 + / − ──[10 A inline fuse]──┐
                                                                                       │
 BATTERY 12 V 18 Ah  + post ◄──────────────────────────────────────────────────────────┘
                     + post ──[10 A inline fuse]──► PCB J1 "+"
                     − post ──────────────────────► PCB J1 "−"   (and controller 🔋 −)

 PCB J6 "BUCK IN"  + / − ──► XL4015 IN + / −      (module screwed onto the board, M1)
 XL4015 OUT + / −  ──────► PCB J7 "5V IN" + / −   (set 5.0 V and about 1.5 A first)
```

| Connection | Wire | Note |
|---|---|---|
| Panel → controller | Solar cable 2.5 mm² | Connect the battery to the controller **first**, panel second |
| Battery → controller | 2.5 mm², 10 A inline fuse on + | Controller battery type: sealed lead-acid |
| Battery → PCB J1 | 2.5 mm², 10 A inline fuse on + | Fuse within 15 cm of the battery |
| PCB J6 / J7 ↔ XL4015 | 1 mm², short | Never connect J7 before the module is set to 5.0 V |
| Charge controller LOAD output | not used | |

## 2. Loads on the screw terminals

| Terminal | Connect | Wire colour | ESP32 pin |
|---|---|---|---|
| **J2 TOWER** | AD20P tower pump | red → **+**, black → **−** | GPIO25 |
| **J3 NUT A** | Nutrient pump A | + → **+**, − → **−** | GPIO26 |
| **J4 NUT B** | Nutrient pump B | + → **+**, − → **−** | GPIO27 |
| **J5 JETSON** | Jetson barrel plug (5.5 × 2.5 mm) | **+ → centre pin**, − → sleeve | GPIO13 |

The "−" side of each pump terminal is switched by the board. It is **not** ground, so do not connect it to any other ground.

## 3. Modules in the sockets

| Socket | Module | Orientation |
|---|---|---|
| J8 + J9 (2 × 19) | ESP32 NodeMCU-32S | Antenna towards the board edge, **3V3** pin at the "3V3" mark |
| J10 (1 × 10) | ADS1115 | Pin **VDD** at the "VDD" mark |

## 4. Sensors on the headers

| Sensor | Header | Sensor pin → header pin | Notes |
|---|---|---|---|
| **pH module** (PH-4502C) | J11 "PH" | V+ → **5V**, Po → **S**, G → **G** | Leave Do and To empty. Probe on the BNC socket |
| **TDS module** | J12 "TDS" | + → **5V**, A → **S**, − → **G** | |
| **DHT11** | J14 "DHT11" | + → **3V**, S/OUT → **D**, − → **G** | Outside the tank, in the shade |
| **HC-SR04** | J16 "HC-SR04" | VCC → **5V**, TRIG → **T**, ECHO → **E**, GND → **G** | Under the tank lid, looking straight down at the water |
| **Soil moisture** | J20 "EXP" | VCC → **3V**, GND → **G**, AOUT → **35** | Use the 3V pin, not 5V |
| DS18B20 / leak / floats | J13, J15, J17, J18 | — | Not installed in this build (headers stay empty) |

**Recommended addition:** a **float switch on J17 "FL LO"** (closed = tank low). It blocks the tower pump in hardware, even if the software fails. Then set `HAS_FLOAT_LOW true` in `config.h`.

## 5. Breadboard test wiring (before the PCB arrives)

The ESP32 is powered from the PC's USB cable. Pumps and the Jetson are **not** connected: an LED with a 330 Ω resistor on each output pin shows when it would switch on.

| From | To |
|---|---|
| ESP32 3V3 / GND | ADS1115 VDD / GND, ADS1115 **ADDR → GND** |
| ESP32 GPIO21 / GPIO22 | ADS1115 SDA / SCL |
| pH module V+ / G | ESP32 5V / GND |
| pH module Po | **10 kΩ** → ADS1115 A0, and A0 → **20 kΩ** → GND |
| TDS module + / − / A | ESP32 5V / GND / ADS1115 A1 |
| DHT11 + / − / S | ESP32 3V3 / GND / GPIO19 |
| HC-SR04 VCC / GND / TRIG | ESP32 5V / GND / GPIO5 |
| HC-SR04 ECHO | **1 kΩ** → GPIO18, and GPIO18 → **2 kΩ** → GND |
| Soil moisture VCC / GND / AOUT | ESP32 3V3 / GND / GPIO35 |
| Test LEDs (+ 330 Ω to GND) | GPIO25 (tower), GPIO26 (A), GPIO27 (B), GPIO13 (Jetson) |

Never connect a pump directly to an ESP32 pin.

## 6. Firmware: first use

Folder `esp32\tower_controller\` (open `tower_controller.ino` in Arduino IDE; `config.h` and `secrets.h` open as tabs).

1. **secrets.h:** enter your Wi-Fi name, password and a key of your own for commands.
2. **config.h:** check `SENSOR_TO_BOTTOM_MM` (distance from the HC-SR04 to the tank floor) and `TANK_MIN_LEVEL_MM`.
3. Libraries: **Adafruit ADS1X15** and **DHT sensor library** (already installed).
4. Board **ESP32 Dev Module**, port **COM3**, **Upload**. Serial Monitor at **115200**.
5. On the same Wi-Fi, open **http://tower-esp32.local** (or the IP address printed in the Serial Monitor).

| Setting in config.h | Default | Set it when |
|---|---|---|
| `PH_V7`, `PH_SLOPE` | 2.50 V, 0.18 V/pH | After calibrating with pH 7 and pH 4 buffers |
| `TDS_CAL_FACTOR` | 1.0 | After comparing with a TDS pen |
| `SOIL_RAW_DRY`, `SOIL_RAW_WET` | 3000, 1300 | After reading the sensor in air and in water |
| `PUMP_ML_PER_S` | 1.0 | After timing a nutrient pump into a measuring cup |
| `DOSING_ENABLED` | **false** | Only after TDS is calibrated and the pumps are measured |

## 7. What the firmware does

| Function | Behaviour |
|---|---|
| Tower pump | 06:00–18:00: first 15 min of every hour. Night: 5 min every 2 hours. Egypt time from the internet |
| Dry-run protection | Pump blocked when the water is below `TANK_MIN_LEVEL_MM`, or the level sensor gives no reading for 60 s |
| Nutrient dosing | Once a day at 10:00, if TDS < 800 ppm: A → 10 min mixing → B → 10 min mixing. A and B never together |
| Battery protection | Below 12.1 V: no Jetson; below 11.9 V: no dosing; below 11.7 V: no tower pump (back on 0.3 V higher) |
| Jetson | On at 09, 10, 12, 13, 15 and 16 h for a plant check, max 10 min. Shutdown handshake, then power off after 30 s. Watchdog cuts power if it stops responding |
| Web page | Live values, pump states, last plant check result and the last 8 events |

### Commands (from a browser on the same Wi-Fi)

| Command | Effect |
|---|---|
| `/cmd?key=KEY&pump=tower&sec=60` | Tower pump on for 60 s |
| `/cmd?key=KEY&pump=a&sec=5` | Nutrient pump A for 5 s (max 30 s) |
| `/cmd?key=KEY&pump=all&sec=0` | Stop all manual pump runs |
| `/cmd?key=KEY&dose=1` | One full dosing cycle now |
| `/cmd?key=KEY&jetson=on` / `off` | Start a plant check / ask the Jetson to shut down |
| `/cmd?key=KEY&maintenance=1` / `0` | Keep the Jetson on for work over SSH |
