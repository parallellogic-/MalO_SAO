//#include <Wire.h>

IMU::IMU(i2c_inst_t* i2c_hardware) : _i2c(i2c_hardware), _is_booted(0), _rx_fifo_count(0),
        _state_quaternion{ {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} }  {

    memset((void*)_state_quaternion, 0, sizeof(_state_quaternion));
}

void IMU::begin() {
}

uint8_t IMU::get_fifo_sample_count() const{
  return _rx_fifo_count;
}

int IMU::getRequiredDescriptorCount(uint32_t frame_id, uint8_t subframe_id, uint8_t subframe_max) {
    if (subframe_id > 0) return 0;
    
    // We add 3 additional descriptor operations ahead of the data blocks to modify the I2C block target configuration 
    if (!_is_booted) {
        return 3 + sizeof(_boot_cmd)/sizeof(_boot_cmd[0]); // 3 Address configs + Boot execution payload blocks
    } else {
        return 3 + 2; // 3 Address configs + 1 fill mutable buffer with 0x0100, 1 fill mutable with 0x0300, 1 fill start address
    }
}

void IMU::populateDescriptors(uint32_t frame_id, uint8_t subframe_id, uint8_t subframe_max, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) {
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
    channel_config_set_chain_to(&cfg_reg, ctrl_channel);
    channel_config_set_enable(&cfg_reg, true);

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
    if (!_is_booted) {
          for(uint8_t iter=0;iter<sizeof(_boot_cmd)/sizeof(_boot_cmd[0]);iter++)
          {
            dma_channel_config cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
            channel_config_set_read_increment(&cfg, true);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, true)); // TX DREQ
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            uint8_t out_index=iter+3;
            pool_start[out_index].read_addr      = &_boot_cmd[iter];
            pool_start[out_index].write_addr     = (void*)i2c_data_cmd_reg;
            pool_start[out_index].transfer_count = sizeof(_boot_cmd[iter])/sizeof(_boot_cmd[iter][0]);
            pool_start[out_index].config         = cfg.ctrl;
          }
          _is_booted=true;
    } else {
        //PRECON: there are >0 samples to read from FIFO (else tries to read 255 bytes from FIFO)
        //read the number of axes in fifo (uint16_t gyro_x, _y, _z, accel_x, _y, _z, gyro_x, _y, _z --> qty 9 (?)) = 1/2 number of bytes to read
        //restructure scater-gather engine: push chain_to to invidual callers, and setting the enable (or not) individually

        //Standard data dma: read fifo_count into dma operation register (RAM).
        //Single Data dma to write the fifo address from ram to periphreal (where to read fifo from).
        //Data dma setup aux 0 to start at address fifo_byte_count and run 255 times. Data dma reads out address byte from aux 1. This is number of bytes without stop bit.
        //Standard data dma to send 1 byte as fifo address to read from. Now setup stream read operation using aux dma:
        //Data dma setup aux0 operation to kickoff i2c write x99 (ram to periphreal). Enable is off.  
        //Data dma setup aux1 op to kickoff i2c read x99 (peripreal to ram). No start. Chain_to control dma
        //Data dma kickoff aux0 and aux1. Do not chain to command dma

        uint8_t dma_index=3;
        //want to know how many samples are in imu buffer, so ask imu...

        // Issue TX commands for I2C data extraction clocks
        dma_channel_config cfg_tx = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg_tx, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg_tx, true);
        channel_config_set_write_increment(&cfg_tx, false);
        channel_config_set_dreq(&cfg_tx, i2c_get_dreq(_i2c, true)); // TX DREQ
        channel_config_set_chain_to(&cfg_tx, ctrl_channel);
        channel_config_set_enable(&cfg_tx, true);

        pool_start[dma_index].read_addr      = &_get_fifo_size_cmd;
        pool_start[dma_index].write_addr     = (void*)i2c_data_cmd_reg;
        pool_start[dma_index].transfer_count = sizeof(_get_fifo_size_cmd)/sizeof(_get_fifo_size_cmd[0]);
        pool_start[dma_index].config         = cfg_tx.ctrl;
        dma_index++;

        // Strip values out of the RX FIFO stream
        dma_channel_config cfg_rx = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg_rx, DMA_SIZE_8);
        channel_config_set_read_increment(&cfg_rx, false);
        channel_config_set_write_increment(&cfg_rx, false);
        channel_config_set_dreq(&cfg_rx, i2c_get_dreq(_i2c, false)); // RX DREQ
        channel_config_set_chain_to(&cfg_rx, ctrl_channel);
        channel_config_set_enable(&cfg_rx, true);

        pool_start[dma_index].read_addr      = (const void*)i2c_data_cmd_reg;
        pool_start[dma_index].write_addr     = (void*)&_rx_fifo_count;
        pool_start[dma_index].transfer_count = 1; //read one byte: the number of samples availabe in imu fifo
        pool_start[dma_index].config         = cfg_rx.ctrl;
        dma_index++;

        /*dma_channel_config cfg_data;

        // -- setup aux0 register values for aux0 to write values to i2c.  setup data_chan to be the one to populate aux0 --

        dma_channel_config cfg__aux0_fifo_size_write = dma_channel_get_default_config(aux0_channel);
        channel_config_set_transfer_data_size(&cfg__aux0_fifo_size_write, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg__aux0_fifo_size_write, true);
        channel_config_set_write_increment(&cfg__aux0_fifo_size_write, false);
        channel_config_set_dreq(&cfg__aux0_fifo_size_write, i2c_get_dreq(_i2c, true)); // TX DREQ
        //channel_config_set_chain_to(&cfg__aux0_fifo_size_write, ctrl_channel);
        channel_config_set_chain_to(&cfg__aux0_fifo_size_write, aux1_channel);
        channel_config_set_enable(&cfg__aux0_fifo_size_write, false); //aux0 and aux1 will be kicked off syncronously by data_chan

        _aux0_fifo_size_write.read_addr      = &_get_fifo_size_cmd;
        _aux0_fifo_size_write.write_addr     = (void*)i2c_data_cmd_reg;
        _aux0_fifo_size_write.transfer_count = sizeof(_get_fifo_size_cmd)/sizeof(_get_fifo_size_cmd[0]);
        _aux0_fifo_size_write.config         = cfg__aux0_fifo_size_write.ctrl;

        cfg_data = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg_data, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg_data, true);
        channel_config_set_write_increment(&cfg_data, true);
        //channel_config_set_dreq(&cfg_data, i2c_get_dreq(_i2c, true)); // TX DREQ
        channel_config_set_chain_to(&cfg_data, ctrl_channel);
        channel_config_set_enable(&cfg_data, true);

        pool_start[dma_index].read_addr      = (const void*)&_aux0_fifo_size_write;
        pool_start[dma_index].write_addr     = (void*)&dma_hw->ch[aux0_channel].read_addr;
        pool_start[dma_index].transfer_count = 4;
        pool_start[dma_index].config         = cfg_data.ctrl;
        dma_index++;

        // -- setup aux1 register values for aux1 to read value from i2c.  setup data_chan to be the one to populate aux1 --

        dma_channel_config cfg__aux1_fifo_size_read = dma_channel_get_default_config(aux1_channel);
        channel_config_set_transfer_data_size(&cfg__aux1_fifo_size_read, DMA_SIZE_8);
        channel_config_set_read_increment(&cfg__aux1_fifo_size_read, true);
        channel_config_set_write_increment(&cfg__aux1_fifo_size_read, false);
        channel_config_set_dreq(&cfg__aux1_fifo_size_read, i2c_get_dreq(_i2c, false)); // RX DREQ
        channel_config_set_chain_to(&cfg__aux1_fifo_size_read, ctrl_channel); //read operation is the one that passes control back to ctrl_chan
        channel_config_set_enable(&cfg__aux1_fifo_size_read, false); //aux0 and aux1 will be kicked off syncronously by data_chan

        _aux1_fifo_size_read.read_addr      = (const void*)i2c_data_cmd_reg;
        _aux1_fifo_size_read.write_addr     = (void*)&_rx_fifo_count;
        _aux1_fifo_size_read.transfer_count = 1;
        _aux1_fifo_size_read.config         = cfg__aux1_fifo_size_read.ctrl;

        cfg_data = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg_data, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg_data, true);
        channel_config_set_write_increment(&cfg_data, true);
        //channel_config_set_dreq(&cfg_data, i2c_get_dreq(_i2c, true)); // TX DREQ
        channel_config_set_chain_to(&cfg_data, ctrl_channel);
        channel_config_set_enable(&cfg_data, true);

        pool_start[dma_index].read_addr      = (const void*)&_aux1_fifo_size_read;
        pool_start[dma_index].write_addr     = (void*)&dma_hw->ch[aux1_channel].read_addr;
        pool_start[dma_index].transfer_count = 4;
        pool_start[dma_index].config         = cfg_data.ctrl;
        dma_index++;

        // -- setup data_chan to kick-off both aux0 and aux1--

        _aux0_aux1_trigger_mask=(1u << aux0_channel);// | (1u << aux1_channel);

        cfg_data = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg_data, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg_data, false);
        channel_config_set_write_increment(&cfg_data, false);
        //channel_config_set_chain_to(&cfg_data, ctrl_channel); //aux1 is the one that passes chain back to ctrl_chan
        channel_config_set_enable(&cfg_data, true);

        pool_start[dma_index].read_addr      = &_aux0_aux1_trigger_mask;
        pool_start[dma_index].write_addr     = (void*)&dma_hw->multi_channel_trigger;
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg_data.ctrl;
        dma_index++;*/


        //_rx_fifo_count now has "18" (for example) to mean there are 3x gyro readings (3x axes each), and 3x accel readings (3x axes each) in periphreal buffer (each reading is 16 bits - requires 2x i2c reads to fetch)

        //now need to read that many samples (16-bit values), including an I2C termination character
        //take the _rx_fifo_count, decrement by 1, save in _rx_fifo_count_decrement
        //do this by indexing into _rx_buffer at index _rx_fifo_count, moving backward one read, then pulling out the decremented index into _rx_fifo_count_decrement





        //now that _rx_fifo_count and _rx_fifo_count_decrement are set, need to perform parallel dma operation: one to write i2c

    }
}


/*
void loop() {
  // Read FIFO sample count from STATUS1 & STATUS2 (12 bits total)
  uint8_t status1 = readRegister(REG_FIFO_STATUS1);
  uint8_t status2 = readRegister(REG_FIFO_STATUS2);
  uint16_t fifo_samples = (status1) | ((status2 & 0x07) << 8);

  // Each sample (Accel only) consists of 6 bytes (X_L, X_H, Y_L, Y_H, Z_L, Z_H)
  if (fifo_samples > 0) {
    uint16_t bytes_to_read = fifo_samples * 6;
    uint8_t buffer[bytes_to_read];

    // Burst read the entire FIFO payload
    readRegisters(REG_FIFO_DATA_OUT_L, buffer, bytes_to_read);

    // Process each 6-byte sample
    for (uint16_t i = 0; i < fifo_samples; i++) {
      uint8_t idx = i * 6;
      int16_t x = (int16_t)(buffer[idx + 1] << 8 | buffer[idx]);
      int16_t y = (int16_t)(buffer[idx + 3] << 8 | buffer[idx + 2]);
      int16_t z = (int16_t)(buffer[idx + 5] << 8 | buffer[idx + 4]);

      // Convert to 'g' (16-bit signed integer / 16384 for ±2g)
      float ax = x / 16384.0;
      float ay = y / 16384.0;
      float az = z / 16384.0;

      Serial.print("Accel (g) - X: ");
      Serial.print(ax);
      Serial.print(", Y: ");
      Serial.print(ay);
      Serial.print(", Z: ");
      Serial.println(az);
    }
  }

  // Adjust polling rate to prevent flooding the serial output
  delay(10); 
}

// Low-level I/O Helper Functions
uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(LSM6DS_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(LSM6DS_ADDR, 1);
  return Wire.read();
}

void readRegisters(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(LSM6DS_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(LSM6DS_ADDR, len);
  for (uint16_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
}

void writeRegister(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(LSM6DS_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}
*/