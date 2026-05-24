#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <Arduino.h>
#include <hardware/i2c.h>
#include <hardware/dma.h>
#include "dma_control_block.h"

// LTR-308ALS-01 I2C Specifications
#define LTR308_I2C_ADDR      0x53
#define LTR308_MAIN_CTRL     0x00
#define LTR308_ALS_DATA_0    0x0D // ALS Data Ch0 (Low Byte)
#define LTR308_ALS_DATA_1    0x0E // ALS Data Ch0 (Mid Byte)
#define LTR308_ALS_DATA_2    0x0F // ALS Data Ch0 (High Byte)

class LightSensor : public IMultiDmaTransactionSource {
private:
    i2c_inst_t* _i2c;
    bool _raw_lux_ping_pong=0;
    volatile uint32_t _raw_lux[2] __attribute__((aligned(4)))={0,0};
    bool _is_booted=0;
    
    //to be verified: The I2C hardware macro block cannot change its target address (IC_TAR) while the peripheral is actively enabled. Attempting to write to IC_TAR via DMA while the block is active will cause the hardware to silently ignore the transaction.
    uint32_t _i2c_disable       __attribute__((aligned(4))) = 0;
    uint32_t _i2c_target_addr   __attribute__((aligned(4))) = LTR308_I2C_ADDR;
    uint32_t _i2c_enable        __attribute__((aligned(4))) = 1;

    // Command buffers aligned for DMA safety
    // LTR-308 Active Mode Config: Write 0x01 to MAIN_CTRL (0x00)
    //uint16_t _boot_cmd[2] __attribute__((aligned(4))); 
    //uint16_t _boot_cmd2[2] __attribute__((aligned(4))); 
    const uint16_t _boot_cmd[2][2] __attribute__((aligned(4)))={
        {LTR308_MAIN_CTRL,0x02}, // Active mode, Gain = 1 //0202
        {0x04 | 0x0400, 0x40 | 0x0200},// Targets the ALS_GAIN / Integration Time Register // 0x04 = 10ms integration time + 0x0200 (I2C STOP bit!)
    }

    /*const uint16_t _boot_check_cmd[1][2] __attribute__((aligned(4)))={
      | 0x0400, 
    }; //to cleanly switch between i2c targets, need to end with a read operation to ensure fifos are empty
    uint8_t _boot_check;*/

    // I2C requires writing read requests to IC_DATA_CMD (bit 8 set) to trigger read clocks
    uint16_t _read_request[4] __attribute__((aligned(4)))={
    LTR308_ALS_DATA_0, // Point I2C to start reading at Data 0
    0x0100,            // Command a Read byte (Bit 8 is CMD_READ)
    0x0100,            // Command a Read byte
    0x0300            // Command a Read byte //0300
    }; 
    
    // Target buffer for incoming RX FIFO data
    //volatile uint16_t _rx_buffer[3] __attribute__((aligned(4)));

public:
    LightSensor(i2c_inst_t* i2c_hardware = i2c0);
    
    void begin();

    // Get the calculated brightness
    uint32_t getBrightness() const;

    // IMultiDmaTransactionSource Interface
    int getRequiredDescriptorCount(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max) override;
    void populateDescriptors(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) override;
};

#endif