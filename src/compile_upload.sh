#!/bin/bash

# 1. Save the directory where the user called this script from
START_DIR=$(pwd)

# 2. Derive the project root relative to this script's location
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

echo "Starting build process..."
echo "Project root: $PROJECT_ROOT"

SKIP_COMPILE=0
if [ "$1" = "--skip-compile" ]; then
    SKIP_COMPILE=1
    echo "Flag detected: Skipping compilation stage. Proceeding directly to deployment."
fi

if [ ! -f "$PROJECT_ROOT/src/build/build-MALO_BOARD/firmware.uf2" ]; then
	echo "Error: Cannot skip compilation because no existing firmware.uf2 was found at:"
	echo "$PROJECT_ROOT/src/build/build-MALO_BOARD/firmware.uf2"
    SKIP_COMPILE=0
fi

# Track whether we can proceed to flash/deploy
PROCEED_TO_DEPLOY=0

if [ $SKIP_COMPILE -eq 0 ]; then

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

	BUILD_STATUS=$?
	if [ $BUILD_STATUS -eq 0 ]; then
        	PROCEED_TO_DEPLOY=1
    	else
        	echo "Build failed."
    	fi
else
    # Verify the compiled binary actually exists before attempting to flash it
    if [ -f "$PROJECT_ROOT/src/build/build-MALO_BOARD/firmware.uf2" ]; then
        PROCEED_TO_DEPLOY=1
    else
        echo "Error: Cannot skip compilation because no existing firmware.uf2 was found at:"
        echo "$PROJECT_ROOT/src/build/build-MALO_BOARD/firmware.uf2"
    fi
fi

# 6. Copy the firmware and sync Python files if the build succeeded
if [ $PROCEED_TO_DEPLOY -eq 1 ]; then
#if [ $BUILD_STATUS -eq 0 ]; then
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

