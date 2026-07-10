#pragma once

//USB interacts with Flash, so put file system in here too for shared resource

#include <Adafruit_TinyUSB.h>
#include <SdFat.h>
#include "hardware/flash.h"

//#define USB_MOUNS_US 34'000 //how long to wait between mount request (and allow all periphreals to safe themselves) vs performing the mounting operation
#define USB_BLOCK_SIZE    512
//#define FLASH_SECTOR_SIZE 4096

const uint32_t FLASH_TARGET_OFFSET = 2 * 1024 * 1024; 
const uint32_t DISK_SIZE_BYTES     = 14 * 1024 * 1024; 

int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize);
int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize);
void msc_flush_cb(void);
bool msc_ready_cb(void);

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

class FlashInterface{
  private:
    inline static RP2350CustomFlashDriver hardware_block_driver;
  public:
    static void begin();
    static void ls();
};

class UniversalSerialBus{
  private:
    inline static bool _is_mount_request=false;
    inline static bool _is_mounted=false;
  public:
    static void begin();
    static void update(bool is_core1_shutdown);
    //static void end(); //only way to clear is to reboot the whole device - skips messiness of re-init'ing a bunch of periprehals...
    static bool get_mount_request();
    static bool get_mounted();
    static void set_mounted(); //send a character from terminal, or go into settings and enable USB mount
};