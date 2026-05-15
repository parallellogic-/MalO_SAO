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

const uint8_t init_128x128[] = {
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

const uint8_t contrast_command_buffer_1[]={SSD1327_DISPLAYON};
const uint8_t contrast_command_buffer_2[]={0x81,0x2F};

const uint8_t frame_command_buffer[]={
                   SSD1327_SETROW,    0, 0x7F,
                   SSD1327_SETCOLUMN, 0, 0x3F};


SSD1327::SSD1327():SSD1327(9,8,11,10,spi1,8'000'000){}
SSD1327::SSD1327(uint8_t cs, uint8_t dc, uint8_t mosi, uint8_t sclk,spi_inst_t* spi_port,uint32_t spi_baud){
    _cs=cs; _dc=dc; _mosi=mosi; _sclk=sclk; _spi_port=spi_port; _spi_baud=spi_baud;
}
void SSD1327::begin(){
  //claim spi channel, init spi pins
  spi_init(_spi_port, _spi_baud);
  gpio_set_function(_sclk, GPIO_FUNC_SPI);
  gpio_set_function(_mosi, GPIO_FUNC_SPI);
  
  // Set CS and DC pins as GPIO outputs (software controlled)
  gpio_init(_cs);
  gpio_set_dir(_cs, GPIO_OUT);
  gpio_put(_cs, HIGH); // CS high by default (inactive)

  gpio_init(_dc);
  gpio_set_dir(_dc, GPIO_OUT);
  
  // --- DMA Setup ---
  _dma_tx_channel = dma_claim_unused_channel(true);

  dma_channel_config c = dma_channel_get_default_config(_dma_tx_channel);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_8); // 8-bit transfers
  channel_config_set_read_increment(&c, true); // Increment read address (source buffer)
  channel_config_set_write_increment(&c, false); // Don't increment write address (SPI data register is a fixed address)
  // Set the DREQ for SPI0 TX to automatically trigger transfers
  channel_config_set_dreq(&c, spi_get_dreq(_spi_port, true)); 

  // Configure the DMA channel, but don't start it yet
  dma_channel_configure(
      _dma_tx_channel,
      &c,
      &spi_get_hw(_spi_port)->dr, // Destination: SPI Data Register
      _tx_buffer[_display_index],// Source: our data buffer
      SSD1327_BUFFER_SIZE,       // Number of transfers
      false                      // Don't start immediately
  );

  delay(100);//need >30ms for screen to boot up stable, otherwise comes up with inverted or offset colors (?)
  send_data_dma(init_128x128, sizeof(init_128x128),false);
  delay(100);
  send_data_dma(contrast_command_buffer_1, sizeof(contrast_command_buffer_1),false);
  send_data_dma(contrast_command_buffer_2, sizeof(contrast_command_buffer_2),false);
}
void SSD1327::flush(){//push buffer to hardware
  this->_display_index^=1;
  draw();
}
void SSD1327::draw(){//update hardware
  send_data_dma(frame_command_buffer, sizeof(frame_command_buffer),false);
  send_data_dma(_tx_buffer[_display_index], SSD1327_BUFFER_SIZE,true);//dc true only for frame data
}
uint8_t* SSD1327::get_buffer(){ return get_buffer(1); }
uint8_t* SSD1327::get_buffer(bool is_black){//pointer to buffer that can be written into.  can set to all zeros with is_black=true (returns stale values if is_black=false)
  if(is_black)
  {
    for (int i = 0; i < SSD1327_BUFFER_SIZE; i++)
      _tx_buffer[!_display_index][i]=0;
  }
  return _tx_buffer[!_display_index];
}
//TODO: refactor without blocking calls...
void SSD1327::send_data_dma(const uint8_t *data, size_t len, bool dc_value) {
    // Ensure the previous DMA transfer is complete
    dma_channel_wait_for_finish_blocking(_dma_tx_channel);
    while (spi_is_busy(_spi_port));

    // Set Data/Command line to Data mode (if required by your OLED)
    gpio_put(_dc,dc_value);

    // Pull CS low to begin transaction
    gpio_put(_cs, LOW);

    // Reconfigure the DMA transfer count for the current data length
    dma_channel_set_read_addr(_dma_tx_channel, data, false);
    dma_channel_set_trans_count(_dma_tx_channel, len, false);
    
    // Start the DMA transfer
    dma_channel_start(_dma_tx_channel);

    // Wait for the DMA transfer to complete without blocking the CPU
    // The CPU can do other tasks here if needed
    dma_channel_wait_for_finish_blocking(_dma_tx_channel);
    while (spi_is_busy(_spi_port));
    // Pull CS high to end transaction
    gpio_put(_cs, HIGH);

}