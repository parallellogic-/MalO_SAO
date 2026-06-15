# Inject the SDK-level package overrides globally across the entire build
#add_compile_definitions(
#    PICO_RP2350A=0
#    PICO_RP2350B=1
#)

# cmake file for Raspberry Pi Pico2
set(PICO_BOARD "malo_core")

# To change the gpio count for QFN-80
set(PICO_NUM_GPIOS 48)

list(APPEND PICO_BOARD_HEADER_DIRS ${MICROPY_BOARD_DIR})

set(PICO_FLASH_SIZE_BYTES 16777216)

if(NOT DEFINED MICROPY_HW_FLASH_STORAGE_BYTES)
    set(MICROPY_HW_FLASH_STORAGE_BYTES 14680064)  # 14 * 1024 * 1024
endif()
