#include "universal_serial_bus.h"

void UniversalSerialBus::begin()
{
  Serial.begin();//TODO: move to USB
  long start_tms=millis();
  while(!Serial && (millis()-start_tms)<7000);//wait for terminal to connect or timeout, whichever is first
  Serial.println("START");
}

void UniversalSerialBus::update(bool is_core1_shutdown)
{
  if(Serial.available()>0) set_mounted(); //character received over terminal prompts reqeust to mount as usb mass storage device
  //if any character received over Serial terminal, drop into mounted mode
  if(_is_mount_request && is_core1_shutdown && !_is_mounted)
  {//if enough time has passed without satisfying the mount request, then servie the mount request



    _is_mounted=true;
  }
}

//true = manifest as USB mass storage drive on computer
void UniversalSerialBus::set_mounted(){ _is_mount_request=true; }
bool UniversalSerialBus::get_mounted(){ return _is_mounted; }
bool UniversalSerialBus::get_mount_request(){ return _is_mount_request; }