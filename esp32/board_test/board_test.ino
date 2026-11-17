// Board test: reads every sensor on the PCB every 2 s.
// The ESP32 makes its own Wi-Fi network "Tower-Test" (password towertest). Join it with a phone and open
// http://192.168.4.1 to see the readings. They are also printed on the Serial Monitor (115200 baud).
// Pump buttons on the page run one pump at a time for PUMP_TEST_MS, then switch it off again.
// The J5 (Jetson) button stays on until ALL OFF, for testing J5 with the Jetson or a pump.
// Libraries: "Adafruit ADS1X15" and "DHT sensor library" (already installed for the sensor tests).

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_ADS1X15.h>
#include <DHT.h>

// pins on PCB v1.1 (same as tower_controller/config.h)
// IO13's track is shorted to GND on this board, so J5 is driven from IO33 through a wire to R16
const int PIN_TOWER_PUMP = 25, PIN_NUT_A = 26, PIN_NUT_B = 27, PIN_JETSON = 33;
const int PIN_SOIL = 35, PIN_DHT = 19, PIN_TRIG = 5, PIN_ECHO = 18;
const int PIN_FLOAT_LOW = 32;      // J17, 10k pull-up on the PCB: LOW = switch closed = tank low

const float PH_DIVIDER  = 1.5;    // (10k + 20k) / 20k
const float VBAT_FACTOR = 4.78;   // (68k + 18k) / 18k
const unsigned long PUMP_TEST_MS = 10000;

// J5 is the Jetson output; it is only used here to run a pump while the Jetson is not connected
const int   PUMP_PINS[]  = {PIN_TOWER_PUMP, PIN_NUT_A, PIN_NUT_B, PIN_JETSON};
const char* PUMP_NAMES[] = {"Tower pump (J2)", "Nutrient A (J3)", "Nutrient B (J4)", "J5 output"};
const int   PUMP_COUNT   = 4;
int pumpOn = -1;                   // index of the running pump, -1 = all off
unsigned long pumpStart = 0;

Adafruit_ADS1115 ads;
DHT dht(PIN_DHT, DHT11);
WebServer server(80);
bool adsOk = false;
String report = "starting...";

float adsVolts(int ch) {
  long sum = 0;
  for (int i = 0; i < 8; i++) sum += ads.readADC_SingleEnded(ch);
  return ads.computeVolts((int16_t)(sum / 8));
}

float distanceMm() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(4);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, 30000);
  return us ? us * 0.343 / 2 : -1;
}

void readSensors() {
  char line[64];
  String r;
  if (adsOk) {
    snprintf(line, sizeof line, "Battery : %.2f V\n", adsVolts(2) * VBAT_FACTOR);              r += line;
    snprintf(line, sizeof line, "pH      : module %.3f V\n", adsVolts(0) * PH_DIVIDER);       r += line;
    snprintf(line, sizeof line, "TDS     : %.3f V\n", adsVolts(1));                            r += line;
  } else {
    r += "ADS1115 : NOT FOUND - check that it sits fully in J10\n";
  }
  float t = dht.readTemperature(), h = dht.readHumidity();
  if (isnan(t)) r += "DHT11   : no reading\n";
  else { snprintf(line, sizeof line, "DHT11   : %.1f C, %.0f %%\n", t, h); r += line; }
  float d = distanceMm();
  if (d < 0) r += "HC-SR04 : no echo\n";
  else { snprintf(line, sizeof line, "HC-SR04 : %.0f mm\n", d); r += line; }
  snprintf(line, sizeof line, "Soil    : raw %d\n", analogRead(PIN_SOIL));                    r += line;
  r += digitalRead(PIN_FLOAT_LOW) == LOW ? "Float   : CLOSED (tank low - pump blocked)\n"
                                         : "Float   : open (water OK)\n";
  report = r;
  Serial.println("--------------------------------");
  Serial.print(report);
}

void allPumpsOff() {
  for (int p : PUMP_PINS) digitalWrite(p, LOW);
  pumpOn = -1;
}

void handleRoot() {
  String page =
    "<html><head><meta name=viewport content='width=device-width'><meta http-equiv=refresh content='2;url=/'>"
    "<title>Board test</title></head><body><h3>Tower board test</h3><pre style='font-size:18px'>" + report +
    "Running : " + (pumpOn < 0 ? String("none") : String(PUMP_NAMES[pumpOn])) + "</pre>";
  for (int i = 0; i < PUMP_COUNT; i++)
    page += "<p><a href='/pump?n=" + String(i) + "' style='font-size:22px'>Run " + PUMP_NAMES[i] +
            (i == 3 ? " (stays on)" : " for 10 s") + "</a></p>";
  page += "<p><a href='/off' style='font-size:22px;color:red'>ALL OFF</a></p></body></html>";
  server.send(200, "text/html", page);
}

void handlePump() {
  int n = server.arg("n").toInt();
  allPumpsOff();
  if (n >= 0 && n < PUMP_COUNT) {
    digitalWrite(PUMP_PINS[n], HIGH);
    pumpOn = n;
    pumpStart = millis();
    Serial.printf("%s ON\n", PUMP_NAMES[n]);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleOff() {
  allPumpsOff();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  int outs[] = {PIN_TOWER_PUMP, PIN_NUT_A, PIN_NUT_B, PIN_JETSON};
  for (int p : outs) { digitalWrite(p, LOW); pinMode(p, OUTPUT); }
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_FLOAT_LOW, INPUT);

  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);
  adsOk = ads.begin(0x48);
  if (adsOk) ads.setGain(GAIN_ONE);
  dht.begin();

  WiFi.softAP("Tower-Test", "towertest");
  server.on("/", handleRoot);
  server.on("/pump", handlePump);
  server.on("/off", handleOff);
  server.begin();
  Serial.println("\nBoard test started - Wi-Fi Tower-Test, page http://192.168.4.1");
}

void loop() {
  static unsigned long last = 0;
  server.handleClient();
  // J5 (index 3) stays on until ALL OFF, so a Jetson has time to boot
  if (pumpOn >= 0 && pumpOn != 3 && millis() - pumpStart >= PUMP_TEST_MS) {
    Serial.printf("%s OFF\n", PUMP_NAMES[pumpOn]);
    allPumpsOff();
  }
  if (millis() - last >= 2000) {
    last = millis();
    readSensors();
  }
}
