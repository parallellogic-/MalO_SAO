#include "py/runtime.h"
#include "pico/multicore.h"
#include "core1_main.h"

// 1. Define the C function that MicroPython will call
static mp_obj_t malo_init_core1(void) {
    // Pico SDK function to launch core1_entry() on Core 1
    //multicore_launch_core1(malo_core1_entry);
    malo_core1_entry();
    return mp_const_none;
}
// 2. Bind the C function to a MicroPython function object
static MP_DEFINE_CONST_FUN_OBJ_0(malo_init_core1_obj, malo_init_core1);

// New method: set_charlieplex_led(bank_index, led_index, brightness)
static mp_obj_t malo_set_charlieplex_led(mp_obj_t bank_idx_obj, mp_obj_t led_idx_obj, mp_obj_t brightness_obj) {
    uint8_t bank_index = (uint8_t)mp_obj_get_int(bank_idx_obj);
    uint8_t led_index = (uint8_t)mp_obj_get_int(led_idx_obj);
    uint8_t brightness = (uint8_t)mp_obj_get_int(brightness_obj);
    
    bool result = set_charlieplex_led(bank_index, led_index, brightness);
    return mp_obj_new_bool(result);
}
static MP_DEFINE_CONST_FUN_OBJ_3(malo_set_charlieplex_led_obj, malo_set_charlieplex_led);

// New method: set_effective_led_count(bank_index, led_count)
static mp_obj_t malo_set_effective_led_count(mp_obj_t bank_idx_obj, mp_obj_t led_count_obj) {
    uint8_t bank_index = (uint8_t)mp_obj_get_int(bank_idx_obj);
    uint8_t led_count = (uint8_t)mp_obj_get_int(led_count_obj);
    
    bool result = set_effective_led_count(bank_index, led_count);
    return mp_obj_new_bool(result);
}
static MP_DEFINE_CONST_FUN_OBJ_2(malo_set_effective_led_count_obj, malo_set_effective_led_count);

// New method: flush([bank_index]) with fallback logic if no args provided
static mp_obj_t malo_flush(size_t n_args, const mp_obj_t *args) {
    bool result1 = true;
    bool result2 = true;

    if (n_args == 0) {
        // If py provides no arguments, flush(0); and flush(1); are called
        result1 = flush(0);
        result2 = flush(1);
        return mp_obj_new_bool(result1 && result2);
    } else {
        // Otherwise, extract the integer and flush the specific bank index
        uint8_t bank_index = (uint8_t)mp_obj_get_int(args[0]);
        bool result = flush(bank_index);
        return mp_obj_new_bool(result);
    }
}
// MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN allows a minimum of 0 and maximum of 1 argument
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(malo_flush_obj, 0, 1, malo_flush);

// 3. Map the Python function name string to the object
static const mp_rom_map_elem_t malo_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),                MP_ROM_QSTR(MP_QSTR_malo) },
    { MP_ROM_QSTR(MP_QSTR_init_core1),              MP_ROM_PTR(&malo_init_core1_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_charlieplex_led),     MP_ROM_PTR(&malo_set_charlieplex_led_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_effective_led_count), MP_ROM_PTR(&malo_set_effective_led_count_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),                   MP_ROM_PTR(&malo_flush_obj) },
};
static MP_DEFINE_CONST_DICT(malo_module_globals, malo_module_globals_table);

// 4. Create the module structure
const mp_obj_module_t malo_user_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&malo_module_globals,
};

// 5. LOCK IN THE NAME: "import malo"
MP_REGISTER_MODULE(MP_QSTR_malo, malo_user_module);

