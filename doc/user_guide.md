# Overview

Note: the following steps are abbreviated for expedience.  If you encounter any issues or would like any clarification, please file an [issue ticket](https://github.com/parallellogic-/MalO_SAO/issues).

This documentation will cover two topics:
- Blinking an LED
- Compiling the application software
- Adding a new LED pattern

# Hardware Setup

## SAO Port 180 Degree Rotation

The DEFCON34 SAO port is mounted 180 degrees rotated from the [traditional specification](https://hackaday.io/project/175182-simple-add-ons-sao).  To allow for this badge to work with the DEFCON34 badge AND badges from other conferences without the need to mount it upside down or require an adapter, a quick-access switch has been installed to rotate 4 of the 6 pins.  The additional 2 pins can be rotated by cutting two jumpers and forming two solder joints.  In the majority of cases (ex for badges that do not use/connect the GPIO pins), simply sliding the switch is sufficient to switch the MalO SAO between inverted and non-inverted configurations.

![SAO pinout rotation](doc/sao_pinout.png)

## InfraRed Boost

By default the DEFCON34 badge is power-limited to 100 mA shared between the two available SAO ports.  The MalO is fitted with InfraRed LEDs that transmit at up to 90 mA, but these are configured to only 30 mA by default to comply with the badge power limitation.  To triple the default transmit power to maximum, solder this jumper closed

![IR TXD Boost](doc/ir_txd_boost.png)

# Software Setup

## Integrated Development Environment

### Arduino IDE

Download and install the latest Arduino IDE as explained [here](https://www.youtube.com/watch?v=SX8z3-BEuWQ)

### Hardware

In Arduino IDE:
File >> Preferences >> "Additional board manager URLs"
paste in the following URL to add RP2040 support:
https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json

Restart the IDE

Go to the "Boards Manager" (of the fiver vertical icons on the left of the screen, it is the second one down)
type "philhower"
click "Install" for the single search hit

### Libraries

In Arduino IDE, go to the "Manage Libraries" ( Ctrl+Shift+I ) screen.  Install the following:
- "lvgl" by kisvegabor
- "Adafruit GFX Library" by Adafruit
- "Adafruit SSD1327" by Adafruit

Make a copy of this file:
[src/malo/support/lv_conf.h](src/malo/support/lv_conf.h)
inside your lvgl scr folder:
~/Arduino/libraries/lvgl/src/

# Demos

Note: compiling and uploading new firmware to the MalO SAO will wipe any existing content up to and possibly including the file system.

The processor used in project can be rebooted by pressing the "Reboot" button on the rear of the device.  It is possible that the device gets hung up or is otherwise non-response.  In these cases, press and hold down the "BOOTSEL" button and then tap the "Reboot" button.  In BOOTSEL mode the device will enumerate as a uf2 USB slave.  From here you can drag and drop uf2 files into it as if it were a USB mass storage drive or compile and uplaod Arduino IDE sketches.  In normal circumstances, it is not necessary to put the device into BOOTSEL mode to upload new code.

Use the following settings in the Arduino IDE:
![RP2350B Configuration](doc/rp2350b_configuration.png)

## Blink LED

Open this Arduino sketch:
[src/experimentation/blink/blink.ino](src/experimentation/blink/blink.ino)

Press Ctrl+U to compile and upload the sketch to the MalO SAO

The green LED will blink.  Refer to the left side of page 1 of the [schematic](doc/r2/schematic.pdf) for more information about the pins connected to the processor:

## Build Production

Open this Arduino sketch:
[src/malo/malo.ino](src/malo/malo.ino)

Press Ctrl+U to compile and upload the sketch to the MalO SAO

## Adding LED Pattern

Look for the `animation_static_red` method in the led.ino file.  Add the following new method below it:
`void Charlieplex::animation_static_yellow(SensorSuite &sensor_suite)`
`{`
`    set_max_effective_led_count(CHARLIPLEX_LED_COUNT/2);`
`    for (uint8_t iter = 0; iter < CHARLIPLEX_LED_COUNT; iter++) set_brightness(iter,128);`
`}`

In the array at the top, add the string and method name:
`const AnimationMapping animation_table[] = {`
`...`
`{"Static Yellow",      &Charlieplex::animation_static_yellow},`
`...`
`};`

Press Ctrl+U to compile and upload the sketch to the MalO SAO.  The new LED pattern will appear in Animations >> LED Upper >> Static Yellow 
