# Include standard MicroPython modules (like asyncio and aioble)
include("$(PORT_DIR)/boards/manifest.py")

# Freeze all your custom Python scripts from core0 into the root namespace
#freeze("/mnt/Data/Projects/malo_sao/MalO_SAO/src/malo_core0") #doesn't work, makes files invisible to user in Thonny IDE
