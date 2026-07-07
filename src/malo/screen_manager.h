/*#pragma once
#include <vector>
#include <cstdint>
#include "Screen.h"

// Forward declaration of LVGL object type to avoid including lvgl.h in the header
struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

class ScreenManager {
private:
    std::vector<Screen*> screenStack;
    Screen* activeScreen = nullptr;
    Screen* overlayScreen = nullptr;

    // Header LVGL objects
    lv_obj_t* headerContainer = nullptr;
    lv_obj_t* batteryLabel = nullptr;
    lv_obj_t* tickerLabel = nullptr;

    void updateHeaderData();

public:
    void initHeader();
    void navigateTo(Screen* newScreen);
    void goBack();
    void goToRoot();
    void showOverlay(Screen* overlay);
    void closeOverlay();
    void tick();
    void dispatchButton(uint8_t buttonId, bool pressed);
};
*/