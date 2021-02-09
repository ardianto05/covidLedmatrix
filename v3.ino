# COVID-19 (coronavirus) timer-based live update
# on MicroPython for ESP8266 & MAX7219 LED display
# by Alan Wang, Taiwan
import network, urequests, utime, ntptime
from machine import Pin, SPI, Timer, reset

# MAX7219 driver: https://github.com/mcauser/micropython-max7219
# Note: this driver is designed for 4-in-1 MAX7219 modules.
# VCC: 3.3V or 5V
# GND: GND
# DIN: MOSI (D7)
# CS:  D8
# CLK: SCK (D5)
from max7219 import Matrix8x8

# ----------------------------------------

# WiFi AP ssid and password
SSID = "wifi_ssid"
PW   = "wifi_password"

# COVID API: https://github.com/javieraviles/covidAPI
# change the end of url to your country name
URL = "https://coronavirus-19-api.herokuapp.com/countries/taiwan"

# config parameters
TIMEZONE_HOUR_OFFSET = 8      # timezone hour offset
MAX7219_NUM          = 4      # Number of MAX7219 LED modules
MAX7219_BRIGHTNESS   = 15     # MAX7219 brightness (0-15)
MAX7219_INVERT       = False  # Invert MAX7219 display
MAX7219_SCROLL_DELAY = 30     # MAX7219 display scrolling speed (ms)
CLOCK_TIMER_DELAY    = 900000 # clock update delay (ms)
API_TIMER_DELAY      = 300000 # API query delay (ms)

# format string for displaying data
fmt = "COVID-19 {country} " \
      "case {cases}(+{todayCases}) " \
      "death {deaths} " \
      "recov {recovered} " \
      "update {time}"

DISPLAY_TIMER_DELAY  = (len(fmt) + MAX7219_NUM) * 8 * MAX7219_SCROLL_DELAY + 2000

# ----------------------------------------

print("--- COVID-19 live update service ---")

data           = dict()
data_available = False
timer_api      = Timer(-1)
timer_clock    = Timer(-1)
timer_display  = Timer(-1)
led            = Pin(2, Pin.OUT, value=1)

# setup MAX7219
spi = SPI(1, baudrate=10000000, polarity=0, phase=0)
display = Matrix8x8(spi, Pin(15), MAX7219_NUM)
display.brightness(MAX7219_BRIGHTNESS)
display.fill(0)
display.show()

# connect to WiFi
print("Connecting to WiFi...")
wifi = network.WLAN(network.STA_IF)
wifi.active(True)
wifi.connect(SSID, PW)
while not wifi.isconnected():
    pass
print("Connected.")

# decorator for checking WiFi status
def wifi_check_decorator(func):
    def wrapper(*args, **kwargs):
        if not wifi.isconnected():
            # stop timers
            timer_api.deinit()
            timer_clock.deinit()
            timer_display.deinit()
            # reboot
            reset()
        else:
            # run decorated functions
            led.value(0)
            func(*args, **kwargs)
            led.value(1)
    return wrapper

# query time from NTP server
@wifi_check_decorator
def query_time(timer):
    while True:
        try:
            ntptime.settime()
            break
        except:
            utime.sleep(1)

# query data from API
@wifi_check_decorator
def query_api(timer):
    global data, data_available
    try:
        data_available = False
        # query API
        response = urequests.get(URL)
        # query successful
        if response.status_code == 200:
            # parse data as dictionary
            data = response.json()
            response.close()
            # add query time
            local_time_sec = utime.time() + TIMEZONE_HOUR_OFFSET * 3600
            local_time = utime.localtime(local_time_sec)
            time = "{1:02d}/{2:02d} {3:02d}:{4:02d}".format(*local_time)
            data.update({'time': time})
            data_available = True
    except:
        pass

# display data
def data_display(timer):
    if data_available:
        # generate formatted string
        output = fmt.format(**data)
        # scroll text
        p = MAX7219_NUM * 8
        for p in range(MAX7219_NUM * 8, len(output) * -8 - 1, -1):
            display.fill(MAX7219_INVERT)
            display.text(output, p, 0, not MAX7219_INVERT)
            display.show()
            utime.sleep_ms(MAX7219_SCROLL_DELAY)

# query time and data
print("Querying data for first time...")
query_time(None)
query_api(None)

# initialize timers
print("Start timers...")

timer_clock.init(period=CLOCK_TIMER_DELAY,
                 mode=Timer.PERIODIC,
                 callback=query_time)

timer_api.init(period=API_TIMER_DELAY,
               mode=Timer.PERIODIC,
               callback=query_api)

timer_display.init(period=DISPLAY_TIMER_DELAY,
                   mode=Timer.PERIODIC,
                   callback=data_display)

# display for first time...
print("Display data for first time...")
data_display(None)

print("Service started.")

# ----------------------------------------
# enable the following code if you want debug messages in REPL

'''
while True:
    print("data")
    utime.sleep(30)
'''
