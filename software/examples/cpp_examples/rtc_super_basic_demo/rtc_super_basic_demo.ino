#include <Wire.h>
#include "RTClib.h"

RTC_DS3231 rtc;

#define SDA_PIN 6
#define SCL_PIN 7

const int SET_YEAR = 2026;
const int SET_MONTH = 5;
const int SET_DAY = 4;
const int SET_HOUR = 12;
const int SET_MINUTE = 0;
const int SET_SECOND = 0;

const bool FORCE_SET_TIME_ON_BOOT = false;

void printNow(const DateTime& now) {
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());
}



void setup() {
  Serial.begin(115200);

  // Esperar USB estable (muy recomendado en ESP / RP)
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) {
    delay(10);
  }
  delay(200);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!rtc.begin()) {
    Serial.println("ERROR: DS3231 not found");
    while (1) delay(10);
  }

  if (FORCE_SET_TIME_ON_BOOT || rtc.lostPower()) {
    rtc.adjust(DateTime(SET_YEAR, SET_MONTH, SET_DAY,
                        SET_HOUR, SET_MINUTE, SET_SECOND));
    Serial.println("RTC initialized");
  }

  Serial.print("RTC OK: ");
  printNow(rtc.now());
}

void loop() {
  static unsigned long lastPrint = 0;

  if (millis() - lastPrint >= 1000) {
    Serial.print("[RTC] ");
    printNow(rtc.now());
    lastPrint = millis();
  }
}