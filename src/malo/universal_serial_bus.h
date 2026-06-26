#pragma once

#define USB_MOUNS_US 34'000 //how long to wait between mount request (and allow all periphreals to safe themselves) vs performing the mounting operation

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