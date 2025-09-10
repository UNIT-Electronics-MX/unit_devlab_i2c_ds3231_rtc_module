#include <Wire.h>
#include "RTClib.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_DRV2605.h>

//////////////////// Pines / I2C ////////////////////
#define SDA_PIN 6
#define SCL_PIN 7

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

//////////////////// OLED ////////////////////
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//////////////////// RTC ////////////////////
RTC_DS3231 rtc;

//////////////////// DRV2605L (Haptic) ////////////////////
Adafruit_DRV2605 drv;
// Dirección por defecto 0x5A (DRV2605L_ADDR). Cambia aquí si tu placa difiere.
const uint8_t DRV2605_I2C_ADDR = 0x5A;

//////////////////// Localización ////////////////////
const char* semana[7] = { "Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado" };
const char* monthsNames[12] = { "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
                                "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre" };

//////////////////// Alarma ////////////////////
bool alarmaEnabled = true;
int alarmHour = 16;
int alarmMinute = 21;
int lastTriggerMinute = -1;

//////////////////// UI / Tiempo ////////////////////
bool colonOn = true;
unsigned long lastBlink = 0;
unsigned long lastPrint = 0;

//////////////////// Utilidades ////////////////////
void printDateTimeSerial(const DateTime& dt) {
  Serial.print(semana[dt.dayOfTheWeek()]);
  Serial.print(" ");
  Serial.print(dt.day());
  Serial.print(" de ");
  Serial.print(monthsNames[dt.month() - 1]);
  Serial.print(" de ");
  Serial.print(dt.year());
  Serial.print("  ");
  if (dt.hour() < 10) Serial.print('0');
  Serial.print(dt.hour());
  Serial.print(':');
  if (dt.minute() < 10) Serial.print('0');
  Serial.print(dt.minute());
  Serial.print(':');
  if (dt.second() < 10) Serial.print('0');
  Serial.print(dt.second());
}

bool parseDateTime(const String& s, DateTime& out) {
  if (s.length() < 19) return false; // "YYYY-MM-DD HH:MM:SS"
  int Y = s.substring(0, 4).toInt();
  int M = s.substring(5, 7).toInt();
  int D = s.substring(8, 10).toInt();
  int h = s.substring(11, 13).toInt();
  int m = s.substring(14, 16).toInt();
  int sec = s.substring(17, 19).toInt();
  if (Y < 2000 || M < 1 || M > 12 || D < 1 || D > 31 || h < 0 || h > 23 || m < 0 || m > 59 || sec < 0 || sec > 59) {
    return false;
  }
  out = DateTime(Y, M, D, h, m, sec);
  return true;
}

bool parseAlarm(const String& s, int& h, int& m) {
  if (s.length() < 5) return false; // "HH:MM"
  h = s.substring(0, 2).toInt();
  m = s.substring(3, 5).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  return true;
}

void showMenuHelp() {
  Serial.println(F("\n=== MENU RTC/ALARMA ==="));
  Serial.println(F("  H=YYYY-MM-DD HH:MM:SS   -> Ajustar fecha/hora"));
  Serial.println(F("  A=HH:MM                  -> Ajustar hora de alarma (24h)"));
  Serial.println(F("  ENA                      -> Habilitar alarma"));
  Serial.println(F("  DIS                      -> Deshabilitar alarma"));
  Serial.println(F("  NOW?                     -> Mostrar fecha/hora actual"));
  Serial.println(F("  STATUS                   -> Mostrar configuracion"));
  Serial.println(F("  TEST                     -> Vibracion de prueba (DRV2605L)"));
  Serial.println(F("  HELP                     -> Esta ayuda\n"));
}

void showStatus() {
  Serial.println(F("\n--- ESTADO ---"));
  Serial.print(F("Alarma: "));
  Serial.println(alarmaEnabled ? F("HABILITADA") : F("DESHABILITADA"));
  Serial.print(F("Hora de alarma: "));
  if (alarmHour < 10) Serial.print('0');
  Serial.print(alarmHour);
  Serial.print(':');
  if (alarmMinute < 10) Serial.print('0');
  Serial.println(alarmMinute);
  Serial.print(F("Hora actual: "));
  DateTime now = rtc.now();
  printDateTimeSerial(now);
  Serial.println();
  Serial.println(F("---------------\n"));
}

//////////////////// Haptic ////////////////////
// Secuencia de alarma: vibración fuerte -> pausa -> doble pulso -> repetición
// Ver "Adafruit_DRV2605" efectos (0..123 aprox). 85-87 suelen ser fuertes ERM; ajusta al gusto.
void playHapticBurst(uint8_t effect) {
  drv.setWaveform(0, effect);   // efecto
  drv.setWaveform(1, 0);        // fin
  drv.go();
}

void playHapticAlarmPattern(unsigned long duration_ms = 4000) {
  unsigned long t0 = millis();
  while (millis() - t0 < duration_ms) {
    // Patrón: fuerte (85), breve pausa, doble pulso (47)
    playHapticBurst(85); delay(180);
    delay(70);
    playHapticBurst(47); delay(200);
    delay(150);
  }
}

//////////////////// OLED ////////////////////
void drawBellIcon(int16_t x, int16_t y) {
  display.drawTriangle(x+2, y+6, x+5, y, x+8, y+6, SSD1306_WHITE);
  display.drawLine(x+1, y+6, x+9, y+6, SSD1306_WHITE);
  display.drawLine(x+3, y+7, x+7, y+7, SSD1306_WHITE);
  display.drawPixel(x+5, y+8, SSD1306_WHITE);
}

void renderClock(const DateTime& now) {
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(semana[now.dayOfTheWeek()]);

  if (alarmaEnabled) drawBellIcon(SCREEN_WIDTH - 12, 0);

  display.setTextSize(3);
  int charW = 6 * 3;
  int textW = charW * 5; // "HH:MM"
  int x = (SCREEN_WIDTH - textW) / 2;
  int y = 18;

  display.setCursor(x, y);
  if (now.hour() < 10) display.print('0');
  display.print(now.hour());

  display.setCursor(x + charW*2, y);
  display.print(colonOn ? ":" : " ");

  display.setCursor(x + charW*3, y);
  if (now.minute() < 10) display.print('0');
  display.print(now.minute());

  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(now.day());
  display.print(" ");
  display.print(monthsNames[now.month()-1]);
  display.print(" ");
  display.print(now.year());

  display.display();
}

//////////////////// Serial Menu ////////////////////
void handleSerialMenu() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  String raw = line;
  String upper = line; upper.toUpperCase();

  if (upper.startsWith("H=")) {
    String ts = raw.substring(2); ts.trim();
    DateTime dt;
    if (parseDateTime(ts, dt)) {
      rtc.adjust(dt);
      Serial.print(F("Fecha/hora ajustada a: "));
      printDateTimeSerial(dt);
      Serial.println();
    } else {
      Serial.println(F("Formato invalido. Usa: H=YYYY-MM-DD HH:MM:SS"));
    }
  } else if (upper.startsWith("A=")) {
    String hhmm = raw.substring(2); hhmm.trim();
    int h, m;
    if (parseAlarm(hhmm, h, m)) {
      alarmHour = h; alarmMinute = m; lastTriggerMinute = -1;
      Serial.print(F("Alarma ajustada a "));
      if (alarmHour < 10) Serial.print('0');
      Serial.print(alarmHour); Serial.print(':');
      if (alarmMinute < 10) Serial.print('0');
      Serial.println(alarmMinute);
    } else {
      Serial.println(F("Formato invalido. Usa: A=HH:MM (24h)"));
    }
  } else if (upper == "ENA") {
    alarmaEnabled = true; Serial.println(F("Alarma habilitada."));
  } else if (upper == "DIS") {
    alarmaEnabled = false; digitalWrite(LED_BUILTIN, LOW); Serial.println(F("Alarma deshabilitada."));
  } else if (upper == "NOW?") {
    DateTime now = rtc.now(); printDateTimeSerial(now); Serial.println();
  } else if (upper == "STATUS") {
    showStatus();
  } else if (upper == "TEST") {
    Serial.println(F("Vibracion de prueba..."));
    playHapticAlarmPattern(1000);
    Serial.println(F("OK"));
  } else if (upper == "HELP") {
    showMenuHelp();
  } else {
    Serial.println(F("Comando no reconocido. Escribe HELP."));
  }
}

//////////////////// Lógica de alarma ////////////////////
void checkAlarm(const DateTime& now) {
  if (!alarmaEnabled) return;

  if (now.hour() == alarmHour && now.minute() == alarmMinute && now.second() == 0) {
    if (lastTriggerMinute != now.minute()) {
      Serial.println(F("** ALARMA **"));
      digitalWrite(LED_BUILTIN, HIGH);
      playHapticAlarmPattern(4000); // ~4s de vibración
      lastTriggerMinute = now.minute();
    }
  } else {
    if (now.minute() != alarmMinute) {
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
}

//////////////////// Setup / Loop ////////////////////
void setup() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(SDA_PIN, SCL_PIN);
#else
  Wire.begin();
#endif

  Serial.begin(115200);
  delay(200);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("No se encontro OLED (0x3C)."));
    while (1) delay(10);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Reloj OLED + RTC + HAPTIC"));
  display.display();
  delay(600);

  // RTC
  if (!rtc.begin()) {
    Serial.println(F("No se encontro DS3231."));
    while (1) delay(10);
  }
  if (rtc.lostPower()) {
    Serial.println(F("RTC sin hora valida. Ajustando a compilacion..."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // DRV2605L (Haptic)
if (!drv.begin(&Wire)) {
    Serial.println(F("No se encontro DRV2605L (0x5A?)."));
    while (1) delay(10);
  }
  // Selecciona librería de efectos (1 es común), modo trigger interno
  drv.selectLibrary(1);
  // Tipo de motor:
  // drv.useLRA(); // Descomenta si tu motor es LRA
  drv.useERM();    // ERM por defecto
  drv.setMode(DRV2605_MODE_INTTRIG);

  showMenuHelp();
  showStatus();
}

void loop() {
  handleSerialMenu();

  DateTime now = rtc.now();
  checkAlarm(now);

  if (millis() - lastBlink > 500) {
    colonOn = !colonOn;
    lastBlink = millis();
  }

  renderClock(now);

  if (millis() - lastPrint > 2000) {
    Serial.print(F("[RTC] "));
    printDateTimeSerial(now);
    Serial.println();
    lastPrint = millis();
  }

  delay(10);
}
