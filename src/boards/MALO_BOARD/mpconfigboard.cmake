# Inject the SDK-level package overrides globally across the entire build
#add_compile_definitions(
#    PICO_RP2350A=0
#    PICO_RP2350B=1
#)

# cmake file for Raspberry Pi Pico2
set(PICO_BOARD "malo_core")

# To change the gpio count for QFN-80
set(PICO_NUM_GPIOS 48)
set(MICROPY_BINARY_TYPE flash)
set(PICO_BOOT_STAGE2_CHOOSE w25q080)

list(APPEND PICO_BOARD_HEADER_DIRS ${MICROPY_BOARD_DIR})

set(PICO_FLASH_SIZE_BYTES 16777216)

if(NOT DEFINED MICROPY_HW_FLASH_STORAGE_BYTES)
    set(MICROPY_HW_FLASH_STORAGE_BYTES 14680064)  # 14 * 1024 * 1024
endif()

# === CRITICAL RP2350 FIXES FOR COLD BOOT ===
# Forces crt0.S to package and invoke the XIP boot step needed for custom flash
 add_compile_definitions(PICO_EMBED_XIP_SETUP=1)
#add_compile_definitions(PICO_FLASH_SIZE_BYTES=16777216)
add_compile_definitions(PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)
