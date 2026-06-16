#include "py/runtime.h"
#include "pico/multicore.h"
#include "core1_main.h" //malo_core1_entry(void);
//#include "led.h"

// 1. Define the C function that MicroPython will call
static mp_obj_t malo_init_core1(void) {
    // Pico SDK function to launch core1_entry() on Core 1
    //multicore_launch_core1(malo_core1_entry);
    malo_core1_entry();
    return mp_const_none;
}
// 2. Bind the C function to a MicroPython function object
static MP_DEFINE_CONST_FUN_OBJ_0(malo_init_core1_obj, malo_init_core1);

// 3. Map the Python function name string to the object
static const mp_rom_map_elem_t malo_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_malo) },
    { MP_ROM_QSTR(MP_QSTR_init_core1), MP_ROM_PTR(&malo_init_core1_obj) },
};
static MP_DEFINE_CONST_DICT(malo_module_globals, malo_module_globals_table);

// 4. Create the module structure
const mp_obj_module_t malo_user_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&malo_module_globals,
};

// 5. LOCK IN THE NAME: "import malo"
MP_REGISTER_MODULE(MP_QSTR_malo, malo_user_module);

