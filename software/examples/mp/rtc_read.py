from machine import Pin, I2C
from time import sleep, localtime
import sys
import select
# esp32h2 Configuration
# --- Configuración de pines ---
SDA_PIN = 12
SCL_PIN = 22
LED_PIN = 4

# --- Inicializa I2C y LED ---
i2c = I2C(0, sda=Pin(SDA_PIN), scl=Pin(SCL_PIN), freq=100000)
led = Pin(LED_PIN, Pin.OUT)

# --- Direcciones del DS3231 ---
DS3231_ADDR = 0x68

# --- Utilidades BCD ---
def bcd2dec(b):
    return (b // 16) * 10 + (b % 16)

def dec2bcd(d):
    return (d // 10) * 16 + (d % 10)

# --- Funciones RTC ---
def rtc_get_datetime():
    data = i2c.readfrom_mem(DS3231_ADDR, 0x00, 7)
    sec = bcd2dec(data[0] & 0x7F)
    minute = bcd2dec(data[1])
    hour = bcd2dec(data[2])
    wday = bcd2dec(data[3])
    day = bcd2dec(data[4])
    month = bcd2dec(data[5] & 0x1F)
    year = bcd2dec(data[6]) + 2000
    return (year, month, day, hour, minute, sec, wday)

def rtc_set_datetime(y, m, d, h, mi, s):
    data = bytes([
        dec2bcd(s),
        dec2bcd(mi),
        dec2bcd(h),
        0,  # Día de la semana (auto)
        dec2bcd(d),
        dec2bcd(m),
        dec2bcd(y - 2000)
    ])
    i2c.writeto_mem(DS3231_ADDR, 0x00, data)

# --- Variables de alarma ---
alarma_enabled = True
alarm_hour = 16
alarm_minute = 21
last_trigger_minute = -1

semana = ["Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"]
meses = ["Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
         "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"]

# --- Funciones auxiliares ---
def print_datetime(dt):
    y, m, d, h, mi, s, wd = dt
    print(f"{semana[wd % 7]} {d} de {meses[m-1]} de {y}  {h:02}:{mi:02}:{s:02}")

def parse_datetime(s):
    # Espera formato "YYYY-MM-DD HH:MM:SS"
    try:
        y = int(s[0:4])
        m = int(s[5:7])
        d = int(s[8:10])
        h = int(s[11:13])
        mi = int(s[14:16])
        se = int(s[17:19])
        rtc_set_datetime(y, m, d, h, mi, se)
        print("Fecha/hora ajustada correctamente.")
    except Exception as e:
        print("Formato inválido. Usa: H=YYYY-MM-DD HH:MM:SS")

def parse_alarm(s):
    global alarm_hour, alarm_minute, last_trigger_minute
    try:
        h = int(s[0:2])
        mi = int(s[3:5])
        if 0 <= h < 24 and 0 <= mi < 60:
            alarm_hour = h
            alarm_minute = mi
            last_trigger_minute = -1
            print(f"Alarma ajustada a {alarm_hour:02}:{alarm_minute:02}")
        else:
            print("Formato inválido. Usa: A=HH:MM")
    except:
        print("Formato inválido. Usa: A=HH:MM")

def show_help():
    print("""
=== MENU RTC/ALARMA ===
Comandos:
  H=YYYY-MM-DD HH:MM:SS   -> Ajustar fecha/hora
  A=HH:MM                  -> Ajustar hora de alarma (24h)
  ENA                      -> Habilitar alarma
  DIS                      -> Deshabilitar alarma
  NOW?                     -> Mostrar fecha/hora actual
  STATUS                   -> Mostrar configuración
  HELP                     -> Mostrar esta ayuda
""")

def show_status():
    print("\n--- ESTADO ---")
    print("Alarma:", "HABILITADA" if alarma_enabled else "DESHABILITADA")
    print(f"Hora de alarma: {alarm_hour:02}:{alarm_minute:02}")
    print("Hora actual:", end=" ")
    print_datetime(rtc_get_datetime())
    print("---------------\n")

def check_alarm(dt):
    global last_trigger_minute
    if not alarma_enabled:
        return
    y, m, d, h, mi, s, wd = dt
    if h == alarm_hour and mi == alarm_minute and s == 0:
        if last_trigger_minute != mi:
            print("** ALARMA ACTIVADA **")
            led.on()
            last_trigger_minute = mi
    elif mi != alarm_minute:
        led.off()

# --- Inicio ---
print("Iniciando DS3231...")
devices = i2c.scan()
if DS3231_ADDR not in devices:
    print("❌ No se encontró el RTC DS3231. Verifica cableado.")
    sys.exit()

print("RTC detectado correctamente.")
show_help()
show_status()

# --- Bucle principal ---
last_print = 0
while True:
    if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
        line = sys.stdin.readline().strip()
        if not line:
            continue
        cmd = line.upper()

        if cmd.startswith("H="):
            parse_datetime(line[2:].strip())
        elif cmd.startswith("A="):
            parse_alarm(line[2:].strip())
        elif cmd == "ENA":
            alarma_enabled = True
            print("Alarma habilitada.")
        elif cmd == "DIS":
            alarma_enabled = False
            led.off()
            print("Alarma deshabilitada.")
        elif cmd == "NOW?":
            print_datetime(rtc_get_datetime())
        elif cmd == "STATUS":
            show_status()
        elif cmd == "HELP":
            show_help()
        else:
            print("Comando no reconocido. Escribe HELP para ver opciones.")

    dt = rtc_get_datetime()
    check_alarm(dt)

    # Imprime cada segundo
    sleep(1)

