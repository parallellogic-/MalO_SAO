#pragma once

#include "hardware/spi.h"

#define SSD1327_SETCOLUMN 0x15
#define SSD1327_SETROW 0x75
#define SSD1327_SETCONTRAST 0x81
#define SSD1305_SETLUT 0x91

#define SSD1327_SEGREMAP 0xA0
#define SSD1327_SETSTARTLINE 0xA1
#define SSD1327_SETDISPLAYOFFSET 0xA2
#define SSD1327_NORMALDISPLAY 0xA4
#define SSD1327_DISPLAYALLON 0xA5
#define SSD1327_DISPLAYALLOFF 0xA6
#define SSD1327_INVERTDISPLAY 0xA7
#define SSD1327_SETMULTIPLEX 0xA8
#define SSD1327_REGULATOR 0xAB
#define SSD1327_DISPLAYOFF 0xAE
#define SSD1327_DISPLAYON 0xAF

#define SSD1327_PHASELEN 0xB1
#define SSD1327_DCLK 0xB3
#define SSD1327_PRECHARGE2 0xB6
#define SSD1327_GRAYTABLE 0xB8
#define SSD1327_PRECHARGE 0xBC
#define SSD1327_SETVCOM 0xBE

#define SSD1327_FUNCSELB 0xD5
#define SSD1327_CMDLOCK 0xFD

#define SSD1327_BUFFER_SIZE 128*128/2

/*typedef struct {
    uint8_t transform;
      //0: transpose/scale: sx*x+tx, sy*y+ty.  args[0], args[1] is tx, ty, args[2],args[3] is sx,sy
      //1: rotation about origin: x'=rx+(x-rx)*cos(a)-(y-ry)*sin(a); y'=yr+(x-rx)*sin(s)+(y-yr)*cos(a)
      //2: quaternion (via tx,ty,sx,sy) -> wxyz
      float args[4]={};
    //float tx, ty;//translation amount
    //float sx, sy;//scale (mirror)
    //float angle_degrees;
    //float rx, ry;//center of rotation --> use a translation forward forward and back
    //float qw,qx,qy,qz;//quaternion (replaces shear) - isn't AffineTransform -> ProjectiveTransform
} AffineTransform;
//if no quaternions, then do affine transform to save on compute (affine about 15x faster without divide operation)

class Rectangle{
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
}

//active area defines the 0,0 point
class SpriteTransform{
  Sprite* sprite;
  bool active_area_limit=false;//true - limits to arctive_area.  fills area outside active_area with null color
  Rectangle active_area;//nullptr to .  defined in the sprite's coordinate frame (ex. using a subset of sprite sheet)
  ProjectiveTransform* transform_list;
  uint8_t transform_count;
}

//if sprite is nullptr on pixel_list, then will fill with null_color (ex. how to create a rectangle of single color)
class Sprite{
  uint8_t null_color;//when trying to access out-of-bounds during transform, what color to default to (ex. transparent, white, black, etc).  is defined in c48 format
  uint16_t width_px; //number of pixels - c04 and c14 have half as many bytes as c48
  uint16_t height_px; //used for ensuring queries points are in bounds
  uint8_t pixel_format; //format of images: 0: c04 (4 bit color, no alpha), 1: c14 (0x00 means transparent, any other nibble is non-transparent color), 2: c48 (upper nibble is alpha channel), lower 4 bits is color
  uint8_t* pixel_list; //for c04 and c14, there are half as many bytes as c48
};*/

class Screen : public IMultiDmaTransactionSource {
private:
    static constexpr uint8_t init_128x128[] = {
          // Init sequence for 128x32 OLED module
          SSD1327_DISPLAYOFF, // 0xAE
          SSD1327_SETCONTRAST,
          0x80,             // 0x81, 0x80
          SSD1327_SEGREMAP, // 0xA0 0x53
          0x51, // remap memory, odd even columns, com flip and column swap
          SSD1327_SETSTARTLINE,
          0x00, // 0xA1, 0x00
          SSD1327_SETDISPLAYOFFSET,
          0x00, // 0xA2, 0x00
          SSD1327_DISPLAYALLOFF, SSD1327_SETMULTIPLEX,
          0x7F, // 0xA8, 0x7F (1/64)
          SSD1327_PHASELEN,
          0x11, // 0xB1, 0x11
          /*
          SSD1327_GRAYTABLE,
          0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
          0x07, 0x08, 0x10, 0x18, 0x20, 0x2f, 0x38, 0x3f,
          */
          SSD1327_DCLK,
          0x00, // 0xb3, 0x00 (100hz)
          SSD1327_REGULATOR,
          0x01, // 0xAB, 0x01
          SSD1327_PRECHARGE2,
          0x04, // 0xB6, 0x04
          SSD1327_SETVCOM,
          0x0F, // 0xBE, 0x0F
          SSD1327_PRECHARGE,
          0x08, // 0xBC, 0x08
          SSD1327_FUNCSELB,
          0x62, // 0xD5, 0x62
          SSD1327_CMDLOCK,
          0x12, // 0xFD, 0x12
          SSD1327_NORMALDISPLAY, SSD1327_DISPLAYON};

    static constexpr uint8_t contrast_command_buffer[]={SSD1327_DISPLAYON,0x81,0x2F};

    static constexpr uint8_t frame_command_buffer[]={
                      SSD1327_SETROW,    0, 0x7F,
                      SSD1327_SETCOLUMN, 0, 0x3F};

    // ----

    spi_inst_t* _spi;
    uint32_t _baud=8'000'000;

    volatile uint32_t *_dc_pin_ctrl_reg_ptr;

    bool _is_flush=0; //application raises flag with flush()
    bool _screen_ping_pong=0; //internal varaible to track toggling between screen buffers
    uint8_t _frame_buffer[2][SSD1327_BUFFER_SIZE] __attribute__((aligned(4)))={};

    uint8_t _get_boot_state(uint64_t frame_id) const; //detemrine what stage of commands to send we're at
public:
    Screen(spi_inst_t* spi_port = spi1, uint32_t baud = 8'000'000, uint8_t dc_pin = 9);
    
    void begin();
    void end();

    //pointer to where application can upload the frame information (4-bits per pixel)
    //be aware of existing dirty frame contents present in buffer
    uint8_t* get_frame_buffer();

    //if no flush, then screen re-sends the previous frame (holds frame static until flush())
    void flush();

    // IMultiDmaTransactionSource Interface
    int getRequiredDescriptorCount(uint64_t frame_id) override;
    void populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) override;
};

