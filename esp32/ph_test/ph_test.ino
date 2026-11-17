// pH sensor test: PH-4502C module -> 10k/20k divider -> ADS1115 A0 -> ESP32 (I2C on GPIO21/22)
// Prints the voltage and an uncalibrated pH value every second (Serial Monitor at 115200 baud).

#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

const float DIVIDER = 1.5;      // (10k + 20k) / 20k: module voltage = A0 voltage x 1.5
const float V_PH7   = 2.50;     // module voltage at pH 7 (set during calibration)
const float SLOPE   = 0.18;     // volts per pH unit (set during calibration)

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);           // SDA, SCL
  if (!ads.begin(0x48)) {
    Serial.println("ADS1115 not found - check VDD, GND, SDA, SCL and ADDR wiring");
    while (true) delay(1000);
  }
  ads.setGain(GAIN_ONE);        // input range +/-4.096 V, 0.125 mV per step
  Serial.println("pH test started");
}

void loop() {
  // average 10 readings to smooth out noise
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += ads.readADC_SingleEnded(0);
    delay(20);
  }
  float raw    = sum / 10.0;
  float vA0    = ads.computeVolts((int16_t)raw);
  float vMod   = vA0 * DIVIDER;
  float ph     = 7.0 + (V_PH7 - vMod) / SLOPE;

  Serial.print("A0: ");      Serial.print(vA0, 3);  Serial.print(" V | module: ");
  Serial.print(vMod, 3);     Serial.print(" V | pH (uncalibrated): ");
  Serial.println(ph, 2);
  delay(1000);
}
