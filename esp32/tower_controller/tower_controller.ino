// Tower controller - ESP32 (NodeMCU-32S) low-level controller of the hydroponic strawberry tower.
//
// Jobs: read the sensors, run the tower pump schedule, dose nutrients A and B, protect the pump
// from running dry, protect the battery (load shedding) and switch the Jetson on for plant checks.
// Settings are in config.h, the Wi-Fi login in secrets.h.
//
// Libraries: "Adafruit ADS1X15", "DHT sensor library" (Adafruit). Board: ESP32 Dev Module.
//
// Web interface (same Wi-Fi):  http://tower-esp32.local/        status page
//                  (own Wi-Fi "Tower"): http://192.168.4.1/
//                              http://tower-esp32.local/status  status as JSON
// Commands (need ?key=API_KEY):
//   /cmd?key=..&pump=tower&sec=60     run the tower pump for 60 s   (pump=a / b for nutrients, sec=0 stops)
//   /cmd?key=..&dose=1                run one dosing cycle now (A, mix, B, mix)
//   /cmd?key=..&jetson=on|off         switch the Jetson for a check / ask it to shut down
//   /cmd?key=..&maintenance=1|0       keep the Jetson on (no time limit, no watchdog)
// Jetson side (need ?key=API_KEY):
//   /jetson/alive    heartbeat, answer says if the Jetson should shut down
//   /jetson/result?label=..&conf=..   disease check result
//   /jetson/done     "I am shutting down now"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <time.h>
#include <Adafruit_ADS1X15.h>
#include <DHT.h>
#include "config.h"
#include "secrets.h"
#include "page.h"

Adafruit_ADS1115 ads;
DHT dht(PIN_DHT, DHT11);
WebServer server(80);

// ---------------------------------------------------------------- state
struct Readings {
  float phV = NAN, ph = NAN;
  float tdsV = NAN, tds = NAN;
  float vbat = NAN;
  float airT = NAN, airH = NAN;
  float levelMm = NAN;
  bool levelValid = false;
  int soilRaw = -1;
  float soilPct = NAN;
  bool floatLow = false;          // true = tank low (only if HAS_FLOAT_LOW)
} rd;

bool adsOk = false;
bool benchMode = true;            // no battery measured -> battery protection off
bool shedJetson = false, shedDosing = false, shedPump = false;
unsigned long lastLevelValidMs = 0;

bool towerOn = false, nutAOn = false, nutBOn = false;
unsigned long manualTowerUntil = 0, manualAUntil = 0, manualBUntil = 0;

enum DoseState { DOSE_IDLE, DOSE_A, MIX_A, DOSE_B, MIX_B };
DoseState doseState = DOSE_IDLE;
unsigned long doseStepEnd = 0;
int lastDoseYday = -1;
String lastDoseInfo = "none yet";

enum JetState { JET_OFF, JET_ON, JET_SHUTTING };
JetState jetState = JET_OFF;
unsigned long jetOnSince = 0, jetLastAlive = 0, jetShutdownAt = 0, jetRequestAt = 0;
bool jetShutdownRequested = false, maintenance = false;
bool jetHeard = false;            // a message came from the Jetson since it was switched on
int lastCheckSlot = -1;
String lastResult = "none yet", lastResultTime = "-";

String events[8];                 // last events for the status page
int eventPos = 0;
bool mdnsStarted = false;
unsigned long lastSensorMs = 0, lastReportMs = 0, lastWifiTry = 0;

// ---------------------------------------------------------------- helpers
bool getTime(struct tm &t) {
  if (!getLocalTime(&t, 5)) return false;
  return t.tm_year > 120;         // year > 2020 means NTP time is set
}

String timeStr() {
  struct tm t;
  if (!getTime(t)) return "t+" + String(millis() / 1000) + "s";
  char b[20];
  strftime(b, sizeof(b), "%Y-%m-%d %H:%M", &t);
  return String(b);
}

void logEvent(const String &msg) {
  String line = timeStr() + "  " + msg;
  Serial.println(line);
  events[eventPos] = line;
  eventPos = (eventPos + 1) % 8;
}

bool before(unsigned long until) {          // millis() is still before "until" (overflow safe)
  return (long)(until - millis()) > 0;
}

String num(float v, int d) { return isnan(v) ? String("null") : String(v, d); }

// ---------------------------------------------------------------- sensors
float readVolts(int ch) {
  long sum = 0;
  for (int i = 0; i < 8; i++) sum += ads.readADC_SingleEnded(ch);
  return ads.computeVolts((int16_t)(sum / 8));
}

float readLevelMm() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(3);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, 30000);   // max 30 ms (about 5 m)
  if (us == 0) return NAN;
  float dist = us * 0.343 / 2.0;                       // mm
  if (dist < 20 || dist > 4000) return NAN;
  return SENSOR_TO_BOTTOM_MM - dist;                   // water height above the floor
}

void readSensors() {
  if (adsOk) {
    rd.phV = readVolts(ADS_PH) * PH_DIVIDER;
    rd.ph = 7.0 + (PH_V7 - rd.phV) / PH_SLOPE;

    rd.tdsV = readVolts(ADS_TDS);
    float v25 = rd.tdsV / (1.0 + 0.02 * (WATER_TEMP_C - 25.0));
    float tds = (133.42 * v25 * v25 * v25 - 255.86 * v25 * v25 + 857.39 * v25) * 0.5 * TDS_CAL_FACTOR;
    rd.tds = tds < 0 ? 0 : tds;

    rd.vbat = readVolts(ADS_VBAT) * VBAT_FACTOR;
  }
  benchMode = !adsOk || isnan(rd.vbat) || rd.vbat < VBAT_PRESENT_MIN;

  float t = dht.readTemperature(), h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) { rd.airT = t; rd.airH = h; }

  if (HAS_LEVEL_SENSOR) {
    float lv = readLevelMm();
    if (!isnan(lv)) { rd.levelMm = lv; rd.levelValid = true; lastLevelValidMs = millis(); }
    else if (millis() - lastLevelValidMs > LEVEL_LOST_S * 1000UL) {
      if (rd.levelValid) logEvent("tank level sensor: no valid reading - tower pump blocked");
      rd.levelValid = false;
    }
  }

  if (HAS_SOIL_SENSOR) {
    long s = 0;
    for (int i = 0; i < 8; i++) s += analogRead(PIN_SOIL);
    rd.soilRaw = s / 8;
    float pct = 100.0 * (SOIL_RAW_DRY - rd.soilRaw) / (float)(SOIL_RAW_DRY - SOIL_RAW_WET);
    rd.soilPct = constrain(pct, 0.0, 100.0);
  }

  if (HAS_FLOAT_LOW) rd.floatLow = (digitalRead(PIN_FLOAT_LOW) == LOW);   // switch closed = tank low
}

// ---------------------------------------------------------------- protection
void updateShedding() {
  if (benchMode) { shedJetson = shedDosing = shedPump = false; return; }
  float v = rd.vbat;
  struct Lim { bool *flag; float th; const char *name; } lims[] = {
    {&shedJetson, VBAT_NO_JETSON, "Jetson"}, {&shedDosing, VBAT_NO_DOSING, "dosing"}, {&shedPump, VBAT_NO_PUMP, "tower pump"}};
  for (auto &l : lims) {
    if (!*l.flag && v < l.th) { *l.flag = true; logEvent(String("battery ") + String(v, 2) + " V: " + l.name + " switched off"); }
    else if (*l.flag && v > l.th + VBAT_HYST) { *l.flag = false; logEvent(String("battery ") + String(v, 2) + " V: " + l.name + " allowed again"); }
  }
}

bool tankOk() {
  if (HAS_FLOAT_LOW && rd.floatLow) return false;
  if (!HAS_LEVEL_SENSOR) return true;
  return rd.levelValid && rd.levelMm >= TANK_MIN_LEVEL_MM;
}

// ---------------------------------------------------------------- tower pump schedule
bool scheduleWantsTower() {
  struct tm t;
  if (getTime(t)) {
    bool day = t.tm_hour >= DAY_START_H && t.tm_hour < DAY_END_H;
    if (day) return t.tm_min < DAY_ON_MIN;
    return (t.tm_hour % NIGHT_PERIOD_H == 0) && t.tm_min < NIGHT_ON_MIN;
  }
  return (millis() / 60000UL) % 60 < DAY_ON_MIN;   // no clock yet: daytime pattern on uptime
}

// ---------------------------------------------------------------- dosing (A -> mix -> B -> mix)
unsigned long doseMs() { return (unsigned long)(DOSE_ML / PUMP_ML_PER_S * 1000.0); }

void startDose(const String &why) {
  if (doseState != DOSE_IDLE) return;
  doseState = DOSE_A;
  doseStepEnd = millis() + doseMs();
  logEvent("dosing started (" + why + "): nutrient A " + String(doseMs() / 1000.0, 1) + " s");
}

void updateDosing() {
  struct tm t;
  bool tv = getTime(t);
  if (doseState != DOSE_IDLE && (shedDosing || !tankOk())) {
    doseState = DOSE_IDLE;
    lastDoseInfo = "aborted at " + timeStr() + " (battery or tank level)";
    logEvent("dosing aborted (battery or tank level)");
    return;
  }
  switch (doseState) {
    case DOSE_IDLE:
      if (DOSING_ENABLED && tv && t.tm_hour == DOSE_HOUR && t.tm_yday != lastDoseYday && !isnan(rd.tds)) {
        lastDoseYday = t.tm_yday;
        if (shedDosing || !tankOk()) lastDoseInfo = timeStr() + ": skipped (battery or tank level)";
        else if (rd.tds < TDS_TARGET - TDS_TOLERANCE) startDose("TDS " + String(rd.tds, 0) + " ppm");
        else lastDoseInfo = timeStr() + ": not needed (TDS " + String(rd.tds, 0) + " ppm)";
      }
      break;
    case DOSE_A:
      if (!before(doseStepEnd)) { doseState = MIX_A; doseStepEnd = millis() + MIX_MIN * 60000UL; }
      break;
    case MIX_A:
      if (!before(doseStepEnd)) { doseState = DOSE_B; doseStepEnd = millis() + doseMs(); logEvent("dosing: nutrient B"); }
      break;
    case DOSE_B:
      if (!before(doseStepEnd)) { doseState = MIX_B; doseStepEnd = millis() + MIX_MIN * 60000UL; }
      break;
    case MIX_B:
      if (!before(doseStepEnd)) {
        doseState = DOSE_IDLE;
        lastDoseInfo = timeStr() + ": dosed " + String(DOSE_ML, 1) + " ml A + B";
        logEvent("dosing finished");
      }
      break;
  }
}

// ---------------------------------------------------------------- Jetson power
void jetsonOn(const String &why) {
  jetState = JET_ON;
  jetOnSince = jetLastAlive = millis();
  jetShutdownRequested = false;
  jetHeard = false;
  logEvent("Jetson on (" + why + ")");
}

void jetsonRequestShutdown(const String &why) {
  if (jetState != JET_ON || jetShutdownRequested) return;
  jetShutdownRequested = true;
  jetRequestAt = millis();
  logEvent("Jetson asked to shut down (" + why + ")");
}

void updateJetson() {
  unsigned long now = millis();
  struct tm t;
  bool tv = getTime(t);
  switch (jetState) {
    case JET_OFF:
      if (shedJetson) break;
      if (maintenance) { jetsonOn("maintenance"); break; }
      if (JETSON_AUTO_ENABLED && tv && t.tm_min < 5) {
        for (int h : JETSON_CHECK_HOURS) {
          int slot = t.tm_yday * 24 + h;
          if (t.tm_hour == h && slot != lastCheckSlot) { lastCheckSlot = slot; jetsonOn("plant check"); }
        }
      }
      break;
    case JET_ON:
      if (shedJetson) jetsonRequestShutdown("low battery");
      else if (!maintenance && now - jetOnSince > JETSON_MAX_ON_MIN * 60000UL) jetsonRequestShutdown("time limit");
      if (jetShutdownRequested && now - jetRequestAt > JETSON_FORCE_OFF_S * 1000UL) {
        jetState = JET_OFF; logEvent("Jetson did not confirm shutdown - power cut");
      } else if (!maintenance && now - jetOnSince > JETSON_BOOT_GRACE_S * 1000UL &&
                 now - jetLastAlive > JETSON_ALIVE_TIMEOUT_S * 1000UL) {
        jetState = JET_OFF; logEvent("Jetson watchdog: no heartbeat - power cut");
      }
      break;
    case JET_SHUTTING:
      if (now - jetShutdownAt > JETSON_SHUTDOWN_WAIT_S * 1000UL) { jetState = JET_OFF; logEvent("Jetson power off"); }
      break;
  }
}

// ---------------------------------------------------------------- outputs
void setOut(int pin, bool &state, bool on, const char *name) {
  if (on != state) { state = on; logEvent(String(name) + (on ? " ON" : " OFF")); }
  digitalWrite(pin, on ? HIGH : LOW);
}

void updateOutputs() {
  bool mixing = (doseState == MIX_A || doseState == MIX_B);
  bool wantTower = scheduleWantsTower() || before(manualTowerUntil) || mixing;
  bool tower = wantTower && tankOk() && !shedPump;

  bool a = (doseState == DOSE_A) || before(manualAUntil);
  bool b = (doseState == DOSE_B) || before(manualBUntil);
  if (a && b) b = false;                     // A and B never together
  if (shedDosing || !tankOk()) { a = false; b = false; }

  setOut(PIN_TOWER_PUMP, towerOn, tower, "tower pump");
  setOut(PIN_NUT_A, nutAOn, a, "nutrient pump A");
  setOut(PIN_NUT_B, nutBOn, b, "nutrient pump B");
  digitalWrite(PIN_JETSON, jetState != JET_OFF ? HIGH : LOW);
}

// ---------------------------------------------------------------- web interface
const char *doseName() {
  switch (doseState) { case DOSE_A: return "dosing A"; case MIX_A: return "mixing after A";
    case DOSE_B: return "dosing B"; case MIX_B: return "mixing after B"; default: return "idle"; }
}
const char *jetName() {
  switch (jetState) { case JET_ON: return jetShutdownRequested ? "on, shutdown requested" : "on";
    case JET_SHUTTING: return "shutting down"; default: return "off"; }
}

String jsonEsc(const String &s) {
  String o = s;
  o.replace("\\", "\\\\");
  o.replace("\"", "\\\"");
  return o;
}

String statusJson() {
  String j = "{";
  j += "\"uptime_s\":" + String(millis() / 1000) + ",";
  j += "\"float_low\":" + String(rd.floatLow ? "true" : "false") + ",";
  j += "\"jetson_seen_s\":" + (jetHeard ? String((millis() - jetLastAlive) / 1000) : String("null")) + ",";
  j += "\"wifi_ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("")) + "\",";
  j += "\"events\":[";
  bool first = true;
  for (int i = 0; i < 8; i++) {
    String e = events[(eventPos + 7 - i) % 8];
    if (!e.length()) continue;
    j += (first ? "\"" : ",\"") + jsonEsc(e) + "\"";
    first = false;
  }
  j += "],";
  j += "\"time\":\"" + timeStr() + "\",";
  j += "\"ph\":" + num(rd.ph, 2) + ",\"ph_v\":" + num(rd.phV, 3) + ",";
  j += "\"tds_ppm\":" + num(rd.tds, 0) + ",\"tds_v\":" + num(rd.tdsV, 3) + ",";
  j += "\"air_temp_c\":" + num(rd.airT, 1) + ",\"air_humidity\":" + num(rd.airH, 0) + ",";
  j += "\"tank_level_mm\":" + String(rd.levelValid ? String(rd.levelMm, 0) : "null") + ",";
  j += "\"soil_pct\":" + num(rd.soilPct, 0) + ",\"soil_raw\":" + String(rd.soilRaw) + ",";
  j += "\"battery_v\":" + (benchMode ? String("null") : num(rd.vbat, 2)) + ",";
  j += "\"tank_ok\":" + String(tankOk() ? "true" : "false") + ",";
  j += "\"tower_pump\":" + String(towerOn ? "true" : "false") + ",";
  j += "\"nutrient_a\":" + String(nutAOn ? "true" : "false") + ",\"nutrient_b\":" + String(nutBOn ? "true" : "false") + ",";
  j += "\"dosing\":\"" + String(doseName()) + "\",\"last_dose\":\"" + lastDoseInfo + "\",";
  j += "\"jetson\":\"" + String(jetName()) + "\",\"maintenance\":" + String(maintenance ? "true" : "false") + ",";
  j += "\"last_result\":\"" + jsonEsc(lastResult) + "\",\"last_result_time\":\"" + lastResultTime + "\",";
  j += "\"shed\":{\"jetson\":" + String(shedJetson ? "true" : "false") + ",\"dosing\":" + String(shedDosing ? "true" : "false") +
       ",\"pump\":" + String(shedPump ? "true" : "false") + "}";
  j += "}";
  return j;
}

void handleRoot() {
  server.send_P(200, "text/html", PAGE_HTML);
}

bool keyOk() {
  if (server.arg("key") == API_KEY) return true;
  server.send(403, "application/json", "{\"error\":\"wrong or missing key\"}");
  return false;
}

String jetsonReply() {
  return String("{\"shutdown\":") + (jetShutdownRequested ? "true" : "false") +
         ",\"maintenance\":" + (maintenance ? "true" : "false") + "}";
}

void handleJetsonAlive() {
  if (!keyOk()) return;
  if (!jetHeard) logEvent("Jetson connected");
  jetLastAlive = millis();
  jetHeard = true;
  server.send(200, "application/json", jetsonReply());
}

void handleJetsonResult() {
  if (!keyOk()) return;
  jetLastAlive = millis();
  jetHeard = true;
  lastResult = server.arg("label") + (server.hasArg("conf") ? " " + server.arg("conf") : "");
  lastResultTime = timeStr();
  logEvent("plant check result: " + lastResult);
  server.send(200, "application/json", jetsonReply());
}

void handleJetsonDone() {
  if (!keyOk()) return;
  if (jetState == JET_ON) {
    jetState = JET_SHUTTING;
    jetShutdownAt = millis();
    logEvent("Jetson is shutting down, power off in " + String(JETSON_SHUTDOWN_WAIT_S) + " s");
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleCmd() {
  if (!keyOk()) return;
  String msg;
  if (server.hasArg("pump")) {
    String p = server.arg("pump");
    long sec = server.arg("sec").toInt();
    if (p == "tower") {
      sec = constrain(sec, 0, MANUAL_PUMP_MAX_S);
      manualTowerUntil = millis() + sec * 1000UL;
    } else if (p == "a" || p == "b") {
      sec = constrain(sec, 0, MANUAL_DOSE_MAX_S);
      if (p == "a") { manualAUntil = millis() + sec * 1000UL; manualBUntil = 0; }
      else          { manualBUntil = millis() + sec * 1000UL; manualAUntil = 0; }
    } else if (p == "all") {
      manualTowerUntil = manualAUntil = manualBUntil = 0;
      sec = 0;
    } else { server.send(400, "application/json", "{\"error\":\"pump must be tower, a, b or all\"}"); return; }
    msg = "manual " + p + " " + String(sec) + " s";
  } else if (server.hasArg("dose")) {
    startDose("manual");
    msg = "dosing requested";
  } else if (server.hasArg("jetson")) {
    String j = server.arg("jetson");
    if (j == "on" && jetState == JET_OFF) jetsonOn("manual");
    else if (j == "off") jetsonRequestShutdown("manual");
    msg = "jetson " + j;
  } else if (server.hasArg("maintenance")) {
    maintenance = server.arg("maintenance") == "1";
    msg = String("maintenance ") + (maintenance ? "on" : "off");
  } else { server.send(400, "application/json", "{\"error\":\"unknown command\"}"); return; }
  logEvent("command: " + msg);
  server.send(200, "application/json", "{\"ok\":true,\"done\":\"" + msg + "\"}");
}

// ---------------------------------------------------------------- Wi-Fi
// The ESP32 always has its own Wi-Fi (AP_SSID) and also joins the home Wi-Fi from secrets.h when it can.
bool homeConnected = false;

void handleWifi() {
  if (!HOME_WIFI_ENABLED) return;   // searching for a missing network makes the own Wi-Fi drop out
  if (WiFi.status() == WL_CONNECTED) {
    if (!homeConnected) {
      homeConnected = true;
      logEvent("home Wi-Fi connected: http://" + WiFi.localIP().toString() + "  (http://" HOSTNAME ".local)");
    }
    return;
  }
  if (homeConnected) { homeConnected = false; logEvent("home Wi-Fi lost, reconnecting"); }
  if (millis() - lastWifiTry > 30000UL) { lastWifiTry = millis(); WiFi.disconnect(); WiFi.begin(WIFI_SSID, WIFI_PASS); }
}

void printReport() {
  Serial.printf("[%s] pH %s | TDS %s ppm | air %s C %s %% | tank %s mm%s | soil %s %% | batt %s | pump %s | A %s B %s | dosing %s | Jetson %s\n",
                timeStr().c_str(), num(rd.ph, 2).c_str(), num(rd.tds, 0).c_str(), num(rd.airT, 1).c_str(), num(rd.airH, 0).c_str(),
                rd.levelValid ? String(rd.levelMm, 0).c_str() : "--", tankOk() ? "" : " (BLOCKED)", num(rd.soilPct, 0).c_str(),
                benchMode ? "USB" : (String(rd.vbat, 2) + " V").c_str(), towerOn ? "ON" : "off",
                nutAOn ? "ON" : "off", nutBOn ? "ON" : "off", doseName(), jetName());
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  // all outputs off first
  int outs[] = {PIN_TOWER_PUMP, PIN_NUT_A, PIN_NUT_B, PIN_JETSON};
  for (int p : outs) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  if (HAS_FLOAT_LOW) pinMode(PIN_FLOAT_LOW, INPUT);       // pull-up on the PCB
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_SOIL, ADC_11db);

  Serial.begin(115200);
  delay(300);
  Serial.println("\nTower controller starting");

  Wire.begin(PIN_SDA, PIN_SCL);
  adsOk = ads.begin(0x48);
  if (adsOk) ads.setGain(GAIN_ONE);                      // +/-4.096 V
  logEvent(adsOk ? "ADS1115 found" : "ADS1115 NOT found - pH, TDS and battery disabled");
  dht.begin();

  WiFi.mode(HOME_WIFI_ENABLED ? WIFI_AP_STA : WIFI_AP);
  WiFi.setHostname(HOSTNAME);
  WiFi.softAP(AP_SSID, AP_PASS, 6);   // fixed channel 6
  logEvent("own Wi-Fi \"" AP_SSID "\" on: http://192.168.4.1");
  if (HOME_WIFI_ENABLED) WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastWifiTry = millis();
  if (MDNS.begin(HOSTNAME)) { MDNS.addService("http", "tcp", 80); mdnsStarted = true; }
  configTzTime(TZ_EGYPT, "pool.ntp.org", "time.google.com");

  server.on("/", handleRoot);
  server.on("/status", []() { server.send(200, "application/json", statusJson()); });
  server.on("/cmd", handleCmd);
  server.on("/jetson/alive", handleJetsonAlive);
  server.on("/jetson/hello", handleJetsonAlive);
  server.on("/jetson/result", handleJetsonResult);
  server.on("/jetson/done", handleJetsonDone);
  server.begin();

  readSensors();
  lastSensorMs = millis();
}

void loop() {
  server.handleClient();
  handleWifi();

  if (millis() - lastSensorMs >= SENSOR_PERIOD_MS) {
    lastSensorMs = millis();
    readSensors();
    updateShedding();
  }
  updateDosing();
  updateJetson();
  updateOutputs();

  if (millis() - lastReportMs >= REPORT_PERIOD_MS) {
    lastReportMs = millis();
    printReport();
  }
  delay(5);
}
