//#include <SPI.h>
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"

// --
#define SSD1327_BLACK 0x0
#define SSD1327_WHITE 0xF

#define SSD1327_I2C_ADDRESS 0x3D

#define SSD1305_SETBRIGHTNESS 0x82

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
// --

// Pin definitions
#define OLED_CLK   10
#define OLED_MOSI  11
#define OLED_CS    9
#define OLED_DC    8

// SPI configuration
#define SPI_PORT   spi1
const uint SPI_SPEED = 8000000; // 8 MHz
//8MHz is ~11ms per frame
//32 MHz is ~3 ms per frame
//64 MHz is unstable

uint8_t init_128x128[] = {
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

uint8_t contrast_command_buffer_1[]={SSD1327_DISPLAYON};
uint8_t contrast_command_buffer_2[]={0x81,0x2F};

uint8_t frame_command_buffer[]={
                   SSD1327_SETROW,    0, 0x7F,
                   SSD1327_SETCOLUMN, 0, 0x3F};

// Data buffer for transmission
// Use a large enough buffer for your needs
#define BUFFER_SIZE 128*128/2 //one nibble per pixel 
uint8_t tx_buffer[BUFFER_SIZE];


// DMA channel for SPI TX
int dma_tx_channel;

// Function to control the Data/Command pin (software controlled)
void set_dc_pin(bool isData) {
    gpio_put(OLED_DC,isData);
    /*if (isData) {
        gpio_put(OLED_DC, HIGH);
    } else {
        gpio_put(OLED_DC, LOW);
    }*/
}

void setup() {
    Serial.begin(115200);
    //while(!Serial);
    // Initialize buffer with some data (e.g., a screen's pixel data)
    for (int i = 0; i < BUFFER_SIZE; i++) {
        tx_buffer[i] = i % 256; 
    }

    // --- SPI and GPIO Setup ---
    // Initialize the hardware SPI peripheral 
    spi_init(SPI_PORT, SPI_SPEED);
    gpio_set_function(OLED_CLK, GPIO_FUNC_SPI);
    gpio_set_function(OLED_MOSI, GPIO_FUNC_SPI);
    // MISO is not needed for an OLED (output only), but could be set with gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    
    // Set CS and DC pins as GPIO outputs (software controlled)
    gpio_init(OLED_CS);//, GPIO_FUNC_SIO);
    gpio_set_dir(OLED_CS, GPIO_OUT);
    gpio_put(OLED_CS, HIGH); // CS high by default (inactive)

    gpio_init(OLED_DC);//, GPIO_FUNC_SIO);
    gpio_set_dir(OLED_DC, GPIO_OUT);
    
    // --- DMA Setup ---
    //dma_tx_channel = dma_channel_get_default_config(DMA_CHANNEL_ABSTRACTION_COUNT); // Get a free DMA channel
    dma_tx_channel = dma_claim_unused_channel(true);
    //Serial.print("Setup dma_tx_channel on: ");//0
    //Serial.println(dma_tx_channel,DEC);

    dma_channel_config c = dma_channel_get_default_config(dma_tx_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8); // 8-bit transfers
    channel_config_set_read_increment(&c, true); // Increment read address (source buffer)
    channel_config_set_write_increment(&c, false); // Don't increment write address (SPI data register is a fixed address)
    // Set the DREQ for SPI0 TX to automatically trigger transfers
    channel_config_set_dreq(&c, spi_get_dreq(SPI_PORT, true)); 

    // Configure the DMA channel, but don't start it yet
    dma_channel_configure(
        dma_tx_channel,
        &c,
        &spi_get_hw(SPI_PORT)->dr, // Destination: SPI Data Register
        tx_buffer,                 // Source: our data buffer
        BUFFER_SIZE,               // Number of transfers
        false                      // Don't start immediately
    );

    delay(100);//need >30ms for screen to boot up stable, otherwise comes up with inverted or offset colors (?)
    send_data_dma(init_128x128, sizeof(init_128x128),false);
    delay(100);
    send_data_dma(contrast_command_buffer_1, sizeof(contrast_command_buffer_1),false);
    //delay(1);
    send_data_dma(contrast_command_buffer_2, sizeof(contrast_command_buffer_2),false);
    //delay(1);
}

void loop() {
    // Example usage: send the whole buffer as display data
    unsigned long tms_start=millis();
    send_data_dma(frame_command_buffer, sizeof(frame_command_buffer),false);
    //delay(1);
    send_data_dma(tx_buffer, BUFFER_SIZE,true);//dc true only for frame data
    
    for (int i = 0; i < BUFFER_SIZE; i++) {
        tx_buffer[i]+=33; //animation
    }
    while((tms_start+16)>millis());
    //delay(5); // Send every 100ms
}

void send_data_dma(uint8_t *data, size_t len, bool dc_value) {
    // Ensure the previous DMA transfer is complete
    dma_channel_wait_for_finish_blocking(dma_tx_channel);
    while (spi_is_busy(SPI_PORT));

    //Serial.print("send_data_dma on: ");//0
    //Serial.println(dma_tx_channel,DEC);

    // Set Data/Command line to Data mode (if required by your OLED)
    set_dc_pin(dc_value);

    // Pull CS low to begin transaction
    gpio_put(OLED_CS, LOW);

    // Reconfigure the DMA transfer count for the current data length
    dma_channel_set_read_addr(dma_tx_channel, data, false);
    dma_channel_set_trans_count(dma_tx_channel, len, false);
    
    // Start the DMA transfer
    dma_channel_start(dma_tx_channel);

    // Wait for the DMA transfer to complete without blocking the CPU
    // The CPU can do other tasks here if needed
    dma_channel_wait_for_finish_blocking(dma_tx_channel);
    while (spi_is_busy(SPI_PORT));
    // Pull CS high to end transaction
    gpio_put(OLED_CS, HIGH);

}
