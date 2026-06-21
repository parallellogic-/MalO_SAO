import lvgl as lv
import os
import struct

# 1. Initialize the LVGL core engine
lv.init()

# 2. VIRTUAL DISPLAY REPAIR: Setup a virtual display so lv.screen_active() works
WIDTH = 128
HEIGHT = 128
disp = lv.display_create(WIDTH, HEIGHT) # Establishes default active system layer

# 3. LVGL 9 CANVAS ALLOCATION: Use L8 (8-bit grayscale/luminance format)
# This forces LVGL to layout memory natively as 1-byte-per-pixel grayscale
draw_buf = lv.draw_buf_create(WIDTH, HEIGHT, lv.COLOR_FORMAT.L8, lv.STRIDE_AUTO)

# Create the canvas using the now valid active screen context
canvas = lv.canvas(lv.screen_active())
canvas.set_draw_buf(draw_buf)
canvas.center()

# Fill the entire canvas background with Mid-Gray (0x80)
canvas.fill_bg(lv.color_hex(0x808080), lv.OPA.COVER) 

# 4. DRAW THE RECTANGLE USING V9 LAYER MECHANICS
layer = lv.layer_t()
canvas.init_layer(layer)

dsc = lv.draw_rect_dsc_t()
dsc.init()
dsc.bg_opa = lv.OPA.TRANSP                  # No background color fill
dsc.border_color = lv.color_hex(0x000000)   # Pure Black (0x00) Border
dsc.border_width = 4                        # 4 pixels thick

# Set center rectangle coordinates (64x64 box centered inside 128x128 space)
rect_area = lv.area_t()
rect_area.x1 = 32
rect_area.y1 = 32
rect_area.x2 = 95
rect_area.y2 = 95

# Render shape inside the matrix layer and commit changes
lv.draw_rect(layer, dsc, rect_area)
canvas.finish_layer(layer)

# 5. EXTRACTION: Safely copy the 8-bit grayscale pixel stream
TOTAL_BYTES = WIDTH * HEIGHT
img_desc = canvas.get_image()
c_array_data = img_desc.data

raw_bytes = bytearray(TOTAL_BYTES)
for i in range(TOTAL_BYTES):
    raw_bytes[i] = c_array_data[i]

# 6. WRITE RAW GRAYSCALE FILE DIRECTLY TO FLASH FILE SYSTEM
output_path = "rect_128x128.gray"
print("Writing raw grayscale asset to flash...")
with open(output_path, "wb") as f:
    f.write(raw_bytes)

print("Done! Image successfully saved as:", output_path)
file_size = os.stat(output_path)[6]
print(f"File Size verified on flash: {file_size} bytes")


# ==============================================================================
# 7. NEW EXTENSION: GENERATE AND SAVE A STANDARD 8-BIT GRAYSCALE BMP FILE
# ==============================================================================
bmp_path = "rect_128x128.bmp"
print("\nGenerating standard 8-bit Grayscale BMP asset...")

# A: Build the 54-byte BMP Header structures
# Headers consist of: BMP File Header (14 bytes) + DIB Info Header (40 bytes)
header_offset = 54 + 1024  # 54 bytes headers + 1024 bytes color palette = 1078
total_file_size = header_offset + TOTAL_BYTES

bmp_header = struct.pack(
    "<2sIHHI_I_IIHHIIIIII".replace("_", ""), # Enforces all uppercase 'I' and strips spaces
    b'BM',             # Signature (2 bytes)
    total_file_size,   # Total file size in bytes
    0, 0,              # Reserved fields
    header_offset,     # Absolute pixel data start offset address
    
    40,                # Size of DIB header structure (40 bytes)
    WIDTH,             # Image width in pixels
    HEIGHT,            # Image height in pixels
    1,                 # Number of color planes (Must be 1)
    8,                 # Bit depth (8 bits per pixel)
    0,                 # Compression method (0 = Uncompressed BI_RGB)
    TOTAL_BYTES,       # Size of raw image payload buffer space
    2835, 2835,        # Horizontal and Vertical resolution markers (72 DPI)
    256,               # Total color palette entry indices used (256 colors)
    256                # Total important color palette entry counts
)

# B: Build the 1,024-byte Grayscale Color Palette (Color Lookup Table)
# An 8-bit BMP requires 256 colors defined sequentially as 4-byte [B, G, R, Alpha] segments
bmp_palette = bytearray(1024)
for i in range(256):
    base = i * 4
    bmp_palette[base]     = i  # Blue channel
    bmp_palette[base + 1] = i  # Green channel
    bmp_palette[base + 2] = i  # Red channel
    bmp_palette[base + 3] = 0  # Alpha channel / Reserved padding byte

# C: Stream structural headers and flipped pixel layout to flash memory
with open(bmp_path, "wb") as f:
    f.write(bmp_header)   # Write 54-byte configuration blocks
    f.write(bmp_palette)  # Write 1024-byte gradient color lookup arrays
    
    # BMP pixels are stored upside-down (bottom row up to top row)
    # Loop backward row-by-row to format the file without breaking canvas memory layout
    for row in range(HEIGHT - 1, -1, -1):
        start_idx = row * WIDTH
        end_idx = start_idx + WIDTH
        f.write(raw_bytes[start_idx:end_idx])

print("Done! BMP image successfully saved as:", bmp_path)
bmp_file_size = os.stat(bmp_path)[6]
print(f"File Size verified on flash: {bmp_file_size} bytes (Expects exactly 17462 bytes)")


