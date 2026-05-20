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
    volatile uint32_t _raw_lux;
    
    //to be verified: The I2C hardware macro block cannot change its target address (IC_TAR) while the peripheral is actively enabled. Attempting to write to IC_TAR via DMA while the block is active will cause the hardware to silently ignore the transaction.
    uint32_t _i2c_disable       __attribute__((aligned(4))) = 0;
    uint32_t _i2c_target_addr   __attribute__((aligned(4))) = LTR308_I2C_ADDR;
    uint32_t _i2c_enable        __attribute__((aligned(4))) = 1;


    // Command buffers aligned for DMA safety
    // LTR-308 Active Mode Config: Write 0x01 to MAIN_CTRL (0x00)
    uint16_t _boot_cmd[2] __attribute__((aligned(4))); 
    uint16_t _boot_cmd2[2] __attribute__((aligned(4))); 
    
    // I2C requires writing read requests to IC_DATA_CMD (bit 8 set) to trigger read clocks
    uint16_t _read_request[4] __attribute__((aligned(4))); 
    
    // Target buffer for incoming RX FIFO data
    volatile uint16_t _rx_buffer[3] __attribute__((aligned(4)));

public:
    LightSensor(i2c_inst_t* i2c_hardware = i2c0);
    
    void begin(int sda_pin, int scl_pin, uint32_t baudrate = 400000);

    // Get the calculated brightness
    uint32_t getBrightness() const;

    // IMultiDmaTransactionSource Interface
    int getRequiredDescriptorCount(uint32_t frame_id, uint8_t subframe_id, uint8_t subframe_max) override;
    void populateDescriptors(uint32_t frame_id, uint8_t subframe_id, uint8_t subframe_max, DmaDescriptor* pool_start, int data_channel) override;
};

#endif