import time
from machine import Pin
import _thread,malo

print("START");

# Set pin 15 as an output and pull it HIGH immediately for r1 mistake on layout
imu_power_pin = Pin(15, Pin.OUT, value=1)
# AN4650 needed for reboot time of IMU
time.sleep_ms(20)

# 1. Start the C library loop on Core 1
print("Launching C code on Core 1...")
new_thread=_thread.start_new_thread(malo.init_core1,())

# 2. Set up LED 0 for Core 0 (Python)
led_core0 = Pin(37, Pin.OUT)

charlie=0;

# 3. Infinite blink loop for Core 0
print("Starting Python loop on Core 0...")
while True:
    led_core0.value(1)
    time.sleep(0.1)  # Blinks faster than Core 1
    led_core0.value(0)
    time.sleep(0.1)
    malo.set_charlieplex_led(0,charlie,255);
    malo.set_charlieplex_led(1,charlie,255);
    charlie+=1;
    charlie=charlie%(24*2);
    malo.flush();


