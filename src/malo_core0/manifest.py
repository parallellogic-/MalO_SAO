# Include the default board manifest so standard features work
include("$(BOARD_DIR)/manifest.py")

# Freeze all .py files and subfolders inside your core0 folder
freeze(".")
