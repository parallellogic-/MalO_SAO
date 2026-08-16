#include "universal_serial_bus_flash.h"

volatile bool UniversalSerialBus::_is_mount_request=false;
bool UniversalSerialBus::_is_mounted=false;

void UniversalSerialBus::begin()
{
  Serial.begin();
  long start_tms=millis();
  while(!Serial && (millis()-start_tms)<7000) delay(1);//wait for terminal to connect or timeout, whichever is first
  Serial.println("START");

  FlashInterface::begin();
  //FlashInterface::ls();
}

void UniversalSerialBus::update(bool is_core1_shutdown)
{
  //if(Serial.available()>0) set_mounted(); //character received over terminal prompts reqeust to mount as usb mass storage device
  //if any character received over Serial terminal, drop into mounted mode
  if(_is_mount_request && is_core1_shutdown && !_is_mounted)
  {//if core1 has stopped interacting with Flash, then servie the mount request on core0


  /*usb_msc.setID("MalO", "Flash Drive", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb); 
  usb_msc.setReadyCallback(msc_ready_cb);
  usb_msc.setCapacity(DISK_SIZE_BYTES / USB_BLOCK_SIZE, USB_BLOCK_SIZE);
  //usb_msc.setUnitReady(false);

  
    usb_msc.setUnitReady(true);
    usb_msc.begin();

        // 3. Force the USB hardware to re-handshake with the PC
    #if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
        // For RP2040/RP2350 core implementations
        USBDevice.detach();
        delay(500); // Give the host OS time to realize it disconnected
        USBDevice.attach();
    #endif*/

    //tud_connect();
    FlashInterface::format_disk();//if not already formatted

    //FatFSUSB.onPlug(handleUsbPlug);
    //FatFSUSB.onUnplug(handleUsbUnplug);

    // Initialize the USB stack controller
    if (!FatFSUSB.begin()) {
        Serial.println("[Error] USB Mass Storage emulation initialization failed.");
    } else {
        Serial.println("USB Storage running. Connect to a host PC to browse 'MALO'.");
    }

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

/*void handleUsbPlug(uint32_t param) {
    (void)param;
    UniversalSerialBus.set_mounted();//pc_has_control = true;
}*/

void FlashInterface::begin(){
    // Mount the FAT library safely over your custom driver logic
  Serial.println("Mounting FatVolume library framework layer...");
  /*if (!fat_fs.begin(&hardware_block_driver, true, 0)) {
      Serial.println("CRITICAL ERROR: SdFat failed to mount your internal drive partition layout!");
  } else {
      Serial.println("SdFat File System successfully initialized!");
  }*/
  /*format_disk();//if not already formatted

  //FatFSUSB.onPlug(handleUsbPlug);
  //FatFSUSB.onUnplug(handleUsbUnplug);

  // Initialize the USB stack controller
  if (!FatFSUSB.begin()) {
      Serial.println("[Error] USB Mass Storage emulation initialization failed.");
  } else {
      Serial.println("USB Storage running. Connect to a host PC to browse 'MALO'.");
  }
  FatFSUSB.end();*/
  //tud_disconnect();
}

void FlashInterface::format_disk() {
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
