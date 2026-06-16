# 1. Create the module library
add_library(usermod_malo_module INTERFACE)

# 2. Add your source implementation files
target_sources(usermod_malo_module INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/core1_api.c
    ${CMAKE_CURRENT_LIST_DIR}/core1_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/led.cpp
)

# 3. Handle include track path configurations
target_include_directories(usermod_malo_module INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# 4. Enforce standard C++ compilation standard
target_compile_features(usermod_malo_module INTERFACE cxx_std_11)

# 5. Connect cleanly to the usermod collector target
target_link_libraries(usermod INTERFACE usermod_malo_module)

# 6. Global activation flag definition
list(APPEND MICROPY_BOARD_CFLAGS -DMODULE_malo_MODULE_ENABLED=1)

