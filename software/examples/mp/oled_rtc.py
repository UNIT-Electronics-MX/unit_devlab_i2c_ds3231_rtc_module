from machine import Pin, I2C
from time import sleep
import sys, select
from ssd1306 import SSD1306_I2C

#ESP32H2 Configuration


# --- Pin configuration ---
SDA_PIN = 12
SCL_PIN = 22
LED_PIN = 4
OLED_ADDR = 0x3C
SCREEN_WIDTH = 128
SCREEN_HEIGHT = 64

# --- Initialize hardware ---
i2c = I2C(0, sda=Pin(SDA_PIN), scl=Pin(SCL_PIN), freq=100000)
led = Pin(LED_PIN, Pin.OUT)
display = SSD1306_I2C(SCREEN_WIDTH, SCREEN_HEIGHT, i2c, addr=OLED_ADDR)

# --- DS3231 address ---
DS3231_ADDR = 0x68

# --- BCD utilities ---
def bcd2dec(b): return (b >> 4) * 10 + (b & 0x0F)
def dec2bcd(d): return ((d // 10) << 4) + (d % 10)

# --- RTC functions ---
def rtc_get_datetime():
    data = i2c.readfrom_mem(DS3231_ADDR, 0x00, 7)
    s = bcd2dec(data[0] & 0x7F)
    m = bcd2dec(data[1])
    h = bcd2dec(data[2])
    wd = bcd2dec(data[3]) - 1
    d = bcd2dec(data[4])
    mo = bcd2dec(data[5] & 0x1F)
    y = bcd2dec(data[6]) + 2000
    return (y, mo, d, h, m, s, wd)

def rtc_set_datetime(y, mo, d, h, m, s):
    # Ajuste Zeller + corrección DS3231
    # DS3231: 1 = Domingo … 7 = Sábado
    if mo < 3:
        mo += 12
        y -= 1
    k = y % 100
    j = y // 100
    zeller = (d + (13 * (mo + 1)) // 5 + k + (k // 4) + (j // 4) + (5 * j)) % 7
    # Zeller: 0=Saturday, 1=Sunday, 2=Monday, ... 6=Friday
    # Convert to DS3231 (1=Sunday … 7=Saturday)
    ds_wd = ((zeller + 6) % 7) + 1

    data = bytes([
        dec2bcd(s),
        dec2bcd(m),
        dec2bcd(h),
        dec2bcd(ds_wd),
        dec2bcd(d),
        dec2bcd(mo if mo <= 12 else mo - 12),
        dec2bcd(y - 2000)
    ])
    i2c.writeto_mem(DS3231_ADDR, 0x00, data)


# --- Global variables ---
weekdays = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"]
months = ["January", "February", "March", "April", "May", "June",
          "July", "August", "September", "October", "November", "December"]

alarm_enabled = True
alarm_hour = 16
alarm_minute = 21
last_trigger_minute = -1
colon_on = True
last_blink = 0

# --- Helper functions ---
def print_datetime(dt):
    y, mo, d, h, m, s, wd = dt
    print(f"{weekdays[wd % 7]} {d} {months[mo-1]} {y}  {h:02}:{m:02}:{s:02}")

def parse_datetime(s):
    try:
        y = int(s[0:4]); mo = int(s[5:7]); d = int(s[8:10])
        h = int(s[11:13]); m = int(s[14:16]); se = int(s[17:19])
        rtc_set_datetime(y, mo, d, h, m, se)
        print("✅ Date/time set successfully.")
    except:
        print("Invalid format. Use: H=YYYY-MM-DD HH:MM:SS")

def parse_alarm(s):
    global alarm_hour, alarm_minute, last_trigger_minute
    try:
        h = int(s[0:2]); m = int(s[3:5])
        if 0 <= h < 24 and 0 <= m < 60:
            alarm_hour, alarm_minute = h, m
            last_trigger_minute = -1
            print(f"Alarm set to {h:02}:{m:02}")
        else:
            print("Invalid format. Use: A=HH:MM")
    except:
        print("Invalid format. Use: A=HH:MM")

def show_help():
    print("""
=== RTC/ALARM MENU ===
Commands:
  H=YYYY-MM-DD HH:MM:SS   -> Set date/time
  A=HH:MM                 -> Set alarm time (24h)
  ENA                     -> Enable alarm
  DIS                     -> Disable alarm
  NOW?                    -> Show current date/time
  STATUS                  -> Show configuration
  HELP                    -> Show this help
""")

def show_status():
    print("\n--- STATUS ---")
    print("Alarm:", "ENABLED" if alarm_enabled else "DISABLED")
    print(f"Alarm time: {alarm_hour:02}:{alarm_minute:02}")
    print("Current time:", end=" "); print_datetime(rtc_get_datetime())
    print("---------------\n")

def check_alarm(dt):
    global last_trigger_minute
    if not alarm_enabled:
        return
    y, mo, d, h, m, s, wd = dt
    if h == alarm_hour and m == alarm_minute and s == 0:
        if last_trigger_minute != m:
            print("** ALARM TRIGGERED **")
            led.on()
            last_trigger_minute = m
    elif m != alarm_minute:
        led.off()

# --- OLED drawing ---
def draw_bell(x, y):
    display.line(x+2, y+6, x+5, y, 1)
    display.line(x+5, y, x+8, y+6, 1)
    display.line(x+1, y+6, x+9, y+6, 1)
    display.line(x+3, y+7, x+7, y+7, 1)
    display.pixel(x+5, y+8, 1)

def render_clock(dt):
    global colon_on
    y, mo, d, h, m, s, wd = dt
    display.fill(0)
    display.text(weekdays[wd % 7], 0, 0)
    if alarm_enabled:
        draw_bell(SCREEN_WIDTH - 12, 0)

    # Large time
    sep = ":" if colon_on else " "
    hhmm = "{:02d}{}{:02d}".format(h, sep, m)

    display.text(hhmm, 16, 24, 1)

    # Date
    date_str = f"{d} {months[mo-1]} {y}"
    display.text(date_str, 0, 54, 1)
    display.show()

# --- Start ---
print("Starting DS3231 + OLED...")
devices = i2c.scan()
if DS3231_ADDR not in devices:
    print("❌ DS3231 RTC not found.")
    sys.exit()
if OLED_ADDR not in devices:
    print("⚠️ OLED not found at 0x3C.")

show_help()
show_status()

# --- Main loop ---
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
            alarm_enabled = True; print("Alarm enabled.")
        elif cmd == "DIS":
            alarm_enabled = False; led.off(); print("Alarm disabled.")
        elif cmd == "NOW?":
            print_datetime(rtc_get_datetime())
        elif cmd == "STATUS":
            show_status()
        elif cmd == "HELP":
            show_help()
        else:
            print("Unknown command. Type HELP for options.")

    dt = rtc_get_datetime()
    check_alarm(dt)

    # Colon blink
    colon_on = not colon_on
    render_clock(dt)

    sleep(1)
