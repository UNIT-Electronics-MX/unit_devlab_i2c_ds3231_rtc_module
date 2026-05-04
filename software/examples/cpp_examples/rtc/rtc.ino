#include <Wire.h>
#include "RTClib.h"

RTC_DS3231 rtc;

// Ajusta estos pines solo si usas ESP32/ESP32-Cx; en AVR usa Wire.begin();
#define SDA_PIN 6
#define SCL_PIN 7

// LED de alarma
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// Español
const char* semana[7] = { "Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado" };
const char* monthsNames[12] = { "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
                                "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre" };

// Estado de alarma
bool alarmaEnabled = true;
int alarmHour = 16;
int alarmMinute = 21;
int lastTriggerMinute = -1; // Para evitar múltiples disparos en el mismo minuto

// ---- Utilidades ----
void printDateTime(const DateTime& dt) {
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
  // Espera "YYYY-MM-DD HH:MM:SS"
  if (s.length() < 19) return false;
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
  // Espera "HH:MM"
  if (s.length() < 5) return false;
  h = s.substring(0, 2).toInt();
  m = s.substring(3, 5).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  return true;
}

void showMenuHelp() {
  Serial.println(F("\n=== MENU RTC/ALARMA ==="));
  Serial.println(F("Comandos:"));
  Serial.println(F("  H=YYYY-MM-DD HH:MM:SS   -> Ajustar fecha/hora"));
  Serial.println(F("  A=HH:MM                  -> Ajustar hora de alarma (24h)"));
  Serial.println(F("  ENA                      -> Habilitar alarma"));
  Serial.println(F("  DIS                      -> Deshabilitar alarma"));
  Serial.println(F("  NOW?                     -> Mostrar fecha/hora actual"));
  Serial.println(F("  STATUS                   -> Mostrar configuracion"));
  Serial.println(F("  HELP                     -> Mostrar esta ayuda\n"));
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
  printDateTime(now);
  Serial.println();
  Serial.println(F("---------------\n"));
}

// ---- Setup/Loop ----
void setup() {
  // I2C
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(SDA_PIN, SCL_PIN);
#else
  Wire.begin();
#endif

  Serial.begin(115200);
  delay(300);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  if (!rtc.begin()) {
    Serial.println(F("No se encontro el RTC DS3231. Verifica cableado/alimentacion."));
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println(F("RTC sin hora valida. Ajustando a hora de compilacion..."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  showMenuHelp();
  showStatus();
}

void handleSerialMenu() {
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  line.toUpperCase(); // comandos en mayus, pero guarda originales para parsear números
  // Mantén otra copia sin mayúsculas para parseo de números
  String raw = line;

  if (line.startsWith("H=")) {
    // Hora/fecha
    String ts = raw.substring(2);
    ts.trim();
    DateTime dt;
    if (parseDateTime(ts, dt)) {
      rtc.adjust(dt);
      Serial.print(F("Fecha/hora ajustada a: "));
      printDateTime(dt);
      Serial.println();
    } else {
      Serial.println(F("Formato invalido. Usa: H=YYYY-MM-DD HH:MM:SS"));
    }
  } else if (line.startsWith("A=")) {
    // Alarma
    String hhmm = raw.substring(2);
    hhmm.trim();
    int h, m;
    if (parseAlarm(hhmm, h, m)) {
      alarmHour = h;
      alarmMinute = m;
      lastTriggerMinute = -1; // reinicia marcador
      Serial.print(F("Alarma ajustada a "));
      if (alarmHour < 10) Serial.print('0');
      Serial.print(alarmHour);
      Serial.print(':');
      if (alarmMinute < 10) Serial.print('0');
      Serial.println(alarmMinute);
    } else {
      Serial.println(F("Formato invalido. Usa: A=HH:MM (24h)"));
    }
  } else if (line == "ENA") {
    alarmaEnabled = true;
    Serial.println(F("Alarma habilitada."));
  } else if (line == "DIS") {
    alarmaEnabled = false;
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println(F("Alarma deshabilitada."));
  } else if (line == "NOW?") {
    DateTime now = rtc.now();
    printDateTime(now);
    Serial.println();
  } else if (line == "STATUS") {
    showStatus();
  } else if (line == "HELP") {
    showMenuHelp();
  } else {
    Serial.println(F("Comando no reconocido. Escribe HELP para ver opciones."));
  }
}

void checkAlarm(const DateTime& now) {
  if (!alarmaEnabled) return;

  // Dispara exactamente cuando coincide hora/minuto y second()==0, una sola vez por minuto
  if (now.hour() == alarmHour && now.minute() == alarmMinute && now.second() == 0) {
    if (lastTriggerMinute != now.minute()) {
      Serial.println(F("** ALARMA **"));
      digitalWrite(LED_BUILTIN, HIGH);
      lastTriggerMinute = now.minute();
    }
  } else {
    // Apaga LED cuando deje de estar en el minuto de alarma
    if (digitalWrite, false) {} // no-op para evitar warning en algunas toolchains
    if (now.minute() != alarmMinute) {
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
}

unsigned long lastPrint = 0;

void loop() {
  handleSerialMenu();

  DateTime now = rtc.now();
  checkAlarm(now);

  // Imprime la hora cada ~1s para monitoreo (sin saturar Serial)
  if (millis() - lastPrint > 1000) {
    Serial.print(F("[RTC] "));
    printDateTime(now);
    Serial.println();
    lastPrint = millis();
  }

  delay(10);
}
