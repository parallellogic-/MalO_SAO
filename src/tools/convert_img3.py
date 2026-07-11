import os
import sys
from PIL import Image

def process_image(filepath):
    """
    Resizes image to exactly 128x128, converts to 8-bit grayscale, 
    and saves as raw uint8_t bytes with a replaced .cmp extension.
    """
    try:
        with Image.open(filepath) as img:
            # Convert to 8-bit grayscale ('L' mode = 1 byte per pixel)
            gray_img = img.convert('L')
            
            # Force resize to exactly 128x128 to guarantee a 16,384-byte array
            # Uses Resampling.LANCZOS for clean downscaling
            resized_img = gray_img.resize((128, 128), Image.Resampling.LANCZOS)
            
            # Extract raw byte data (uint8_t array)
            raw_bytes = resized_img.tobytes()
            
            # Replace the extension (e.g., photo.jpg -> photo.cmp)
            base_path, _ = os.path.splitext(filepath)
            output_path = f"{base_path}.cmp"
            
            # Write raw binary data directly to disk
            with open(output_path, 'wb') as f:
                f.write(raw_bytes)
                
            print(f"Successfully processed: {filepath} -> {output_path} ({len(raw_bytes)} bytes)")
            
    except Exception as e:
        print(f"Error processing {filepath}: {e}")

def main():
    # Supported image extensions
    valid_extensions = ('.png', '.jpg', '.jpeg', '.bmp', '.webp', '.tiff')
    
    # Scenario 1: A file argument was passed via command line
    if len(sys.argv) > 1:
        target_file = sys.argv[1]
        if os.path.isfile(target_file):
            process_image(target_file)
        else:
            print(f"Error: '{target_file}' is not a valid file.")
            
    # Scenario 2: No arguments passed, scan the current working directory
    else:
        print("No target file specified. Scanning current directory for images...")
        try:
            files = os.listdir('.')
            image_files = [f for f in files if f.lower().endswith(valid_extensions)]
            
            if not image_files:
                print("No matching image files found in this directory.")
                return
                
            for file in image_files:
                process_image(file)
                
        except Exception as e:
            print(f"Failed to scan directory: {e}")

if __name__ == '__main__':
    main()

