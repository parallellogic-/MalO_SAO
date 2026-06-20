#!/bin/bash

# 1. Save the directory where the user called this script from
START_DIR=$(pwd)

# 2. Derive the project root relative to this script's location
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

echo "Starting build process..."
echo "Project root: $PROJECT_ROOT"

# 3. Clean up the old build directory from the project root baseline
TARGET_BUILD_DIR="$PROJECT_ROOT/src/build/build-MALO_BOARD"
rm -rf "$TARGET_BUILD_DIR"
mkdir -p "$TARGET_BUILD_DIR"

# 4. Navigate into the MicroPython port directory (patch to add in lvgl)
cd "$PROJECT_ROOT/lib/micropython/ports/rp2/" || exit 1

# Initialize missing build dependencies seamlessly without altering tracked code files
echo "Ensuring MicroPython build submodules are initialized..."
(cd ../.. && git submodule update --init --recursive)

# === INSERTED STEP: Fetch submodules specifically for MALO_BOARD ===
echo "Initializing target board submodules..."
make -C . BOARD=MALO_BOARD BOARD_DIR=../../../../src/boards/MALO_BOARD picotool_DIR="$PROJECT_ROOT/lib/picotool/dist" submodules

echo "Building mpy-cross companion engine..."
make -C ../../mpy-cross BUILD="$TARGET_BUILD_DIR/mpy-cross-host"

# 5. Clean and run the build with paths relative to the current port directory
make clean && \
picotool_DIR="$PROJECT_ROOT/lib/picotool/dist" \
make -j \
BOARD=MALO_BOARD \
BOARD_DIR=../../../../src/boards/MALO_BOARD \
BUILD=../../../../src/build/build-MALO_BOARD \
USER_C_MODULES="$PROJECT_ROOT/lib/lv_micropython/user_modules/lv_binding_micropython/bindings.cmake\\;$PROJECT_ROOT/src/malo_core1/micropython.cmake" \
EXTRA_CMAKE_ARGS="-DPICO_BOARD_CMAKE_DIRS=$PROJECT_ROOT/src/boards/MALO_BOARD -DPICO_BOARD_HEADER_DIRS=$PROJECT_ROOT/src/boards/MALO_BOARD"
#BOARD=MALO_BOARD \
#BOARD_DIR=../../../../src/boards/MALO_BOARD \
#BOARD=RPI_PICO2 \
#BOARD_DIR=/mnt/Data/Projects/malo_sao/MalO_SAO/lib/micropython/ports/rp2/boards/RPI_PICO2 \
#USER_C_MODULES=../../../../src/malo_core1/micropython.cmake \
#USER_C_MODULES="$PROJECT_ROOT/lib/lv_micropython/user_modules/lv_binding_micropython/bindings.cmake\\;$PROJECT_ROOT/src/malo_core1/micropython.cmake" \

BUILD_STATUS=$?

# 6. Copy the firmware and sync Python files if the build succeeded
if [ $BUILD_STATUS -eq 0 ]; then
    echo "Searching for RP2350 / Pico bootloader drive..."
    
    # Dynamically locate the mount point (matches RP2350, RP2, RPI-RP2, etc. under /media/ or /run/media/)
    DEST_DIR=$(find /media/ /run/media/ -maxdepth 3 -type d \( -name "*RP2350*" -o -name "*RP2*" -o -name "*RPI*" \) 2>/dev/null | head -n 1)

    if [ -n "$DEST_DIR" ] && [ -d "$DEST_DIR" ]; then
        echo "Found storage at: $DEST_DIR"
        echo "Copying firmware.uf2..."
        cp "$PROJECT_ROOT/src/build/build-MALO_BOARD/firmware.uf2" "$DEST_DIR/"
        echo "Done! Flash complete."
        
        # Give the device a brief moment to boot up and initialize the filesystem before mpremote connects
        echo "Waiting for MicroPython storage to mount..."
        sleep 3
        
        # Verify a valid MicroPython device is connected over USB serial before executing commands
        echo "Testing connection to device..."
        if ! timeout 5 mpremote connect list > /dev/null 2>&1; then
            echo "Error: Device flashed successfully but failed to boot into MicroPython REPL."
            echo "This usually indicates a boot loop, invalid board configuration, or missing dependency."
            cd "$START_DIR"
            exit 1
        fi
        
        echo "Clearing existing files on device (excluding boot.py)..."
        mpremote exec "import os; [os.remove(f) for f in os.listdir('/') if f not in ('boot.py',)]"
        
        echo "Uploading new application files..."
        (cd "$PROJECT_ROOT/src/malo_core0" && mpremote fs cp -r . :)
        echo "Deployment successful!"
        
        # Reboot the MicroPython runtime to start executing your fresh files
        echo "Resetting target device..."
        mpremote reset
        echo "System is running!"
    else
        echo "Build succeeded, but no connected RP2350/Pico bootloader drive was found."
        echo "Firmware saved locally at: $PROJECT_ROOT/src/build/build-MALO_BOARD/firmware.uf2"
    fi
else
    echo "Build failed."
fi

# 7. Safely return the user back to their original terminal location
cd "$START_DIR"

