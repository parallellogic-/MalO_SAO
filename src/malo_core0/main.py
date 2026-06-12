import malo  # This is your compiled C module
import time

print("Core 0: MicroPython booted successfully!")

# Call your C function to launch your heavy C logic onto Core 1
print("Core 0: Launching C code on Core 1...")
malo_c.launch_core1() 

# Core 0 is now free to do standard MicroPython tasks
while True:
    print("Core 0: Running Python loop...")
    time.sleep(2)
