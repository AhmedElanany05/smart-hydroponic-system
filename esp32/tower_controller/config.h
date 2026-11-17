// Tower controller - settings. Change values here, not in the main program.
#pragma once

// ---------------------------------------------------------------- pins (PCB v1.1, ESP32 NodeMCU-32S)
#define PIN_TOWER_PUMP   25   // Q2 -> J2 tower pump
#define PIN_NUT_A        26   // Q3 -> J3 nutrient pump A
#define PIN_NUT_B        27   // Q4 -> J4 nutrient pump B
#define PIN_JETSON       33   // Q6/Q5 -> J5 Jetson 12 V (HIGH = on); wire from IO33 to R16, IO13's track is shorted to GND
#define PIN_FLOAT_LOW    32   // J17 float switch (closed = tank low; D4 also blocks the tower pump in hardware)
#define PIN_SOIL         35   // soil moisture, EXP header pin "35" (analog, input only)
#define PIN_DHT          19   // J14 DHT11
#define PIN_TRIG          5   // J16 HC-SR04 trigger
#define PIN_ECHO         18   // J16 HC-SR04 echo (through the 1k/2k divider)
#define PIN_SDA          21
#define PIN_SCL          22

// ADS1115 channels
#define ADS_PH            0   // pH module through the 10k/20k divider
#define ADS_TDS           1   // TDS module
#define ADS_VBAT          2   // battery through the 68k/18k divider

// ---------------------------------------------------------------- which sensors are connected
#define HAS_LEVEL_SENSOR  false   // HC-SR04 not used: no room above the water in the bought reservoir
#define HAS_FLOAT_LOW     true    // float switch on J17 (closed = tank low)
#define HAS_SOIL_SENSOR   true

// ---------------------------------------------------------------- calibration
#define PH_DIVIDER        1.5     // (10k + 20k) / 20k
#define PH_V7             2.50    // module voltage at pH 7   <- calibrate with buffer pH 7 (6.86)
#define PH_SLOPE          0.18    // volts per pH unit        <- calibrate with buffer pH 4
#define TDS_CAL_FACTOR    1.0     // multiply the TDS result  <- calibrate with a TDS pen
#define WATER_TEMP_C      25.0    // used for TDS until a DS18B20 is added
#define VBAT_FACTOR       4.78    // (68k + 18k) / 18k
#define SOIL_RAW_DRY      3000    // raw reading in dry air   <- measure your sensor
#define SOIL_RAW_WET      1300    // raw reading in water     <- measure your sensor

// ---------------------------------------------------------------- tank (HC-SR04 mounted under the lid, looking down)
#define SENSOR_TO_BOTTOM_MM  300  // distance from the sensor to the tank floor
#define TANK_MIN_LEVEL_MM     60  // below this water height the tower pump is blocked (dry-run protection)
#define LEVEL_LOST_S          60  // no valid level reading for this long -> pump blocked

// ---------------------------------------------------------------- irrigation schedule (tower pump)
#define DAY_START_H        6      // daytime from 06:00
#define DAY_END_H         18      // ... to 18:00
#define DAY_ON_MIN        15      // daytime: first 15 minutes of every hour
#define NIGHT_PERIOD_H     2      // night: every 2 hours ...
#define NIGHT_ON_MIN       5      // ... for 5 minutes

// ---------------------------------------------------------------- nutrient dosing (A then B, never together)
#define DOSING_ENABLED    false   // keep false until TDS is calibrated and the pumps are measured
#define DOSE_HOUR         10      // check TDS once a day at 10:00
#define TDS_TARGET        900     // ppm
#define TDS_TOLERANCE     100     // dose only if TDS < target - tolerance
#define DOSE_ML           5.0     // ml of A (and the same of B) per dose
#define PUMP_ML_PER_S     1.0     // measured flow of the dosing pumps  <- measure with a syringe/cup
#define MIX_MIN           10      // tower pump runs this long after each nutrient to mix the tank
#define MANUAL_DOSE_MAX_S 30      // safety cap for manual nutrient pump commands
#define MANUAL_PUMP_MAX_S 600     // safety cap for manual tower pump commands

// ---------------------------------------------------------------- Jetson plant checks
#define JETSON_AUTO_ENABLED  false    // keep false until the Jetson's plant-check program is installed
                                      // (without it the Jetson never confirms shutdown and gets its power cut)
static const int JETSON_CHECK_HOURS[] = {9, 10, 12, 13, 15, 16};   // switched on at hh:00 (6 checks a day)
#define JETSON_MAX_ON_MIN        10   // ask it to shut down after this long
#define JETSON_SHUTDOWN_WAIT_S   30   // after "done", wait this long before cutting the power
#define JETSON_FORCE_OFF_S      120   // shutdown asked but not confirmed -> cut the power anyway
#define JETSON_BOOT_GRACE_S     240   // time allowed to boot before the heartbeat is watched
#define JETSON_ALIVE_TIMEOUT_S  180   // no heartbeat for this long -> power off (watchdog)

// ---------------------------------------------------------------- battery protection (12 V lead-acid, 50 % = about 12.2 V)
#define VBAT_PRESENT_MIN   6.0    // below this no battery is measured (USB bench test) -> protection off
#define VBAT_NO_JETSON    12.1    // first load to be cut
#define VBAT_NO_DOSING    11.9
#define VBAT_NO_PUMP      11.7    // last load to be cut
#define VBAT_HYST          0.3    // must rise this much above the limit to switch back on

// ---------------------------------------------------------------- network
#define HOSTNAME          "tower-esp32"          // http://tower-esp32.local
#define HOME_WIFI_ENABLED false                  // true once WIFI_SSID / WIFI_PASS in secrets.h are filled in (2.4 GHz only)
#define AP_SSID           "Tower"                // own Wi-Fi, always on: page at http://192.168.4.1
#define AP_PASS           "towertest"            // at least 8 characters
#define TZ_EGYPT          "EET-2EEST,M4.5.5/0,M10.5.4/24"
#define SENSOR_PERIOD_MS  5000
#define REPORT_PERIOD_MS  10000
