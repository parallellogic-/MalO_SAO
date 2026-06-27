#include <Adafruit_TinyUSB.h>
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <Arduino.h>
#include "dma_control_block.h"
#include "screen.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <SdFat.h>

#define SPI1_CS 9
#define SPI1_DC 8
#define SPI1_MOSI 11
#define SPI1_SCLK 10
#define SPI1_BAUD 8'000'000

#define USB_BLOCK_SIZE    512
//#define FLASH_SECTOR_SIZE 4096

Adafruit_USBD_MSC usb_msc;
Screen screen(spi1,SPI1_BAUD,SPI1_DC);
ScatterGatherEngine scatterer_gatherer_engine_screen;

// Custom 16MB hardware layout boundaries
const uint32_t FLASH_TARGET_OFFSET = 2 * 1024 * 1024; 
const uint32_t DISK_SIZE_BYTES     = 14 * 1024 * 1024; 

// RAM cache staging layouts
static uint8_t sector_cache[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));
static int32_t cached_sector_id = -1;
static bool cache_is_dirty = false;

// ====================================================================
// FORWARD DECLARATIONS (CRITICAL FIX FOR SCOPING ERRORS)
// ====================================================================
int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize);
int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize);
void msc_flush_cb(void);
// ====================================================================

// Inherit from FsBlockDevice to perfectly match the RP2040/RP2350 core config
class RP2350CustomFlashDriver : public FsBlockDevice {
public:
    // Core SdFat v2 uses readSector & writeSector with an optional uint32_t count parameter
    bool readSector(uint32_t sector, uint8_t* dst) override {
        return msc_read_cb(sector, dst, 512) == 512;
    }

    bool writeSector(uint32_t sector, const uint8_t* src) override {
        return msc_write_cb(sector, (uint8_t*)src, 512) == 512;
    }

    bool readSectors(uint32_t sector, uint8_t* dst, size_t count) override {
        return msc_read_cb(sector, dst, count * 512) == (int32_t)(count * 512);
    }

    bool writeSectors(uint32_t sector, const uint8_t* src, size_t count) override {
        return msc_write_cb(sector, (uint8_t*)src, count * 512) == (int32_t)(count * 512);
    }

    bool syncDevice() override {
        msc_flush_cb();
        return true;
    }

    // Required pure virtual functions for FsBlockDevice in this core configuration
    bool isBusy() override { return false; }
    uint32_t sectorCount() override { return DISK_SIZE_BYTES / USB_BLOCK_SIZE; }
};

// Instantiate the file system blocks using core-compliant Types
static RP2350CustomFlashDriver hardware_block_driver;
static FatVolume fat_fs; 
//static FsFile sprite_sheet_file;  // Changed 'File' to 'FsFile' to resolve name conflicts
static File32 sprite_sheet_file;

//static uint8_t sprite_buffer[128 * 128];
static lv_image_dsc_t sprite_img_dsc;



// CRITICAL FIX: __no_inline_not_in_flash_func forces this code to run purely out of RAM 
// This allows safe writing to the flash while the XIP cache mapping engine is disabled.
void __no_inline_not_in_flash_func(flush_sector_cache)() {
  // Use one of the RP2350's hardware spinlock registers (Lock ID 31 is typically safe/free)
  uint32_t spin_status = spin_lock_blocking(spin_lock_instance(31));

  // Double-check variables inside the protected gateway
  if (cached_sector_id == -1 || !cache_is_dirty) {
    spin_unlock(spin_lock_instance(31), spin_status);
    return;
  }

  uint32_t sector_start = cached_sector_id * FLASH_SECTOR_SIZE;
  uint32_t physical_flash_addr = FLASH_TARGET_OFFSET + sector_start;
  
  Serial.print("[FLASH RUNTIME] Erasing & Writing Sector ID: ");
  Serial.print(cached_sector_id);
  Serial.print(" at Real Addr: 0x");
  Serial.println(physical_flash_addr, HEX);

  // Turn off internal core interrupts completely during the physical write block window
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(physical_flash_addr, FLASH_SECTOR_SIZE);
  flash_range_program(physical_flash_addr, sector_cache, FLASH_SECTOR_SIZE);
  restore_interrupts(ints);

  uint32_t verification_addr = XIP_BASE + physical_flash_addr;
  Serial.print("[FLASH VERIFY] First byte in memory window: 0x");
  Serial.println(*(uint8_t*)verification_addr, HEX);

  cache_is_dirty = false;

  // Release the hardware gate so the other core/thread can safely interact with flash again
  spin_unlock(spin_lock_instance(31), spin_status);
}


int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
  uint32_t drive_offset = (lba * USB_BLOCK_SIZE);
  uint32_t target_sector_id = drive_offset / FLASH_SECTOR_SIZE;
  uint32_t block_offset_in_sector = drive_offset % FLASH_SECTOR_SIZE;

  if (target_sector_id == cached_sector_id) {
    memcpy(buffer, sector_cache + block_offset_in_sector, bufsize);
  } else {
    uint32_t flash_addr = XIP_BASE + FLASH_TARGET_OFFSET + drive_offset;
    memcpy(buffer, (const void*)flash_addr, bufsize);
  }
  return bufsize;
}

int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  uint32_t drive_offset = (lba * USB_BLOCK_SIZE);
  uint32_t target_sector_id = drive_offset / FLASH_SECTOR_SIZE;
  uint32_t block_offset_in_sector = drive_offset % FLASH_SECTOR_SIZE;

  if (target_sector_id != cached_sector_id) {
    // Commit the previous sector out of the RAM pipeline before reallocating layout space
    if (cached_sector_id != -1 && cache_is_dirty) {
       flush_sector_cache();
    }
    
    cached_sector_id = target_sector_id;
    uint32_t sector_start = cached_sector_id * FLASH_SECTOR_SIZE;
    uint32_t physical_flash_read_addr = XIP_BASE + FLASH_TARGET_OFFSET + sector_start;
    
    // Read the unmodified structure layout securely into RAM
    uint32_t ints = save_and_disable_interrupts();
    memcpy(sector_cache, (const void*)physical_flash_read_addr, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
  }

  // Inject the new bytes coming from Linux straight into the active RAM matrix
  memcpy(sector_cache + block_offset_in_sector, buffer, bufsize);
  cache_is_dirty = true;

  return bufsize;
}

void msc_flush_cb(void) {
  flush_sector_cache();
}

bool msc_ready_cb(void) {
  return true; 
}

void debug_raw_sector_check() {
    Serial.println("\n--- RAW HARDWARE FLASH INTEGRITY CHECK ---");
    uint8_t temp_sector[512];
    
    // Read the very first sector of your virtual drive (Sector 0)
    if (msc_read_cb(0, temp_sector, 512) == 512) {
        Serial.print("Sector 0 Signature Check (Should be 0x55, 0xAA): 0x");
        Serial.print(temp_sector[510], HEX);
        Serial.print(", 0x");
        Serial.println(temp_sector[511], HEX);
        
        // Print the first 16 bytes to look for FAT markers like "MSDOS", "FAT", or "FAT32"
        Serial.print("Raw Header Label String: ");
        for(int i = 3; i < 11; i++) {
            if(temp_sector[i] >= 32 && temp_sector[i] <= 126) Serial.print((char)temp_sector[i]);
            else Serial.print(".");
        }
        Serial.println();
    } else {
        Serial.println("CRITICAL ERROR: Failed to execute hardware read command over sector 0!");
    }
    Serial.println("------------------------------------------\n");
}

#define SCREEN_HEIGHT 128
#define SCREEN_WIDTH 128
static uint8_t canvas_buffer[SCREEN_WIDTH * SCREEN_HEIGHT] __attribute__((aligned(4)));
// 2. Allocate your packed display output buffer
// Two 4-bit pixels pack into one byte: (128 * 128) / 2 = 8,192 Bytes
//static uint8_t packed_display_buffer[(SCREEN_WIDTH * SCREEN_HEIGHT) / 2] __attribute__((aligned(4)));
// 2. LVGL V9 FIX: Pre-allocate the management structure header inside static memory
// This completely bypasses dynamic malloc requirements inside the canvas setup
static lv_draw_buf_t custom_canvas_draw_handle;
void dummy_display_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    lv_display_flush_ready(disp); // Keep the pipeline cycling
}

// Custom function to process the canvas buffer, pack upper nibbles, and transmit
void process_and_flush_canvas() {
    uint32_t packed_idx = 0;
    
    uint8_t* tx_buffer=screen.get_frame_buffer();
    for (int32_t y = 0; y < SCREEN_HEIGHT; y++) {
        for (int32_t x = 0; x < SCREEN_WIDTH; x += 2) {
            
            // --- 90-DEGREE CCW COORDINATE TRANSLATION ---
            // Formula for 90 CCW: New_X = Old_Y, New_Y = (Width - 1) - Old_X
            
            // Calculate source coordinates for the Left output pixel (at column x)
            int32_t src_x_left = (SCREEN_WIDTH - 1) - y;
            int32_t src_y_left = x;
            uint32_t pixel_left_idx = (src_y_left * SCREEN_WIDTH) + src_x_left;

            // Calculate source coordinates for the Right output pixel (at column x + 1)
            int32_t src_x_right = (SCREEN_WIDTH - 1) - y;
            int32_t src_y_right = x + 1;
            uint32_t pixel_right_idx = (src_y_right * SCREEN_WIDTH) + src_x_right;

            // Extract the high-frequency luminosity bits (upper nibbles)
            uint8_t left_nibble  = canvas_buffer[pixel_left_idx]  & 0xF0;
            uint8_t right_nibble = canvas_buffer[pixel_right_idx] & 0xF0;

            // Pack them perfectly: Left pixel high bits, Right pixel low bits
            //packed_display_buffer[packed_idx++] = left_nibble | (right_nibble >> 4);
            tx_buffer[packed_idx++] = left_nibble | (right_nibble >> 4);
        }
    }
    
    // Transmit the fully optimized 4bpp block directly to your display controller
    //ms_screen.flush(packed_display_buffer, sizeof(packed_display_buffer));
    screen.flush();
}

void graphics_init() {
    
    Serial.println("graphics_init");
    // Initialize the LVGL core framework
    lv_init();

    // ====================================================================
    // CRITICAL CORE FIX: Instantiate a Minimal Virtual Display Driver
    // This provides the fallback environment required for widgets to validate layout shifts
    Serial.println("Registering virtual fallback display...");
    lv_display_t* dummy_disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_color_format(dummy_disp, LV_COLOR_FORMAT_L8);
    lv_display_set_flush_cb(dummy_disp, dummy_display_flush_cb);
    // -----------

    Serial.println("lv_canvas_create");
    // 3. Instantiate the LVGL Canvas UI object
    lv_obj_t* canvas = lv_canvas_create(lv_screen_active());
    

    //Serial.println("lv_canvas_set_buffer");
    // Assign our raw 8bpp RAM buffer and dimensions to the canvas object
    //lv_canvas_set_buffer(canvas, canvas_buffer, SCREEN_WIDTH, SCREEN_HEIGHT, LV_COLOR_FORMAT_L8);
        
    Serial.println("Manually configuring draw buffer structure...");
    // 3. Directly populate the properties of our static draw handle
    // Format options: Handle, Width, Height, Color Format, Stride (Width * BytesPerPixel)
    lv_draw_buf_init(
        &custom_canvas_draw_handle, 
        SCREEN_WIDTH, 
        SCREEN_HEIGHT, 
        LV_COLOR_FORMAT_L8, 
        SCREEN_WIDTH, 
        canvas_buffer, 
        sizeof(canvas_buffer)
    );

    Serial.println("lv_canvas_set_draw_buf 2");
    // 4. Bind our static handle directly to the Canvas UI element
    // This function sets properties directly and does not call malloc()
    lv_canvas_set_draw_buf(canvas, &custom_canvas_draw_handle);

    Serial.println("Success! Canvas attached safely without malloc.");


    Serial.println("lv_obj_center");
    lv_obj_center(canvas);

    Serial.println("lv_canvas_fill_bg");
    // 4. Fill the background of the canvas with a baseline color value (e.g., 0x30)
    lv_canvas_fill_bg(canvas, lv_color_hex(0x333333), LV_OPA_COVER);

    // 5. Configure drawing styles for your rectangle
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    
    // Map colors to the 8-bit index space
    rect_dsc.bg_color = lv_color_hex(0xCCCCCC); // Target bright pixels (~0xCC grayscale value)
    rect_dsc.bg_opa = LV_OPA_COVER;
    
    // Define an optional border outline for the rectangle
    rect_dsc.border_color = lv_color_hex(0xFFFFFF); // White outline border (~0xFF grayscale value)
    rect_dsc.border_width = 2;

    Serial.println("lv_canvas_init_layer");
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    // 6. Execute the geometric vector coordinate draw operation onto the canvas
    // Draws a centered 64x64 rectangle inside our 128x128 bounding window
    lv_area_t coords_rect = {32, 32, 32 + 64 - 1, 32 + 64 - 1};

    // Execute the actual universal draw call onto your canvas layer context
    lv_draw_rect(&layer, &rect_dsc, &coords_rect);
    // Close and flush the canvas rendering context block back down to canvas_buffer
    
    // ----------------------------------------------------------------
    // STEP B: DRAW THE TEXT ON TOP OF THE RECTANGLE
    // ----------------------------------------------------------------
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    
    // Configure text appearance parameters
    label_dsc.text = "MALO3";                    // The text string to display
    label_dsc.color = lv_color_hex(000000);      // High luminosity white text
    label_dsc.font = LV_FONT_DEFAULT;              // Fallback to built-in system font
    label_dsc.align = LV_TEXT_ALIGN_CENTER;        // Horizontal alignment math

    // Define the bounding coordinate box for the text placement
    // Placing it squarely inside the boundaries of our background rectangle
    lv_area_t coords_text = {20, 52, 108, 76};
    lv_draw_label(&layer, &label_dsc, &coords_text);
    //---------- end step B

    //---------- step c draw image palceholder here...

    
    // Choose file: Use "film.cmp" (Row 0, Col 0) or "film2.cmp" (Row 0, Col 0)
    // To grab the first 128x128 pixels of film2.cmp, pass row 0 and column 0.
    const char* target_file = "film4.cmp"; 
    int target_row = 0;
    int target_col = 0;


    Serial.printf("start_load: %d\n",time_us_64());
    if (load_sprite_to_lvgl_dsc2(target_file, target_row, target_col)) {
        Serial.printf("end_load: %d\n",time_us_64());
        lv_draw_image_dsc_t img_draw_dsc;
        lv_draw_image_dsc_init(&img_draw_dsc);
        img_draw_dsc.src = &sprite_img_dsc; // Pass our populated descriptor
        img_draw_dsc.opa = LV_OPA_COVER;

        // Area boundaries inside your canvas display context window
        lv_area_t coords_img = {0, 0, 127, 127};

        Serial.println("Drawing sprite layer via LVGL draw pipeline...");
        Serial.printf("start_draw: %d\n",time_us_64());
        lv_draw_image(&layer, &img_draw_dsc, &coords_img);
        Serial.printf("end_draw: %d\n",time_us_64());
    } else {
        Serial.println("Image loading failed.");
    }
    //end step c------------------

    lv_canvas_finish_layer(canvas, &layer);

    Serial.println("lv_refr_now");
    // Force an internal update pass so changes are immediately rasterized into canvas_buffer
    lv_refr_now(NULL);

    // 7. Extract the data layers, pack the nibbles, and flash the screen
    process_and_flush_canvas();
}

void setup() {
    pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  delay(20);  //AN4650 needed for reboot time of IMU --> later, put as part of boot-up sequence routine

  Serial.begin(1000000); 
  long start_tms=millis();


  usb_msc.setID("RP2350B", "Flash Drive", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb); 
  usb_msc.setReadyCallback(msc_ready_cb);
  usb_msc.setCapacity(DISK_SIZE_BYTES / USB_BLOCK_SIZE, USB_BLOCK_SIZE);
  usb_msc.setUnitReady(true);
  
  usb_msc.begin();
  while(!Serial && (millis()-start_tms)<7000); //messes up usb_msc is before usb_msc.begin
  //delay(1000);

  Serial.println("Init SPI OLED Screen...");
  //temp_spi_chan = dma_claim_unused_channel(true);
  spi_init(spi1, SPI1_BAUD);
  gpio_set_function(SPI1_SCLK, GPIO_FUNC_SPI);
  gpio_set_function(SPI1_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(SPI1_CS,   GPIO_FUNC_SPI);
  gpio_init(SPI1_DC);//is needed for proper screen operation
  gpio_set_dir(SPI1_DC, GPIO_OUT);
  gpio_put(SPI1_DC,HIGH);

  Serial.println("Init Screen...");
  screen.begin();
  scatterer_gatherer_engine_screen.registerSource(&screen);//50mA@5V

  Serial.println("Init Scatterer Gatherer...");
  scatterer_gatherer_engine_screen.begin(false); //limit to only 2 channels for screen

  //delay(1000); // Allow system registers to settle safely

  // Mount the FAT library safely over your custom driver logic
  Serial.println("Mounting FatVolume library framework layer...");
  if (!fat_fs.begin(&hardware_block_driver, true, 0)) {
      Serial.println("CRITICAL ERROR: SdFat failed to mount your internal drive partition layout!");
  } else {
      Serial.println("SdFat File System successfully initialized!");
  }

  graphics_init(); //PRECON: must be after fat_fs.begin 
}


void demo_print_filesystem2() {
    Serial.println("\n=====================================");
    Serial.println("   SDFAT: PRINTING FILE SYSTEM LIST  ");
    Serial.println("=====================================");

    uint8_t flags = LS_R | LS_SIZE;
    fat_fs.ls(&Serial, flags);

    Serial.println("=====================================");
    Serial.println("   SDFAT: PEEKING INSIDE FILM.CMP    ");
    Serial.println("=====================================");

    File32 test_file;
    
    // Test 1: Try opening with lowercase string
    test_file = fat_fs.open("film.cmp", O_RDONLY);
    
    // Test 2: If lowercase fails, immediately try uppercase short filename format
    if (!test_file) {
        Serial.println("[PEEK LOG] Lowercase 'film.cmp' failed. Trying uppercase 'FILM.CMP'...");
        test_file = fat_fs.open("FILM.CMP", O_RDONLY);
    }

    if (!test_file) {
        Serial.println("CRITICAL ERROR: Filesystem found the file in ls(), but open() refused the handle.");
        Serial.println("=====================================\n");
        return;
    }

    Serial.println("SUCCESS! film.cmp opened cleanly.");
    Serial.print("File Size Verified: ");
    Serial.print(test_file.size());
    Serial.println(" bytes.");

    // Read and dump the first 16 bytes (the file header)
    uint8_t header_buffer[16];
    int bytes_read = test_file.read(header_buffer, 16);
    
    Serial.print("First 16 Bytes (HEX): ");
    for (int i = 0; i < bytes_read; i++) {
        if (header_buffer[i] < 0x10) Serial.print("0"); // Pad single digits
        Serial.print(header_buffer[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // Print as characters if they are human-readable ASCII bytes
    Serial.print("First 16 Bytes (ASCII): ");
    for (int i = 0; i < bytes_read; i++) {
        if (header_buffer[i] >= 32 && header_buffer[i] <= 126) {
            Serial.print((char)header_buffer[i]);
        } else {
            Serial.print("."); // Placeholder for non-printable binary pixels
        }
    }
    Serial.println();

    // Clean up and close the file
    test_file.close();
    Serial.println("=====================================\n");
}

// 128 * 128 bytes = 16,384 bytes
static uint8_t lvgl_sprite_buffer[128 * 128];
//static lv_image_dsc_t sprite_img_dsc;

bool load_sprite_to_lvgl_dsc(const char* filename, int row, int col) {
    fat_fs.chvol(); 
    
    File32 local_file;
    int retry_count = 0;
    const int max_retries = 5;

    // GATEWAY RETRY LOOP: Fight USB block collisions safely
    while (retry_count < max_retries) {
        local_file = fat_fs.open(filename, O_RDONLY);
        if (local_file) break; // Success! Break out of the loop

        retry_count++;
        Serial.print("[RETRY LOG] USB sector contention detected. Retrying open count: ");
        Serial.println(retry_count);
        delay(5); // Give the background USB engine 5ms to clear its lock
    }

    if (!local_file) {
        Serial.print("SdFat CRITICAL ERROR: File handle refused after maximum retries: ");
        Serial.println(filename);
        return false;
    }

    // Default configuration metrics for film.cmp (12 columns of 128x128 sprites)
    int sheet_cols = 12;
    int sprite_w = 128;
    int sprite_h = 128;

    // Detect if we are parsing film2.cmp (256x256 total pixels)
    // A 256x256 sheet contains exactly 2 columns and 2 rows of 128x128 sprites
    if (strstr(filename, "film2") != nullptr) {
        sheet_cols = 2; 
    }

    const int sheet_pitch = sheet_cols * sprite_w; // Total byte width of one pixel line
    const uint32_t file_header_offset = 12;        // Verified 12-byte metadata skip

    int start_x = col * sprite_w;
    int start_y = row * sprite_h;

    // Extract exactly a 128x128 footprint box out of the file
    Serial.printf("A start: %d\n",time_us_64());
    for (int y = 0; y < 128; y++) {
        uint32_t file_offset = file_header_offset + (((start_y + y) * sheet_pitch) + start_x);
        
        if (!local_file.seek(file_offset)) {
            local_file.close();
            return false;
        }
        
        // Read directly into our dedicated 16KB LVGL image container array
        local_file.read(&lvgl_sprite_buffer[y * 128], 128);
    }
    Serial.printf("A end: %d\n",time_us_64());

    local_file.close();

    // --- POPULATE STANDARD LVGL V9 IMAGE DESCRIPTOR ---
    sprite_img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC; 
    sprite_img_dsc.header.cf = LV_COLOR_FORMAT_L8;       // 8bpp format matrix
    sprite_img_dsc.header.w = 128;                       // Force bounds to 128px wide
    sprite_img_dsc.header.h = 128;                       // Force bounds to 128px high
    sprite_img_dsc.header.stride = 128;                  // Bytes per line inside buffer
    sprite_img_dsc.data_size = sizeof(lvgl_sprite_buffer);
    sprite_img_dsc.data = lvgl_sprite_buffer;            // Bind payload data pointer

    return true;
}

bool load_sprite_to_lvgl_dsc2(const char* filename, int row, int col) {
    fat_fs.chvol(); 
    
    File32 local_file;
    int retry_count = 0;
    const int max_retries = 5;

    // GATEWAY RETRY LOOP: Fight USB block collisions safely
    while (retry_count < max_retries) {
        local_file = fat_fs.open(filename, O_RDONLY);
        if (local_file) break; // Success! Break out of the loop

        retry_count++;
        Serial.print("[RETRY LOG] USB sector contention detected. Retrying open count: ");
        Serial.println(retry_count);
        delay(5); // Give the background USB engine 5ms to clear its lock
    }

    if (!local_file) {
        Serial.print("SdFat CRITICAL ERROR: File handle refused after maximum retries: ");
        Serial.println(filename);
        return false;
    }

    // Default configuration metrics for film.cmp (12 columns of 128x128 sprites)
    int sheet_cols = 12;

    // Detect if we are parsing film2.cmp (2 rows and 2 columns of 128x128 sprites)
    if (strstr(filename, "film2") != nullptr) {
        sheet_cols = 2; 
    }

    const uint32_t file_header_offset = 12;        // Verified 12-byte metadata skip
    const uint32_t sprite_size_bytes = 128 * 128;  // Exactly 16,384 bytes per sprite

    // Calculate sequential sprite index and final file position
    int sprite_index = (row * sheet_cols) + col;
    uint32_t file_offset = file_header_offset + (sprite_index * sprite_size_bytes);

    // Extract the entire contiguous 128x128 box in one operation
    Serial.printf("A start: %d\n", time_us_64());
    
    if (!local_file.seek(file_offset)) {
        local_file.close();
        return false;
    }
    
    // Massive 16KB burst read directly into the LVGL image container array
    local_file.read(lvgl_sprite_buffer, sprite_size_bytes);
    
    Serial.printf("A end: %d\n", time_us_64());

    local_file.close();

    // --- POPULATE STANDARD LVGL V9 IMAGE DESCRIPTOR ---
    sprite_img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC; 
    sprite_img_dsc.header.cf = LV_COLOR_FORMAT_L8;       // 8bpp format matrix
    sprite_img_dsc.header.w = 128;                       // Force bounds to 128px wide
    sprite_img_dsc.header.h = 128;                       // Force bounds to 128px high
    sprite_img_dsc.header.stride = 128;                  // Bytes per line inside buffer
    sprite_img_dsc.data_size = sizeof(lvgl_sprite_buffer);
    sprite_img_dsc.data = lvgl_sprite_buffer;            // Bind payload data pointer

    return true;
}


bool is_task=false;
uint64_t last_us = 0;
uint32_t frame_id=0;
void loop() {
  if((time_us_64()-last_us)>16666)
  {
    last_us=time_us_64();
    scatterer_gatherer_engine_screen.compileAndRun(frame_id);
    frame_id++;
    lv_timer_handler();//gen next frame
  }

  if(!is_task && millis()>7000)
  {
    debug_raw_sector_check();
    //demo_print_filesystem();
    demo_print_filesystem2();
    is_task=true;
  }

  static uint32_t last_write_time = 0;
  
  // Track if cache is dirty and record the timestamp
  if (cache_is_dirty && last_write_time == 0) {
    last_write_time = millis();
  }

  // If data has been sitting unwritten in our RAM cache for more than 2 seconds,
  // force a physical hardware flush to the flash chip automatically.
  if (cache_is_dirty && (millis() - last_write_time > 2000)) {
    flush_sector_cache();
    last_write_time = 0; // Reset timer
    Serial.println("[AUTO-FLUSH] Idle timeout reached. Cache committed to flash!");
  }
}
