//WAS Adafruit USB with custom call_back functions, IS Pico SDK + FatFS + FatFSUSB
//arduino ide, rp2350.  use Pico SDK, FatFS, FatFSUSB.  have rp2350 format the section of the flash (FYI, 14 MB for file system, 2 MB for applciation program) as FAT is it is not already (should appear as a mass-storage dvice in Windows/Ubunutu with disk name "MALO").
//show how to read to read file contents
//use no callbacks

#include <Arduino.h>
#include <FatFS.h>
#include <FatFSUSB.h>

// Low-level FatFS definitions header file mapping
//#include "./ff.h" 

// Disk metadata definitions
const char* disk_name = "MALO";
//const char* target_file = "/log.txt";

// Volatile tracker for hardware state callback tracking
volatile bool pc_has_control = false;
bool drive_was_connected = false;

// --- DEDICATED FILE READ STREAM ---
void printFileContents(const char* filepath) {
    Serial.printf("\n--- Reading File Data: %s ---\n", filepath);
    
    File file = FatFS.open(filepath, "r");
    if (file) {
        while (file.available()) {
            Serial.print((char)file.read());
        }
        file.close();
        Serial.println("\n--- End of File Stream ---");
    } else {
        Serial.printf("[Error] Could not read target path: %s\n", filepath);
    }
}

// --- SECURE & NON-DESTRUCTIVE STORAGE INITIALIZATION ---
void verifyAndSetupStorage() {
    Serial.println("Checking flash partition integrity...");

    // Mount the storage partition. If it fails, HALT. Never auto-format on a live device.
    if (!FatFS.begin()) {
        Serial.println("[CRITICAL ERROR] Filesystem missing or unreadable on boot!");
        //Serial.println("If this is the first execution, uncomment the format override blocks below.");
        
        // --- EMERGENCY MANUAL INITIALIZATION OVERRIDE ---
        
        if (FatFS.format()) {
            Serial.println("Initial format complete. Remounting...");
            FatFS.begin();
            //fatfs::f_setlabel(disk_name);
        }
        
        while (1) { 
            Serial.println("System Halted: Protecting storage from unintended formatting wipe.");
            delay(2000); 
        }
    }

    // Apply the structural volume identification name tag
    fatfs::f_setlabel(disk_name);
    Serial.println("Filesystem verified and mounted safely.");
}

// --- NATIVE PICO SDK INTERRUPT HOOKS ---
void handleUsbPlug(uint32_t param) {
    (void)param;
    pc_has_control = true;
}

void handleUsbUnplug(uint32_t param) {
    (void)param;
    pc_has_control = false;
}

void setup() {
    Serial.begin(115200);
    delay(3000); // Settling buffer delay for host bus allocation operations

    // Run partition validation without any internal erase conditions
    verifyAndSetupStorage();

    // Create a base data file only if it is genuinely missing
    /*if (!FatFS.exists(target_file)) {
        File test_file = FatFS.open(target_file, "w");
        if (test_file) {
            test_file.println("Drive initialization complete on 'MALO'.");
            test_file.close();
        }
    }*/

    // Verify current data profiles
    //printFileContents(target_file);

    // Register callback methods matching the core Pico SDK footprint signature
    FatFSUSB.onPlug(handleUsbPlug);
    FatFSUSB.onUnplug(handleUsbUnplug);

    // Initialize the USB stack controller
    if (!FatFSUSB.begin()) {
        Serial.println("[Error] USB Mass Storage emulation initialization failed.");
    } else {
        Serial.println("USB Storage running. Connect to a host PC to browse 'MALO'.");
    }
}

void loop() {
    // Pure cache protection block tracking using the global flag state
    if (pc_has_control) {
        if (!drive_was_connected) {
            Serial.println("[USB Alert] Host PC mounted the partition. Closing local access.");
            FatFS.end(); // Sever local system layouts to avoid sector cross-talk corruption
            drive_was_connected = true;
        }
        delay(10);
        return; 
    }

    // Handle reclamation routines if the PC unmounts or ejects the block device
    if (drive_was_connected && !pc_has_control) {
        Serial.println("[USB Alert] Host PC disconnected. Remounting storage internally.");
        delay(500); // Settling gap to allow Ubuntu buffers to finish drops cleanly
        FatFS.begin();
        drive_was_connected = false;
        
        // Stream contents out to confirm changes made by your Ubuntu workstation
        //printFileContents(target_file);
    }

    // --- LOCAL FIRMWARE DATA LOGGING ---
    /*static uint32_t last_log_time = 0;
    if (millis() - last_log_time > 5000) {
        last_log_time = millis();

        Serial.println("Appending internal system heartbeat...");
        File file = FatFS.open(target_file, "a");
        if (file) {
            file.seek(file.size()); // Manual check to push cursor tracking pointer to bottom bounds
            file.printf("Active RP2350 tracking timestamp: %lu\n", millis());
            file.close();
        } else {
            Serial.println("[Error] Loop failed to open target file for writing.");
        }
    }*/
}

