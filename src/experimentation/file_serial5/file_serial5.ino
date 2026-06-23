#include <Adafruit_TinyUSB.h>
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <Arduino.h>

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <SdFat.h>

#define USB_BLOCK_SIZE    512
//#define FLASH_SECTOR_SIZE 4096

Adafruit_USBD_MSC usb_msc;

// Custom 16MB hardware layout boundaries
const uint32_t FLASH_TARGET_OFFSET = 2 * 1024 * 1024; 
const uint32_t DISK_SIZE_BYTES     = 14 * 1024 * 1024; 

// RAM cache staging layouts
static uint8_t sector_cache[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));
static int32_t cached_sector_id = -1;
static bool cache_is_dirty = false;

// ====================================================================
// FORWARD DECLARATIONS (CRITICAL FIX FOR SCOPING ERRORS)
// ====================================================================
int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize);
int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize);
void msc_flush_cb(void);
// ====================================================================

// Inherit from FsBlockDevice to perfectly match the RP2040/RP2350 core config
class RP2350CustomFlashDriver : public FsBlockDevice {
public:
    // Core SdFat v2 uses readSector & writeSector with an optional uint32_t count parameter
    bool readSector(uint32_t sector, uint8_t* dst) override {
        return msc_read_cb(sector, dst, 512) == 512;
    }

    bool writeSector(uint32_t sector, const uint8_t* src) override {
        return msc_write_cb(sector, (uint8_t*)src, 512) == 512;
    }

    bool readSectors(uint32_t sector, uint8_t* dst, size_t count) override {
        return msc_read_cb(sector, dst, count * 512) == (int32_t)(count * 512);
    }

    bool writeSectors(uint32_t sector, const uint8_t* src, size_t count) override {
        return msc_write_cb(sector, (uint8_t*)src, count * 512) == (int32_t)(count * 512);
    }

    bool syncDevice() override {
        msc_flush_cb();
        return true;
    }

    // Required pure virtual functions for FsBlockDevice in this core configuration
    bool isBusy() override { return false; }
    uint32_t sectorCount() override { return DISK_SIZE_BYTES / USB_BLOCK_SIZE; }
};

// Instantiate the file system blocks using core-compliant Types
static RP2350CustomFlashDriver hardware_block_driver;
static FatVolume fat_fs; 
static FsFile sprite_sheet_file;  // Changed 'File' to 'FsFile' to resolve name conflicts

static uint8_t sprite_buffer[128 * 128];
static lv_image_dsc_t sprite_img_dsc;



// CRITICAL FIX: __no_inline_not_in_flash_func forces this code to run purely out of RAM 
// This allows safe writing to the flash while the XIP cache mapping engine is disabled.
void __no_inline_not_in_flash_func(flush_sector_cache)() {
  // Use one of the RP2350's hardware spinlock registers (Lock ID 31 is typically safe/free)
  uint32_t spin_status = spin_lock_blocking(spin_lock_instance(31));

  // Double-check variables inside the protected gateway
  if (cached_sector_id == -1 || !cache_is_dirty) {
    spin_unlock(spin_lock_instance(31), spin_status);
    return;
  }

  uint32_t sector_start = cached_sector_id * FLASH_SECTOR_SIZE;
  uint32_t physical_flash_addr = FLASH_TARGET_OFFSET + sector_start;
  
  Serial.print("[FLASH RUNTIME] Erasing & Writing Sector ID: ");
  Serial.print(cached_sector_id);
  Serial.print(" at Real Addr: 0x");
  Serial.println(physical_flash_addr, HEX);

  // Turn off internal core interrupts completely during the physical write block window
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(physical_flash_addr, FLASH_SECTOR_SIZE);
  flash_range_program(physical_flash_addr, sector_cache, FLASH_SECTOR_SIZE);
  restore_interrupts(ints);

  uint32_t verification_addr = XIP_BASE + physical_flash_addr;
  Serial.print("[FLASH VERIFY] First byte in memory window: 0x");
  Serial.println(*(uint8_t*)verification_addr, HEX);

  cache_is_dirty = false;

  // Release the hardware gate so the other core/thread can safely interact with flash again
  spin_unlock(spin_lock_instance(31), spin_status);
}


int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
  uint32_t drive_offset = (lba * USB_BLOCK_SIZE);
  uint32_t target_sector_id = drive_offset / FLASH_SECTOR_SIZE;
  uint32_t block_offset_in_sector = drive_offset % FLASH_SECTOR_SIZE;

  if (target_sector_id == cached_sector_id) {
    memcpy(buffer, sector_cache + block_offset_in_sector, bufsize);
  } else {
    uint32_t flash_addr = XIP_BASE + FLASH_TARGET_OFFSET + drive_offset;
    memcpy(buffer, (const void*)flash_addr, bufsize);
  }
  return bufsize;
}

int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  uint32_t drive_offset = (lba * USB_BLOCK_SIZE);
  uint32_t target_sector_id = drive_offset / FLASH_SECTOR_SIZE;
  uint32_t block_offset_in_sector = drive_offset % FLASH_SECTOR_SIZE;

  if (target_sector_id != cached_sector_id) {
    // Commit the previous sector out of the RAM pipeline before reallocating layout space
    if (cached_sector_id != -1 && cache_is_dirty) {
       flush_sector_cache();
    }
    
    cached_sector_id = target_sector_id;
    uint32_t sector_start = cached_sector_id * FLASH_SECTOR_SIZE;
    uint32_t physical_flash_read_addr = XIP_BASE + FLASH_TARGET_OFFSET + sector_start;
    
    // Read the unmodified structure layout securely into RAM
    uint32_t ints = save_and_disable_interrupts();
    memcpy(sector_cache, (const void*)physical_flash_read_addr, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
  }

  // Inject the new bytes coming from Linux straight into the active RAM matrix
  memcpy(sector_cache + block_offset_in_sector, buffer, bufsize);
  cache_is_dirty = true;

  return bufsize;
}

void msc_flush_cb(void) {
  flush_sector_cache();
}

bool msc_ready_cb(void) {
  return true; 
}

void debug_raw_sector_check() {
    Serial.println("\n--- RAW HARDWARE FLASH INTEGRITY CHECK ---");
    uint8_t temp_sector[512];
    
    // Read the very first sector of your virtual drive (Sector 0)
    if (msc_read_cb(0, temp_sector, 512) == 512) {
        Serial.print("Sector 0 Signature Check (Should be 0x55, 0xAA): 0x");
        Serial.print(temp_sector[510], HEX);
        Serial.print(", 0x");
        Serial.println(temp_sector[511], HEX);
        
        // Print the first 16 bytes to look for FAT markers like "MSDOS", "FAT", or "FAT32"
        Serial.print("Raw Header Label String: ");
        for(int i = 3; i < 11; i++) {
            if(temp_sector[i] >= 32 && temp_sector[i] <= 126) Serial.print((char)temp_sector[i]);
            else Serial.print(".");
        }
        Serial.println();
    } else {
        Serial.println("CRITICAL ERROR: Failed to execute hardware read command over sector 0!");
    }
    Serial.println("------------------------------------------\n");
}

void setup() {
  Serial.begin(1000000); 

  usb_msc.setID("RP2350B", "Flash Drive", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb); 
  usb_msc.setReadyCallback(msc_ready_cb);
  usb_msc.setCapacity(DISK_SIZE_BYTES / USB_BLOCK_SIZE, USB_BLOCK_SIZE);
  usb_msc.setUnitReady(true);
  
  usb_msc.begin();

  delay(1000); // Allow system registers to settle safely

  // Mount the FAT library safely over your custom driver logic
  Serial.println("Mounting FatVolume library framework layer...");
  if (!fat_fs.begin(&hardware_block_driver, true, 0)) {
      Serial.println("CRITICAL ERROR: SdFat failed to mount your internal drive partition layout!");
  } else {
      Serial.println("SdFat File System successfully initialized!");
  }
}

void demo_print_filesystem() {
    Serial.println("\n=====================================");
    Serial.println("   SDFAT: PRINTING FILE SYSTEM LIST  ");
    Serial.println("=====================================");

    // LS_R flags SdFat to print recursively (lists contents inside folders too)
    // LS_SIZE appends the file sizes next to the file names automatically
    uint8_t flags = LS_R | LS_SIZE;

    // Use the official library's high-speed stream dump tool
    fat_fs.ls(&Serial, flags);

    Serial.println("=====================================\n");
}

bool is_task=false;
void loop() {
  if(!is_task && millis()>7000)
  {
    debug_raw_sector_check();
    demo_print_filesystem();
    is_task=true;
  }

  static uint32_t last_write_time = 0;
  
  // Track if cache is dirty and record the timestamp
  if (cache_is_dirty && last_write_time == 0) {
    last_write_time = millis();
  }

  // If data has been sitting unwritten in our RAM cache for more than 2 seconds,
  // force a physical hardware flush to the flash chip automatically.
  if (cache_is_dirty && (millis() - last_write_time > 2000)) {
    flush_sector_cache();
    last_write_time = 0; // Reset timer
    Serial.println("[AUTO-FLUSH] Idle timeout reached. Cache committed to flash!");
  }
}
