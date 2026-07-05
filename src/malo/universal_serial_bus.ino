#include "universal_serial_bus.h"
#include <Adafruit_TinyUSB.h>
#include "hardware/flash.h"

#define USB_BLOCK_SIZE    512

Adafruit_USBD_MSC usb_msc; //usb-flash file system interface
// Custom 16MB hardware layout boundaries
const uint32_t FLASH_TARGET_OFFSET = 2 * 1024 * 1024; 
const uint32_t DISK_SIZE_BYTES     = 14 * 1024 * 1024; 
// RAM cache staging layouts
static uint8_t sector_cache[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));
static int32_t cached_sector_id = -1;
static bool cache_is_dirty = false;

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
  usb_msc.setID("MalO", "Flash Drive", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb); 
  usb_msc.setReadyCallback(msc_ready_cb);
  usb_msc.setCapacity(DISK_SIZE_BYTES / USB_BLOCK_SIZE, USB_BLOCK_SIZE);
  usb_msc.setUnitReady(true);
  usb_msc.begin();


  Serial.begin(1'000'000);
  long start_tms=millis();
//  while(!Serial && (millis()-start_tms)<7000) delay(1);//wait for terminal to connect or timeout, whichever is first
  //delay(1);
  Serial.println("START");
}

void UniversalSerialBus::update(bool is_core1_shutdown)
{
  //if(Serial.available()>0) set_mounted(); //character received over terminal prompts reqeust to mount as usb mass storage device
  //if any character received over Serial terminal, drop into mounted mode
  if(_is_mount_request && is_core1_shutdown && !_is_mounted)
  {//if enough time has passed without satisfying the mount request, then servie the mount request



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