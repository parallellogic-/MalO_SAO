# Overview

This page contains the code and design files used to create the SCP-1471-A MalO Simple Add-On (SAO).  These units will be sold online and handed out at DEFCON in August 2026.

As part of the standard DEFCON experience, each attendee receives a circuit board they wear around their neck that serves both as a multi-day ticket for the con and as a platform to show off hardware/software hacks.  These MalO SAOs plug into the expansion port on the official con badge to allow the user to customize their standard DEFCON badge.

Among other things, these MalO units will have a screen and buttons to allow the user to play games, unlock puzzles, display screen savers, view messages, etc.  The devices can communicate with one another over infrared.  There are dozens of LEDs.  The units have a USB port so users can hack them with Python.

Work in progress render (not final artwork)
![Front](img/r1/front.png)

# User Guide

- Thonny editor for hacking Python on Core0
    - https://thonny.org/
    - File upload/download, .py file editing and IDE
- git clone git@github.com:parallellogic-/MalO_SAO.git

git submodule update --init --recursive

# Hardware

- Processor, [RP2350B](doc/spec_sheets/Processor_RP2350B_C42415655.pdf), C42415655
    - A3/A4 revision resolves E9 current leakage, enabling capacitive touch
    - 150 MHz
    - Unique ID (username)
    - Internal temperature sensor
- Flash Memory, W25Q128JVSIQ, C113767
    - 16 MB
- USB-C, C5187472
    - v1.1, 12 Mbps
- 3V3/5V LDO Regulator, LD1117-3.3, C347229
    - Rated >500 mA
    - Dissipates 170 mW at 100 mA load
- Capacitive Touch
    - 1 MOhm to PWM
- Buzzer, MLT-7525, C95299
    - 100 Hz-10 kHz, peak 2.7kHz
- Microphone, LMD2718T261-OA1, C5373237
    - -26 dB sensitivity, 58 dB SNR
- Hall Effect Sensor, HAL403SO, C42387470
    - +/-50 mT
- IR TxD, XL-2012IRC-940, C965889
    - Per LED: 940 nm, 38 kHz PWM (software-defined), 30mA, 2mW/sr, 120° FOV
    - 1x LED default, solder jumper to enable 2 additional IR LEDs (to triple brightness)
- IR RxD, IRM-H638T/TR2, C91447
    - 38 kHz PWM (hardware-defined), 940 nm, 45° FOV
- \[REDACTED\], LTR-308ALS-01, C492382
    - 0.01-157 klux
- IMU, LSM6DS3TR, C95230
    - 6DOF: Accelerometer + Gyro
    - Internal temperature sensor
- Screen, ER-OLED015-3W, BuyDisplay
    - SSD1327 driver interface
    - 128*128 px, 1.5" diagonal
    - 4-bit (16-level grayscale), neighboring pixels (2 nibbles) combined into byte
- SAO header, PZ254V-12-6P, C492420
    - 2 rows, 6 pins, 100 mil, through hole, male
- Potentiometer, TC33X-2-103E, C719176
    - 10 kOhms
- Vref, LM385DBZ-1.2RG, C5145284
    - 1.24V
- RFID, ST25DV04K-IER6C3, C2908287
    - 13.56 MHz
    - Energy harvesting (LED)
- Red-Green LED, XL-1608SURUGC, C5349934
- N-MOSFET, L2N7002SLLT1G, C22446827
    - For motor, IR TxD, buzzer
- Diode, 1N5819WS, C191023
    - For motor, buzzer, +15V
- +15V Boost, MT3540-F23, C181783
    - For screen

# Software

- Core0
    - User-configurable Python scripts
- Core1
    - Compiled C library

# Resources

- DEFCON34 SAO MICD
    - https://www.reddit.com/r/Defcon/comments/1tj1jv3/def_con_34_badge_alert/
- SAO generic standard
    - https://hackaday.io/project/175182-simple-add-ons-sao

# Credits

- PCB Artwork, Stickers, Promotional Artwork
    - [HotGlewd](http://hotglewd.com)
- Pixel Artwork
    - [BitAssembly](https://bitassembly.itch.io/)
- CCA schematic & layout
    - [ParallelLogic](https://github.com/parallellogic-)


