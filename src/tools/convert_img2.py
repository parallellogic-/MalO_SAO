import sys
import struct
from PIL import Image

# LVGL v9 Specific Enums
LV_IMAGE_HEADER_MAGIC = 0x19  # Magic identifier for v9
LV_COLOR_FORMAT_L8 = 0x0A     # Enum index for L8 luminance format

def png_to_lvgl_v9_bin(image_path, output_bin_path):
    # 1. Load image and force convert to 8-bit grayscale
    img = Image.open(image_path).convert('L')
    width, height = img.size
    
    if width > 2048 or height > 2048:
        print("Warning: LVGL image size natively limits width/height to 2048px max.")
    
    # 2. Extract raw uncompressed pixel data bytes
    pixel_bytes = bytes(img.getdata())
    
    # 3. For L8 format, stride is exactly equal to the image width in bytes
    stride = width 
    flags = 0
    reserved = 0
    
    # 4. Pack exactly 12 bytes using Little-Endian order (<)
    # B = 1 byte, H = 2 bytes unsigned short
    # Layout: magic(1B), cf(1B), flags(2B), width(2B), height(2B), stride(2B), reserved(2B)
    header = struct.pack(
        "<BBHHHHH", 
        LV_IMAGE_HEADER_MAGIC, 
        LV_COLOR_FORMAT_L8, 
        flags, 
        width, 
        height, 
        stride, 
        reserved
    )
    
    # 5. Write out the binary file
    with open(output_bin_path, "wb") as f:
        f.write(header)
        f.write(pixel_bytes)
        
    print(f"Success! Generated file: {output_bin_path}")
    print(f"Dimensions: {width}x{height} (Stride: {stride})")
    print(f"Total Disk Size: {len(header) + len(pixel_bytes)} bytes")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python script.py <input.png> <output.bin>")
    else:
        png_to_lvgl_v9_bin(sys.argv[1], sys.argv[2])

