#pragma once

//USB interacts with Flash, so put file system in here too for shared resource

#include <Adafruit_TinyUSB.h>
#include <SdFat.h>
#include "hardware/flash.h"
#include <string>
#include "pico/multicore.h" // Ensure this is at the top of your file

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
    static FatVolume fat_fs;
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


// --- File System ----

const char* const ACHIEVEMENTS[] = {
    "Champion",
    "Chilly",
    "Defeated",
    "Dance",
    "Dizzy",
    "Evil",
    "Exhausted",
    "Favorite Food",
    "Hacker BSOD",
    "Heat Wave",
    "Innocent",
    "It Is Quiet",
    "It Is Too Quiet",
    "Know MalO",
    "Magnetic Personality",
    "Message Received",
    "Message Sent",
    "MalO Wins",
    "Snooper Booper",
    "Soaking Up Rays",
    "Winner"
};
constexpr size_t ACHIEVEMENT_COUNT = sizeof(ACHIEVEMENTS) / sizeof(ACHIEVEMENTS[0]);


// Packed alignment ensures exact bit-matching across compilers and disk sectors
#pragma pack(push, 1)
struct SaveState {
    private:
    // Helper to find index by string. Returns -1 if not found.
    int get_achievement_index(const std::string& name) const {
        for (size_t i = 0; i < ACHIEVEMENT_COUNT; ++i) {
            if (name == ACHIEVEMENTS[i]) {
                return static_cast<int>(i);
            }
        }
        return -1; // Not found
    }

    public:
    // ⚡ Changed username format constraints to 16 bytes
    uint8_t demo_mode = 0;          
    char username[16] = "malo0000"; 
    uint32_t unlocks = 0;           
    uint32_t seen = 0;              
    uint8_t ula_accepted = 0;       
    uint8_t alert_mode = 0;         
    uint8_t padding = 0;            

    uint16_t crc = 0; // Checksum remains locked as the terminal element

    // =======================================================
    //  DISK I/O MEMBER METHODS WITH CONDITIONAL MUTATORS
    // =======================================================

    bool save(const char* filename) {
        crc = calculate_crc();

        Serial.printf("save 1\n");
        multicore_lockout_start_blocking(); 
        File32 file = FlashInterface::fat_fs.open(filename, O_WRONLY | O_CREAT | O_TRUNC); // [INDEX]
        if (!file)
        {
        Serial.printf("save 2\n");
            multicore_lockout_end_blocking(); 
            return false;
        }

        Serial.printf("save 3\n");
        size_t written = file.write(reinterpret_cast<const uint8_t*>(this), sizeof(SaveState));
        file.close();
        multicore_lockout_end_blocking(); 
        Serial.printf("save 4\n");

        return written == sizeof(SaveState);
    }

    // Parameterless save wrapper targeting the persistent file pathway
    bool save() {
        Serial.printf("save 5\n");
        const char* folder_path = "data";
        const char* file_path = "data/save.bin";

        // Double check directory path existence prior to saving
        if (!FlashInterface::fat_fs.exists(folder_path)) { // [INDEX]
            multicore_lockout_start_blocking(); 
            FlashInterface::fat_fs.mkdir(folder_path);     // [INDEX]
            multicore_lockout_end_blocking(); 
        }
        Serial.printf("save 7\n");

        return save(file_path);
    }

    bool load(const char* filename) {
        File32 file = FlashInterface::fat_fs.open(filename, O_RDONLY); // [INDEX]
        if (!file) return false;

        SaveState temp_state;
        size_t read_bytes = file.read(reinterpret_cast<uint8_t*>(&temp_state), sizeof(SaveState));
        file.close();

        if (read_bytes != sizeof(SaveState)) return false;

        // Verify the binary payload integrity
        bool is_hacker=temp_state.crc != temp_state.calculate_crc();
        bool file_updated=false;

        //if (temp_state.crc == temp_state.calculate_crc()) {
            *this = temp_state; // Map loaded memory layout locally

            // ⚡ CONDITION HOOK: Enforce special assignment rules if user agreement is incomplete
            if (this->ula_accepted == 0) {
                apply_default_rp2350_username();
                file_updated=true;
            }
            //return true;
        //}else{
            
        //}
        if(is_hacker)
        {
            unlock("Hacker BSOD");
            file_updated=true;
        }

        if(file_updated) save();

        return true;
        //return false; 
    }


    // Parameterless load wrapper matching your designated path logic
    bool load() {
        const char* folder_path = "data";
        const char* file_path = "data/save.bin";

        // Try to load the file if it already exists
        if (load(file_path)) {
            return true;
        }

        // Folder/file missing or corrupt. Ensure directory tree exists
        if (!FlashInterface::fat_fs.exists(folder_path)) { // [INDEX]
            FlashInterface::fat_fs.mkdir(folder_path);     // [INDEX]
        }

        // Reset memory back to fresh default values
        *this = SaveState(); 

        // Generate the unique fallback hardware username if needed
        if (this->ula_accepted == 0) {
            apply_default_rp2350_username();
        }

        // Initialize and lock down the default file context on your disk
        save(file_path);

        return true;
    }

    // =======================================================
    //  UNIQUE ID HANDLING HELPER
    // =======================================================
    
    void apply_default_rp2350_username() {
        pico_unique_board_id_t board_id;
        pico_get_unique_board_id(&board_id); // Returns the 8-byte unique OTP identifier [INDEX, INDEX]

        // Combine the last two bytes of the identifier to generate a unique 4-digit number (0-9999)
        uint16_t dynamic_numeric_suffix = ((static_cast<uint16_t>(board_id.id[6]) << 8) | board_id.id[7]) % 10000;

        // Securely format directly into the fixed array, guaranteeing null termination
        memset(username, 0, sizeof(username));
        snprintf(username, sizeof(username), "malo%04d", dynamic_numeric_suffix);
    }

    // =======================================================
    //  CRC METRIC COMPUTATION
    // =======================================================

    uint16_t calculate_crc() const {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(this);
        size_t length = offsetof(SaveState, crc); 

        uint16_t calculated_crc = 0xFFFF;
        for (size_t i = 0; i < length; i++) {
            calculated_crc ^= (static_cast<uint16_t>(bytes[i]) << 8);
            for (uint8_t bit = 0; bit < 8; bit++) {
                if (calculated_crc & 0x8000) {
                    calculated_crc = (calculated_crc << 1) ^ 0x1021;
                } else {
                    calculated_crc <<= 1;
                }
            }
        }
        return calculated_crc;
    }

    // =======================================================
    //  ACHIEVEMENT ASSISTANCE METHODS
    // =======================================================
    

    bool is_unlocked(const std::string& name) const {
        int index = get_achievement_index(name);
        if (index == -1 || index >= 32) return false; 
        return (unlocks & (1UL << index)) != 0;
    }

    bool unlock(const std::string& name) {
        int index = get_achievement_index(name);
        if (index == -1 || index >= 32) return false;
        uint32_t mask = (1UL << index);
        if ((unlocks & mask) != 0) return false;
        unlocks |= mask;
        return true; 
    }

    bool is_seen(const std::string& name) const {
        int index = get_achievement_index(name);
        if (index == -1 || index >= 32) return true; 
        return (seen & (1UL << index)) != 0;
    }

    bool mark_as_seen(const std::string& name) {
        int index = get_achievement_index(name);
        if (index == -1 || index >= 32) return false;
        uint32_t mask = (1UL << index);
        if ((seen & mask) != 0) return false;
        seen |= mask;
        return true;
    }
};
#pragma pack(pop)
