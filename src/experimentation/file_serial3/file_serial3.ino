#include <Adafruit_TinyUSB.h>
#include "hardware/flash.h"

#define USB_BLOCK_SIZE 512

// Create the USB Mass Storage instance
Adafruit_USBD_MSC usb_msc;

// Set up boundary layouts for your 16MB Flash
// Reserve the first 4MB for code/firmware, use the remaining 12MB for files
const uint32_t FLASH_TARGET_OFFSET = 4 * 1024 * 1024; 
const uint32_t DISK_SIZE_BYTES     = 12 * 1024 * 1024; 

// Callback triggered when your PC reads storage blocks
int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
  uint32_t flash_addr = XIP_BASE + FLASH_TARGET_OFFSET + (lba * USB_BLOCK_SIZE);
  memcpy(buffer, (const void*)flash_addr, bufsize);
  return bufsize;
}

// Callback triggered when your PC writes storage blocks
int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  uint32_t flash_offset = FLASH_TARGET_OFFSET + (lba * USB_BLOCK_SIZE);
  
  // Disable interrupts safely during a hardware flash write operation
  uint32_t ints = save_and_disable_interrupts();
  
  // Erase the hardware sector if the address lands exactly on a 4096-byte boundary
  if (flash_offset % FLASH_SECTOR_SIZE == 0) {
    flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
  }
  
  // Program the 512-byte block from the computer
  flash_range_program(flash_offset, buffer, bufsize);
  
  restore_interrupts(ints);
  return bufsize;
}

// Critical Callback: Tells Ubuntu that the drive has a storage medium inserted
bool msc_ready_cb(void) {
  return true; 
}

void setup() {
  Serial.begin(1000000); 

  // Configure the USB drive device properties
  usb_msc.setID("RP2350B", "Flash Drive", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, NULL);
  usb_msc.setReadyCallback(msc_ready_cb); // Fixes the "Media Removed" bug
  
  // Set the drive capacity block structure 
  usb_msc.setCapacity(DISK_SIZE_BYTES / USB_BLOCK_SIZE, USB_BLOCK_SIZE);
  
  // Tell the operating system it can read and write to this disk
  usb_msc.setUnitReady(true);
  
  usb_msc.begin();
}

void loop() {
  // Leave empty or add non-blocking application tasks
}
