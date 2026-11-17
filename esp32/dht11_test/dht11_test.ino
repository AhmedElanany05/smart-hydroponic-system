// DHT11 test: air temperature and humidity on GPIO19. Serial Monitor at 115200 baud.
// Library: "DHT sensor library" by Adafruit (install it with its dependencies).

#include <DHT.h>

DHT dht(19, DHT11);

void setup() {
  Serial.begin(115200);
  delay(500);
  dht.begin();
  Serial.println("DHT11 test started");
}

void loop() {
  delay(2000);                    // the DHT11 needs about 2 s between readings
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("No reading - check wiring (VCC 3V3, GND, DATA on GPIO19)");
    return;
  }
  Serial.print("Air temperature: "); Serial.print(t, 1); Serial.print(" C | Humidity: ");
  Serial.print(h, 0); Serial.println(" %");
}
