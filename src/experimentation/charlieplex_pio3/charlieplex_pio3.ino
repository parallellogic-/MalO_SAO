#include <hardware/pio.h>
#include <hardware/dma.h>
#include "charlie.pio.h"

uint8_t const FIRST_PIN=2;
uint8_t const sequence_C_len=24*2+1;//8*7+1;
uint32_t sequence_C[3][sequence_C_len];// __attribute__((aligned(256)));
uint8_t sequence_C_index = 0;
uint32_t* current_list_ptr = sequence_C[0]; // The "Next" list pointer

PIO pio = pio0;
uint sm;
int data_chan;
int ctrl_chan;

void setup() {
    // Initialize Built-in LED
    pinMode(LED_BUILTIN, OUTPUT);

    // 1. Initialize PIO
    uint offset = pio_add_program(pio, &charlie_dma_program);
    sm = pio_claim_unused_sm(pio, true);
    charlie_dma_program_init(pio, sm, offset, FIRST_PIN, 8);

    // 2. Configure DATA DMA (The worker)
    data_chan = dma_claim_unused_channel(true);
    dma_channel_config c_data = dma_channel_get_default_config(data_chan);
    channel_config_set_transfer_data_size(&c_data, DMA_SIZE_32);
    channel_config_set_read_increment(&c_data, true);
    channel_config_set_write_increment(&c_data, false);
    channel_config_set_dreq(&c_data, pio_get_dreq(pio, sm, true));
    
    // 3. Configure CONTROL DMA (The restarter)
    ctrl_chan = dma_claim_unused_channel(true);
    dma_channel_config c_ctrl = dma_channel_get_default_config(ctrl_chan);
    channel_config_set_transfer_data_size(&c_ctrl, DMA_SIZE_32);
    channel_config_set_read_increment(&c_ctrl, false);
    channel_config_set_write_increment(&c_ctrl, false);

    // CHAIN: Data finishing triggers Control
    channel_config_set_chain_to(&c_data, ctrl_chan);

    dma_channel_configure(
        data_chan, &c_data,
        &pio->txf[sm],     
        current_list_ptr,  
        sequence_C_len,                // Process exactly 64 elements
        false              
    );

    dma_channel_configure(
        ctrl_chan, &c_ctrl,
        &dma_hw->ch[data_chan].al3_read_addr_trig, // Restart Data Channel
        &current_list_ptr,                         // By reading the pointer variable
        1,
        true                                       // Start!
    );
}

void loop() {
    if(true)
    {
        uint32_t total_darkness = 255 * 255 * (sequence_C_len-1);
        uint8_t index = 0;
        
        // Use a different index than the one currently being displayed by DMA
        uint8_t write_index = (sequence_C_index + 1) % 3;

        for(int row=0; row<8; row++) {
            for(int col=0; col<8; col++) {
                if(row==col) continue;
                if(index==(sequence_C_len-1)) break;
                uint8_t dir = (1<<row) | (1<<col);
                uint8_t value = (1<<row);

                uint16_t brightness = (millis()/4)+row*16+col*16; 
                if(brightness & 0x0100) brightness=255-(uint8_t)brightness;
                brightness=0x00FF & brightness;
                brightness=brightness*brightness;

                sequence_C[write_index][index] = (brightness << 16) | (dir << 8) | value; 
                index++;
                total_darkness -= brightness;
            }
        }
        
        // Your long-wait blackout at the end
        sequence_C[write_index][index] = total_darkness << 8; 

        // CRITICAL FIX: Update the pointer the Control Channel uses.
        // This is a "lazy" update. The hardware will switch to this address
        // ONLY once the current 64-step sequence is totally done.
        current_list_ptr = sequence_C[write_index];

        sequence_C_index = write_index;
        //delay(16); 
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
}
