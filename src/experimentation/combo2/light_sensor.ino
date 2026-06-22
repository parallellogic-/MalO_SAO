
#include "light_sensor.h"
#include "hardware/i2c.h"

static uint32_t scratch=0;

LightSensor::LightSensor(i2c_inst_t* i2c_hardware) : _i2c(i2c_hardware)
    {
    /*_boot_cmd[0] = LTR308_MAIN_CTRL;
    _boot_cmd[1] = 0x02 ;//| 0x0200; // Active mode, Gain = 1 //0202

    // Add these configuration variables inside your LightSensor constructor:
    _boot_cmd2[0] = 0x04;         // Targets the ALS_GAIN / Integration Time Register
    _boot_cmd2[1] = 0x40 | 0x0200; // 0x04 = 10ms integration time + 0x0200 (I2C STOP bit!)*/

    // Construct the read sequence commands
    /*_read_request[0] = LTR308_ALS_DATA_0; // Point I2C to start reading at Data 0
    _read_request[1] = 0x0100;            // Command a Read byte (Bit 8 is CMD_READ)
    _read_request[2] = 0x0100;            // Command a Read byte
    _read_request[3] = 0x0300;            // Command a Read byte //0300*/
    
    //memset((void*)_rx_buffer, 0, sizeof(_rx_buffer));
}

void LightSensor::begin() {
}
void LightSensor::end() {
}

//24-bit reading
uint32_t LightSensor::getBrightness() const {
    return _raw_lux[!_raw_lux_ping_pong];
}

int LightSensor::getRequiredDescriptorCount(uint64_t frame_id) {
    
    // We add 3 additional descriptor operations ahead of the data blocks to modify the I2C block target configuration 
    if (!_is_booted) {
        return 3 + sizeof(_boot_cmd)/sizeof(_boot_cmd[0])+1+1; // 3 Address configs + 1 Boot execution payload block
    } else {
        return 3 + 2; // 3 Address configs + 1 TX request block + 1 RX fetch block
    }
}

void LightSensor::populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) {
    
    _raw_lux_ping_pong=frame_id%2;
    uint8_t dma_index=0;
    dma_channel_config cfg;

    // Hardware Base Registers mapping pointers
    i2c_hw_t* hw = i2c_get_hw(_i2c);
    volatile void* i2c_enable_reg   = (volatile void*)&hw->enable;
    volatile void* i2c_tar_reg      = (volatile void*)&hw->tar;
    volatile void* i2c_data_cmd_reg = (volatile void*)&hw->data_cmd;

    // --- BASE DMA REGISTER METADATA FOR HARDWARE OVERRIDES ---
    // These run without a Data Request (DREQ) pacing flag because internal hardware register updates complete instantly
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    // Stage 1: Disable I2C Engine (Required to update TAR)
    pool_start[dma_index].read_addr      = &_i2c_disable;
    pool_start[dma_index].write_addr     = (void*)i2c_enable_reg;
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    // Stage 2: Inject Target Slave Device Address into I2C Core
    pool_start[dma_index].read_addr      = &_i2c_target_addr;
    pool_start[dma_index].write_addr     = (void*)i2c_tar_reg;
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    // Stage 3: Re-Enable I2C Engine 
    pool_start[dma_index].read_addr      = &_i2c_enable;
    pool_start[dma_index].write_addr     = (void*)i2c_enable_reg;
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    // --- DATA PAYLOAD TRANSMISSION CONFIGURATIONS ---
    if (!_is_booted) {
      for(uint8_t iter=0;iter<sizeof(_boot_cmd)/sizeof(_boot_cmd[0]);iter++)
      {
          cfg = dma_channel_get_default_config(data_channel);
          channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
          channel_config_set_read_increment(&cfg, true);
          channel_config_set_write_increment(&cfg, false);
          channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, true)); // TX DREQ
          channel_config_set_chain_to(&cfg, ctrl_channel);
          channel_config_set_enable(&cfg, true);

          pool_start[dma_index].read_addr      = &_boot_cmd[iter];
          pool_start[dma_index].write_addr     = (void*)i2c_data_cmd_reg;
          pool_start[dma_index].transfer_count = sizeof(_boot_cmd[iter])/sizeof(_boot_cmd[0][0]);
          pool_start[dma_index].config         = cfg.ctrl;
          dma_index++;
      }

      //append a final read operation, so that the dma stalls until it is complete, allowing fifo buffer to empty and allow clean swap between i2c targets
      cfg = dma_channel_get_default_config(data_channel);
      channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
      channel_config_set_read_increment(&cfg, true);
      channel_config_set_write_increment(&cfg, false);
      channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, true)); // TX DREQ
      channel_config_set_chain_to(&cfg, ctrl_channel);
      channel_config_set_enable(&cfg, true);

      pool_start[dma_index].read_addr      = &_boot_check_cmd;
      pool_start[dma_index].write_addr     = (void*)i2c_data_cmd_reg;
      pool_start[dma_index].transfer_count = sizeof(_boot_check_cmd)/sizeof(_boot_check_cmd[0]);
      pool_start[dma_index].config         = cfg.ctrl;
      dma_index++;

      //read will hang dma until byte is read from i2d->ram
      cfg = dma_channel_get_default_config(data_channel);
      channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
      channel_config_set_read_increment(&cfg, false);
      channel_config_set_write_increment(&cfg, false);
      channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, false)); // RX DREQ
      channel_config_set_chain_to(&cfg, ctrl_channel);
      channel_config_set_enable(&cfg, true);

      pool_start[dma_index].read_addr      = (const void*)i2c_data_cmd_reg;
      pool_start[dma_index].write_addr     = (void*)&_boot_check;
      pool_start[dma_index].transfer_count = sizeof(_boot_check_cmd)/sizeof(_boot_check_cmd[0])-1;
      pool_start[dma_index].config         = cfg.ctrl;
      dma_index++;
          
           /*cfg = dma_channel_get_default_config(data_channel);
          channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
          channel_config_set_read_increment(&cfg, true);
          channel_config_set_write_increment(&cfg, false);
          channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, true)); // TX DREQ
          channel_config_set_chain_to(&cfg, ctrl_channel);
          channel_config_set_enable(&cfg, true);

          pool_start[4].read_addr      = &_boot_cmd2;
          pool_start[4].write_addr     = (void*)i2c_data_cmd_reg;
          pool_start[4].transfer_count = sizeof(_boot_cmd2)/sizeof(_boot_cmd2[0]);
          pool_start[4].config         = cfg.ctrl;*/
          _is_booted=true;
          //Serial.print("IMU DMA instruction size: "); Serial.println(dma_index); while(1);
    } else {
        // Unpack calculations from preceding framework frames

        // Stage 4 (Index 3): Issue TX commands for I2C data extraction clocks
        //PRECON: transaction is sort <~16 values and fits within I2C FIFO
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, true)); // TX DREQ
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = &_read_request;
        pool_start[dma_index].write_addr     = (void*)i2c_data_cmd_reg;
        pool_start[dma_index].transfer_count = sizeof(_read_request)/sizeof(_read_request[0]);
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        // Stage 5 (Index 4): Strip values out of the RX FIFO stream
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, false)); // RX DREQ
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)i2c_data_cmd_reg;
        pool_start[dma_index].write_addr     = (void*)&_raw_lux[_raw_lux_ping_pong];
        pool_start[dma_index].transfer_count = sizeof(_read_request)/sizeof(_read_request[0])-1; // same number of bytes, minus the address byte
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //Serial.print("DMA instruction size: "); Serial.println(dma_index); while(1);
    }              

    // -- dead reckoning delay to allow this i2c fifo transaction to clear before moving to the next transaction --
    /*dma_channel_config cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    static uint8_t dummy_reg_read=0x00;
    static uint8_t dummy_reg_write=0x00;

    pool_start[dma_index].read_addr      = (const void*)&dummy_reg_read;
    pool_start[dma_index].write_addr     = (void*)&dummy_reg_write;
    pool_start[dma_index].transfer_count = 25000; 
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;*/
}
