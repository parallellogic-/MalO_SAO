#include <hardware/pio.h>
#include <hardware/dma.h>
#include "charlie.pio.h"

// 64 elements * 4 bytes = 256 bytes. 
// We must align to 256 so the hardware "wrap" math works correctly.
uint32_t sequence_A[64] __attribute__((aligned(256)));
uint32_t sequence_B[64] __attribute__((aligned(256)));
uint32_t sequence_C[3][64] __attribute__((aligned(256)));
uint8_t sequence_C_index=0;

PIO pio = pio0;
uint sm;
int dma_chan;

void setup() {
    /*sequence_A[0] = (0x1000UL << 16) | (0x01 << 8) | 0x03; 
    sequence_A[1] = (0x1000UL << 16) | (0x02 << 8) | 0x03;
    sequence_A[2] = (0x1000UL << 16) | (0x04 << 8) | 0x06;
    sequence_A[3] = (0x1000UL << 16) | (0x08 << 8) | 0x0C;
    sequence_A[4] = (0xF000UL << 16) | (0x10 << 8) | 0x18;

    sequence_B[0] = (0x1000UL << 16) | (0x01 << 8) | 0x03; 
    sequence_B[1] = (0x1000UL << 16) | (0x02 << 8) | 0x03;
    sequence_B[2] = (0x1000UL << 16) | (0x00 << 8) | 0x00;*/

    sequence_A[0] = (0x1000UL << 16) | (0x03 << 8) | 0x01; 
    sequence_A[1] = (0x1000UL << 16) | (0x03 << 8) | 0x02;
    sequence_A[2] = (0x1000UL << 16) | (0x06 << 8) | 0x04;
    sequence_A[3] = (0x1000UL << 16) | (0x0C << 8) | 0x08;
    sequence_A[4] = (0xF000UL << 16) | (0x18 << 8) | 0x10;

    sequence_B[0] = (0x1000UL << 16) | (0x03 << 8) | 0x01; 
    sequence_B[1] = (0x1000UL << 16) | (0x03 << 8) | 0x02;
    sequence_B[2] = (0x1000UL << 16) | (0x00 << 8) | 0x00;

    // 1. Initialize PIO
    uint offset = pio_add_program(pio, &charlie_dma_program);
    sm = pio_claim_unused_sm(pio, true);
    charlie_dma_program_init(pio, sm, offset, 2, 8);

    // Set pins 2 through 9 to minimum drive strength (2mA)
    /*for (int i = 2; i < 10; i++) {
        gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_2MA);
    }*/

    // 2. Configure DMA
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));

    // RING CONFIGURATION:
    // false = ring the READ address.
    // 8 = ring size is 2^8 = 256 bytes.
    // This makes the DMA wrap back to the start every 64 uint32_ts.
    channel_config_set_ring(&c, false, 8); 

    dma_channel_configure(
        dma_chan, &c,
        &pio->txf[sm], 
        sequence_A, 
        0xFFFFFFFF, // Run for "forever" (4 billion transfers)
        true        // Start
    );
}

void loop() {
    // Swap the sequence mid-flight
    // The DMA will continue to "ring" inside the new 256-byte block
    if(false)
    {
        delay(1000);
        dma_channel_set_read_addr(dma_chan, sequence_B, true);
        
        delay(1000);
        dma_channel_set_read_addr(dma_chan, sequence_A, true);
    }
    if(false)
    {
        dma_channel_set_read_addr(dma_chan, sequence_A, true);
        uint8_t index=0;
        for(int row=0;row<8;row++)
        {
            for(int col=0;col<8;col++)
            {
                if(row==col) continue;
                sequence_A[index]=0;
                index++;
            }
        }
        index=0;
        sequence_A[63]=(0xFFFFUL << 16) | (0x00 << 8) | 0x00; 
        for(int row=0;row<8;row++)
        {
            for(int col=0;col<8;col++)
            {
                if(row==col) continue;
                uint8_t dir=(1<<row) | (1<<col);
                uint8_t value=(1<<row);
                sequence_A[index]=(0xFD01UL << 16) | (dir << 8) | value; 
                delay(100);
                sequence_A[index]=0;
                index++;
            }
        }
    }
    if(true)
    {
        uint32_t total_darkness=255*255*8*7;
        uint8_t index=0;
        uint16_t brightness=(uint16_t)(millis()/32);//+row*16+col*16;
        brightness=(uint8_t)brightness;
        brightness=brightness*brightness;
        brightness=64*64;//33*33;
        for(int row=0;row<8;row++)
        {
            for(int col=0;col<8;col++)
            {
                if(row==col) continue;
                uint8_t dir=(1<<row) | (1<<col);
                uint8_t value=(1<<row);
                sequence_C[sequence_C_index][index]=(brightness << 16) | (dir << 8) | value; 
                index++;
                total_darkness-=brightness;
            }
        }
        sequence_C[sequence_C_index][63]=total_darkness<<8;
        dma_channel_set_read_addr(dma_chan, sequence_C[sequence_C_index], false);
        sequence_C_index=(sequence_C_index+1)%3;
        delay(2000);
    }
}