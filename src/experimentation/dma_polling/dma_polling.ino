#include <Arduino.h>
#include "hardware/dma.h"
#include "hardware/i2c.h"

#define I2C_INST i2c0 

int dma_chan_polling;
int dma_chan_config_tar; // Placeholder for Channel 3
uint32_t dummy_destination_buffer = 0;
const uint32_t I2C_TX_EMPTY_MASK = 0x00000010; // Bit 4 of IC_RAW_INTR_STAT

void setup_polling_channel() {
    // 1. Claim channels safely
    dma_chan_polling = dma_claim_unused_channel(true);
    dma_chan_config_tar = dma_claim_unused_channel(true); 

    // 2. Set up pacing timer
    int timer_id = 0; 
    dma_timer_set_fraction(timer_id, 1, 1500); 
    int timer_dreq = dma_get_timer_dreq(timer_id);

    // 3. Configure standard channel settings
    dma_channel_config c2 = dma_channel_get_default_config(dma_chan_polling);
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_32); 
    channel_config_set_read_increment(&c2, false);          
    channel_config_set_write_increment(&c2, false);         
    channel_config_set_dreq(&c2, timer_dreq);               
    channel_config_set_chain_to(&c2, dma_chan_config_tar);  
    
    // Explicitly enable sniffing in the channel's structure
    channel_config_set_sniff_enable(&c2, true);

    // 4. FIX: Use the SDK function to enable the sniffer for this channel.
    // Passing '0' maps to the underlying hardware "normal/none" CRC/checksum function mode
    // which allows raw pass-through data matching.
    dma_sniffer_enable(dma_chan_polling, 0, true);
    
    // Seed the hardware sniffer data register with the targeted mask comparison value
    dma_hw->sniff_data = I2C_TX_EMPTY_MASK; 

    // 5. Apply and prime the channel configuration
    dma_channel_configure(
        dma_chan_polling,
        &c2,
        &dummy_destination_buffer,                
        &i2c_get_hw(I2C_INST)->raw_intr_stat,     
        0xFFFF,                                   
        false                                     
    );
}

void setup() {
    Serial.begin(115200);
    i2c_init(I2C_INST, 400000); 
    setup_polling_channel();
}

void loop() {
    delay(1000);
}
