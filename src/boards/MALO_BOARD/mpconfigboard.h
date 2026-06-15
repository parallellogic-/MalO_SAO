// Board and hardware specific configuration
#define MICROPY_HW_BOARD_NAME                   "ParallelLogic MalO"
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
//#define MICROPY_HW_FLASH_STORAGE_BYTES  (3145728) // 3MB filesystem matching your CMake config
//#define MICROPY_HW_FLASH_ROOT_DIR       (1)       // Tells the boot sequence to initialize from the baked root image
//#define MICROPY_HW_MCU_NAME   "RP2350B"
//#define PICO_RP2350A 0
//#define PICO_RP2350B 1 
//#define PICO_TYPE    PICO_TYPE_RP2350B
//#define MICROPY_HW_NUM_PIN_INDEX (48)

// Explicitly declare 15MB filesystem space matching the CMake file
//#define MICROPY_HW_FLASH_STORAGE_BYTES (15 * 1024 * 1024)
