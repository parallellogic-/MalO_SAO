
#include "light_sensor.h"
#include "hardware/i2c.h"

LightSensor::LightSensor(i2c_inst_t* i2c_hardware) : _i2c(i2c_hardware), _raw_lux(0) {
    // Construct the initialization command: Write 0x01 to MAIN_CTRL register
    _boot_cmd[0] = LTR308_MAIN_CTRL;
    _boot_cmd[1] = 0x01; // Active mode, Gain = 1

    // Construct the read sequence commands
    _read_request[0] = LTR308_ALS_DATA_0; // Point I2C to start reading at Data 0
    _read_request[1] = 0x0100;            // Command a Read byte (Bit 8 is CMD_READ)
    _read_request[2] = 0x0100;            // Command a Read byte
    _read_request[3] = 0x0100;            // Command a Read byte
    
    memset((void*)_rx_buffer, 0, sizeof(_rx_buffer));
}

void LightSensor::begin(int sda_pin, int scl_pin, uint32_t baudrate) {
    i2c_init(_i2c, baudrate);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);
}

uint32_t LightSensor::getBrightness() const {
    return _raw_lux;
}

int LightSensor::getRequiredDescriptorCount(uint32_t frame_id, uint8_t subframe_id, uint8_t subframe_max) {
    if (subframe_id > 0) return 0;
    
    // We add 3 additional descriptor operations ahead of the data blocks to modify the I2C block target configuration 
    if (frame_id == 0) {
        return 3 + 1; // 3 Address configs + 1 Boot execution payload block
    } else {
        return 3 + 2; // 3 Address configs + 1 TX request block + 1 RX fetch block
    }
}

void LightSensor::populateDescriptors(uint32_t frame_id, uint8_t subframe_id, uint8_t subframe_max, DmaDescriptor* pool_start, int data_channel) {
    if (subframe_id > 0) return;

    // Hardware Base Registers mapping pointers
    i2c_hw_t* hw = i2c_get_hw(_i2c);
    volatile void* i2c_enable_reg   = (volatile void*)&hw->enable;
    volatile void* i2c_tar_reg      = (volatile void*)&hw->tar;
    volatile void* i2c_data_cmd_reg = (volatile void*)&hw->data_cmd;

    // --- BASE DMA REGISTER METADATA FOR HARDWARE OVERRIDES ---
    // These run without a Data Request (DREQ) pacing flag because internal hardware register updates complete instantly
    dma_channel_config cfg_reg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg_reg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg_reg, false);
    channel_config_set_write_increment(&cfg_reg, false);

    // Stage 1: Disable I2C Engine (Required to update TAR)
    pool_start[0].read_addr      = &_i2c_disable;
    pool_start[0].write_addr     = (void*)i2c_enable_reg;
    pool_start[0].transfer_count = 1;
    pool_start[0].config         = cfg_reg.ctrl;

    // Stage 2: Inject Target Slave Device Address into I2C Core
    pool_start[1].read_addr      = &_i2c_target_addr;
    pool_start[1].write_addr     = (void*)i2c_tar_reg;
    pool_start[1].transfer_count = 1;
    pool_start[1].config         = cfg_reg.ctrl;

    // Stage 3: Re-Enable I2C Engine 
    pool_start[2].read_addr      = &_i2c_enable;
    pool_start[2].write_addr     = (void*)i2c_enable_reg;
    pool_start[2].transfer_count = 1;
    pool_start[2].config         = cfg_reg.ctrl;

    // --- DATA PAYLOAD TRANSMISSION CONFIGURATIONS ---
    if (frame_id == 0) {
        dma_channel_config cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, true)); // TX DREQ

        pool_start[3].read_addr      = &_boot_cmd;
        pool_start[3].write_addr     = (void*)i2c_data_cmd_reg;
        pool_start[3].transfer_count = sizeof(_boot_cmd)/sizeof(_boot_cmd[0]);
        pool_start[3].config         = cfg.ctrl;

    } else {
        // Unpack calculations from preceding framework frames
        uint32_t b0 = _rx_buffer[0] & 0xFF;
        uint32_t b1 = _rx_buffer[1] & 0xFF;
        uint32_t b2 = _rx_buffer[2] & 0xFF;
        _raw_lux = b0 | (b1 << 8) | (b2 << 16);

        // Stage 4 (Index 3): Issue TX commands for I2C data extraction clocks
        dma_channel_config cfg_tx = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg_tx, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg_tx, true);
        channel_config_set_write_increment(&cfg_tx, false);
        channel_config_set_dreq(&cfg_tx, i2c_get_dreq(_i2c, true)); // TX DREQ

        pool_start[3].read_addr      = &_read_request;
        pool_start[3].write_addr     = (void*)i2c_data_cmd_reg;
        pool_start[3].transfer_count = sizeof(_read_request)/sizeof(_read_request[0]);
        pool_start[3].config         = cfg_tx.ctrl;

        // Stage 5 (Index 4): Strip values out of the RX FIFO stream
        dma_channel_config cfg_rx = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg_rx, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg_rx, false);
        channel_config_set_write_increment(&cfg_rx, true);
        channel_config_set_dreq(&cfg_rx, i2c_get_dreq(_i2c, false)); // RX DREQ

        pool_start[4].read_addr      = (const void*)i2c_data_cmd_reg;
        pool_start[4].write_addr     = (void*)&_rx_buffer;
        pool_start[4].transfer_count = sizeof(_rx_buffer) / sizeof(_rx_buffer[0]);
        pool_start[4].config         = cfg_rx.ctrl;
    }
}
