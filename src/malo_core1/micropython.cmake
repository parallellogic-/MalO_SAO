# Create an INTERFACE library for our C module.
# We name the library 'usermod_malo_module'
add_library(usermod_malo_module INTERFACE)

# Add our source files to the lib
target_sources(usermod_malo_module INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/core1_api.c
    ${CMAKE_CURRENT_LIST_DIR}/core1_main.c
    #${CMAKE_CURRENT_LIST_DIR}/led.c
)

# Add the current directory as an include directory.
target_include_directories(usermod_malo_module INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Link our INTERFACE library to the main 'usermod' target provided by MicroPython.
target_link_libraries(usermod INTERFACE usermod_malo_module)

list(APPEND MICROPY_BOARD_CFLAGS -DMODULE_malo_MODULE_ENABLED=1)
