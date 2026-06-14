import time
from machine import Pin
#import malo

# 1. Start the C library loop on Core 1
print("Launching C code on Core 1...")
#malo.init_core1()

# 2. Set up LED 0 for Core 0 (Python)
led_core0 = Pin(0, Pin.OUT)

# 3. Infinite blink loop for Core 0
print("Starting Python loop on Core 0...")
while True:
    led_core0.value(1)
    time.sleep(0.2)  # Blinks faster than Core 1
    led_core0.value(0)
    time.sleep(0.2)

