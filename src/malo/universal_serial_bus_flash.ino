#include "universal_serial_bus_flash.h"
#include <FatLib/FatFormatter.h>

Adafruit_USBD_MSC usb_msc; //usb-flash file system interface
// Custom 16MB hardware layout boundaries
// RAM cache staging layouts
static uint8_t sector_cache[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));
static int32_t cached_sector_id = -1;
static bool cache_is_dirty = false;

static RP2350CustomFlashDriver hardware_block_driver;
FatVolume FlashInterface::fat_fs; 

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

void UniversalSerialBus::begin()
{

  //usb_msc.setUnitReady(true);
  //usb_msc.begin();

  Serial.begin(1'000'000);
  long start_tms=millis();
  while(!Serial && (millis()-start_tms)<7000) delay(1);//wait for terminal to connect or timeout, whichever is first
  Serial.println("START");

  FlashInterface::begin();
  FlashInterface::ls();
}

void UniversalSerialBus::update(bool is_core1_shutdown)
{
  //if(Serial.available()>0) set_mounted(); //character received over terminal prompts reqeust to mount as usb mass storage device
  //if any character received over Serial terminal, drop into mounted mode
  if(_is_mount_request && is_core1_shutdown && !_is_mounted)
  {//if core1 has stopped interacting with Flash, then servie the mount request on core0
      delay(20);
      Serial.println("Mount USB... _is_mount_request && is_core1_shutdown && !_is_mounted");


    // 2. CONFIGURE THE USB DEVICE APPEARANCE
    usb_msc.setID("MalO", "Flash Drive", "1.0");
    usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb); 
    usb_msc.setReadyCallback(msc_ready_cb);
    usb_msc.setCapacity(DISK_SIZE_BYTES / USB_BLOCK_SIZE, USB_BLOCK_SIZE);
    //usb_msc.setUnitReady(false);


    // 1. CHECK IF THE DRIVE IS FORMATTED BEFORE INITIALIZING THE USB INTERFACE
    // fatfs.begin returns false if it can't mount a valid FAT partition
    if (!FlashInterface::fat_fs.begin(&FlashInterface::hardware_block_driver)) {
      Serial.println("MalO Filesystem not found or corrupted. Formatting drive...");
      
      FatFormatter formatter;
      uint8_t formatWorkspace[512]; // Buffer required by the formatter to build sectors
      
      // 3. Format the block device directly
      if (!formatter.format(&FlashInterface::hardware_block_driver, formatWorkspace, &Serial)) {
          // Formatting failed handling
          //return;
      //}

      // Attempt to format the flash memory
      //if (!FlashInterface::fat_fs.format(&FlashInterface::hardware_block_driver)) {
        Serial.println("Critical Error: Failed to format MalO flash drive.");
        // Optional: blink an error LED or handle the hardware failure here
      } else {
        Serial.println("MalO Format successful!");
        
        // Remount the newly formatted filesystem to ensure it works
        if (!FlashInterface::fat_fs.begin(&FlashInterface::hardware_block_driver)) {
          Serial.println("Error: Failed to mount MalO filesystem after formatting.");
        }
      }
    } else {
      Serial.println("Valid MalO filesystem detected. Skipping format.");
    }

    usb_msc.setUnitReady(true);
    usb_msc.begin();

    // 3. Force the USB hardware to re-handshake with the PC
    #if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
        // For RP2040/RP2350 core implementations
        USBDevice.detach();
        delay(500); // Give the host OS time to realize it disconnected
        USBDevice.attach();
    #endif

    _is_mounted=true;
  }
  if(_is_mounted && is_core1_shutdown)
  {//polling service of file system

  }
}

//true = manifest as USB mass storage drive on computer
void UniversalSerialBus::set_mounted(){ _is_mount_request=true; }
bool UniversalSerialBus::get_mounted(){ return _is_mounted; }
bool UniversalSerialBus::get_mount_request(){ return _is_mount_request; }

/*void FlashInterface::begin(){
    // Mount the FAT library safely over your custom driver logic
  Serial.println("Mounting FatVolume library framework layer...");
  if (!fat_fs.begin(&hardware_block_driver, true, 0)) {
      Serial.println("CRITICAL ERROR: SdFat failed to mount your internal drive partition layout!");
  } else {
      Serial.println("SdFat File System successfully initialized!");
  }
}*/

void FlashInterface::begin(){
  Serial.println("Mounting FatVolume library framework layer...");
  
  // 1. Try to mount the existing filesystem
  // Passing 'false' as the second parameter prevents it from throwing unhandled panics immediately
  if (!FlashInterface::fat_fs.begin(&FlashInterface::hardware_block_driver)) {
      Serial.println("MalO Filesystem not found or corrupted. Formatting drive...");
      
      FatFormatter formatter;
      uint8_t formatWorkspace[512]; // Buffer required by the formatter to build sectors
      
      // 3. Format the block device directly
      if (!formatter.format(&FlashInterface::hardware_block_driver, formatWorkspace, &Serial)) {
          // Formatting failed handling
          //return;
      //}

      // Attempt to format the flash memory
      //if (!FlashInterface::fat_fs.format(&FlashInterface::hardware_block_driver)) {
        Serial.println("Critical Error: Failed to format MalO flash drive.");
        // Optional: blink an error LED or handle the hardware failure here
      } else {
        Serial.println("MalO Format successful!");
        
        // Remount the newly formatted filesystem to ensure it works
        if (!FlashInterface::fat_fs.begin(&FlashInterface::hardware_block_driver)) {
          Serial.println("Error: Failed to mount MalO filesystem after formatting.");
        }
      }
    } else {
      Serial.println("Valid MalO filesystem detected. Skipping format.");
    }
  
  Serial.println("SdFat File System successfully initialized!");
}

void FlashInterface::ls()
{
    Serial.println("\n=====================================");
    Serial.println("   SDFAT: PRINTING FILE SYSTEM LIST  ");
    Serial.println("=====================================");

    uint8_t flags = LS_R | LS_SIZE;
    fat_fs.ls(&Serial, flags);
}