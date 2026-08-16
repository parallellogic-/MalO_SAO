//WAS Adafruit USB with custom call_back functions, IS Pico SDK + FatFS + FatFSUSB
//arduino ide, rp2350.  use Pico SDK, FatFS, FatFSUSB.  have rp2350 format the section of the flash (FYI, 14 MB for file system, 2 MB for applciation program) as FAT is it is not already (should appear as a mass-storage dvice in Windows/Ubunutu with disk name "MALO").
//show how to read to read file contents
//use no callbacks

#include <Arduino.h>
#include <FatFS.h>
#include <FatFSUSB.h>

const char* disk_name = "MALO";
const char* target_file = "/log.txt";
bool drive_was_connected = false;
volatile bool pc_has_control = false;

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

// --- SECURE STORAGE INITIALIZATION & AUTO-FORMAT ---
/*void verifyAndSetupStorage() {
    Serial.println("Checking flash partition integrity...");

    // Step 1: Attempt to mount the storage partition
    if (!FatFS.begin()) {
        Serial.println("[Warning] Filesystem missing or damaged. Triggering clean format...");
        
        // Step 2: Format the partition if mounting fails
        // FatFS.format() clears the 14MB block allocated in the IDE Tools menu
        if (FatFS.format()) {
            Serial.println("Format successful. Mounting newly created drive partition...");
            
            // Re-mount following formatting operation
            if (!FatFS.begin()) {
                Serial.println("[Fatal Error] Failed to mount partition post-format!");
                while (1) { delay(1000); }
            }
        } else {
            Serial.println("[Fatal Error] Flash formatting execution failed!");
            while (1) { delay(1000); }
        }
    }

    // Step 3: Enforce the "MALO" volume identity label on the partition
    //FatFS.setLabel(disk_name);
    Serial.println("Filesystem verified and mounted safely.");
}*/

/*void verifyAndSetupStorage() {
    if (!FatFS.begin()) {
        Serial.println("FatFS failed to mount inside setup!");
        while(1);
    }
}*/

void verifyAndSetupStorage() {
    Serial.println("Checking flash partition integrity...");

    // Step 1: Attempt to mount the storage partition
    if (!FatFS.begin()) {
        Serial.println("[Warning] Filesystem missing or damaged. Triggering clean FAT format...");
        
        // Step 2: Format the partition (FatFS.format takes 0 arguments)
        if (FatFS.format()) {
            Serial.println("Format successful. Mounting newly created drive partition...");
            
            // Re-mount following formatting operation
            if (!FatFS.begin()) {
                Serial.println("[Fatal Error] Failed to mount partition post-format!");
                while (1) { delay(1000); }
            }
            
            // Step 3: FIXED - Accessing low-level FatFS using the explicit fatfs:: namespace
            fatfs::FRESULT res = fatfs::f_setlabel(disk_name);
            if (res == fatfs::FR_OK) {
                Serial.printf("Partition volume label successfully assigned as '%s'\n", disk_name);
            } else {
                Serial.printf("[Warning] Failed to write partition label. Code: %d\n", res);
            }
            
        } else {
            Serial.println("[Fatal Error] Flash formatting execution failed!");
            while (1) { delay(1000); }
        }
    } else {
        // Partition mounted fine. Enforce or refresh the label string dynamically inside fatfs space
        fatfs::f_setlabel(disk_name);
    }

    Serial.println("Filesystem verified and mounted safely.");
}


// 1. Triggered automatically when the PC physically mounts the device
void onUsbPlug(uint32_t param) {
    (void)param;
    pc_has_control = true;
    
    // CRITICAL: Close down local execution allocations so the host PC 
    // can securely manage raw storage sector layouts safely.
    FatFS.end();
}

// 2. Triggered automatically when the PC safely ejects/unmounts the drive
void onUsbUnplug(uint32_t param) {
    (void)param;
    // Re-mount internal storage layouts for local board write/read calls
    if (FatFS.begin()) {
        pc_has_control = false;
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000); // Buffer for Serial terminal setup

    // Run partition validation and formatting check
    verifyAndSetupStorage();

    // Create a default file if it doesn't exist yet
    if (!FatFS.exists(target_file)) {
        File test_file = FatFS.open(target_file, "w");
        if (test_file) {
            test_file.println("Drive initialization complete on 'MALO'.");
            test_file.close();
        }
    }

    // Output sample content to confirm access
    printFileContents(target_file);

    // --- ENFORCE THE "MALO" VOLUME IDENTITY ---
    // Under Pico SDK, this changes what the host computer reports in the file manager
    //PicoUSB.setManufacturer("Custom Hardware");
    //PicoUSB.setProduct(disk_name);

    // Initialize the USB Mass Storage Emulation Stack (No callbacks passed)
    if (!FatFSUSB.begin()) {
        Serial.println("[Error] USB Mass Storage emulation initialization failed.");
    } else {
        Serial.println("USB Storage running. Connect to a host PC to browse 'MALO'.");
    }
}

void loop() {
    // Poll host connection status strictly without callback overrides
    //bool pc_has_control = FatFSUSB.driveConnected();

    if (pc_has_control) {
        if (!drive_was_connected) {
            Serial.println("[USB Alert] Host PC mounted the partition. Closing local access.");
            FatFS.end(); // Sever local ties to prevent tables overlapping
            drive_was_connected = true;
        }
        delay(10);
        return; 
    }

    // If the PC unmounts/ejects, automatically recover local file systems
    if (drive_was_connected && !pc_has_control) {
        Serial.println("[USB Alert] Host PC disconnected. Remounting storage internally.");
        FatFS.begin();
        drive_was_connected = false;
        
        // Read contents to view host changes
        printFileContents(target_file);
    }

    // --- MICROCUT EXECUTION SEGMENT (LOCAL STORAGE MODIFICATION) ---
    static uint32_t last_log_time = 0;
    if (millis() - last_log_time > 5000) {
        last_log_time = millis();

        Serial.println("Appending internal system heartbeat...");
        File file = FatFS.open(target_file, "a");
        if (file) {
            file.printf("Active RP2350 tracking timestamp: %lu\n", millis());
            file.close();
        }
    }
}
