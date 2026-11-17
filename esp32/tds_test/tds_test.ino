// TDS + pH test: TDS module -> ADS1115 A1, pH module -> 10k/20k divider -> ADS1115 A0.
// ESP32 I2C on GPIO21 (SDA) / GPIO22 (SCL). Serial Monitor at 115200 baud.

#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

const float WATER_TEMP = 25.0;  // C, until the DS18B20 is connected
const float PH_DIVIDER = 1.5;   // (10k + 20k) / 20k
const float V_PH7 = 2.50, PH_SLOPE = 0.18;   // pH calibration (not done yet)

float readVolts(int channel) {
  long sum = 0;
  for (int i = 0; i < 10; i++) {           // average 10 readings
    sum += ads.readADC_SingleEnded(channel);
    delay(20);
  }
  return ads.computeVolts((int16_t)(sum / 10));
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);
  if (!ads.begin(0x48)) {
    Serial.println("ADS1115 not found - check VDD, GND, SDA, SCL and ADDR wiring");
    while (true) delay(1000);
  }
  ads.setGain(GAIN_ONE);                   // +/-4.096 V range
  Serial.println("TDS + pH test started");
}

void loop() {
  // TDS: module output 0..2.3 V, converted with the standard probe curve, corrected to 25 C
  float vTds = readVolts(1);
  float v25  = vTds / (1.0 + 0.02 * (WATER_TEMP - 25.0));
  float tds  = (133.42 * v25 * v25 * v25 - 255.86 * v25 * v25 + 857.39 * v25) * 0.5;
  if (tds < 0) tds = 0;

  // pH (uncalibrated)
  float vPh = readVolts(0) * PH_DIVIDER;
  float ph  = 7.0 + (V_PH7 - vPh) / PH_SLOPE;

  Serial.print("TDS: ");  Serial.print(tds, 0);  Serial.print(" ppm (");
  Serial.print(vTds, 3);  Serial.print(" V) | pH: ");
  Serial.print(ph, 2);    Serial.print(" (uncalibrated, ");
  Serial.print(vPh, 3);   Serial.println(" V)");
  delay(1000);
}
