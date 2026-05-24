//#include <Wire.h>

IMU::IMU(i2c_inst_t* i2c_hardware) : _i2c(i2c_hardware), _is_booted(0), _rx_fifo_count(0),
        _state_quaternion{ {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} }  {

    memset((void*)_state_quaternion, 0, sizeof(_state_quaternion));
}

void IMU::begin() {
}

uint16_t IMU::get_fifo_sample_count() const{
  return _rx_fifo_count;
}

float IMU::get_celsius() const{
  return _temperature[!_temperature_ping_pong]/16.0;
}

int IMU::getRequiredDescriptorCount(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max) {
    if (subframe_id > 0) return 0;
    
    //if(frame_id<2) return 0; //skip first 20 ms (2 framees at 60 FPS) to allow IMU to boot properly

    // We add 3 additional descriptor operations ahead of the data blocks to modify the I2C block target configuration 
    if (!_is_booted) {
        return 3 + sizeof(_boot_cmd)/sizeof(_boot_cmd[0]) +1;//+4; // 3 Address configs + Boot execution payload blocks
    } else {
        return 17; // 3 Address configs + 1 fill mutable buffer with 0x0100, 1 fill mutable with 0x0300, 1 fill start address
    }
}

void IMU::populateDescriptors(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) {
    if (subframe_id > 0) return;

    dma_channel_config cfg;
    _temperature_ping_pong=frame_id%2;
    uint8_t dma_index=0;

    // Hardware Base Registers mapping pointers
    i2c_hw_t* hw = i2c_get_hw(_i2c);
    volatile void* i2c_enable_reg   = (volatile void*)&hw->enable;
    volatile void* i2c_tar_reg      = (volatile void*)&hw->tar;
    volatile void* i2c_data_cmd_reg = (volatile void*)&hw->data_cmd;
    //volatile void* scratch_reg      = (volatile void*)&watchdog_hw->scratch[0];

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
            pool_start[dma_index].transfer_count = sizeof(_boot_cmd[iter])/sizeof(_boot_cmd[iter][0]);
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;

            if(iter==0)
            {//after reboot command, allow >50us to stabalize
              cfg = dma_channel_get_default_config(data_channel);
              channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
              channel_config_set_read_increment(&cfg, false);
              channel_config_set_write_increment(&cfg, false);
              channel_config_set_chain_to(&cfg, ctrl_channel);
              channel_config_set_enable(&cfg, true);

              static uint8_t dummy_reg_read=0x00;
              static uint8_t dummy_reg_write=0x00;

              pool_start[dma_index].read_addr      = (const void*)&dummy_reg_read;
              pool_start[dma_index].write_addr     = (void*)&dummy_reg_write;
              pool_start[dma_index].transfer_count = 25000; //factor of ~2 margin on reboot time
              pool_start[dma_index].config         = cfg.ctrl;
              dma_index++;

            }

          }
          _is_booted=true;
          //TODO: Also reset address pointers on FIFO
          _rx_buffer_ptr = (uint32_t)_rx_buffer;
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

        //want to know how many samples are in imu buffer, so ask imu...

        // Issue TX commands for I2C data extraction clocks
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, true)); // TX DREQ
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = &_get_fifo_size_cmd;
        pool_start[dma_index].write_addr     = (void*)i2c_data_cmd_reg;
        pool_start[dma_index].transfer_count = sizeof(_get_fifo_size_cmd)/sizeof(_get_fifo_size_cmd[0]);
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        // Strip values out of the RX FIFO stream
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, false)); // RX DREQ
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)i2c_data_cmd_reg;
        pool_start[dma_index].write_addr     = (void*)&_rx_fifo_count;
        //pool_start[dma_index].write_addr     = (void*)&IMU_DMA_SCRATCH_REG; //write to destination that will only keep the lower 12 bits
        pool_start[dma_index].transfer_count = sizeof(_get_fifo_size_cmd)/sizeof(_get_fifo_size_cmd[0])-1; //read one byte: the number of samples availabe in imu fifo
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //need to write SCRATCH_REG as a single operation. so do two uint8_t writes to RAM above, then move uint16_t into scratch reg
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&_rx_fifo_count;
        pool_start[dma_index].write_addr     = (void*)&IMU_DMA_SCRATCH_REG;
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //ok, now have uint16_t size of buffer (12 bits, without upper bits of status flags because scratch reg removed them)

        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&IMU_DMA_SCRATCH_REG;
        pool_start[dma_index].write_addr     = (void*)&_rx_fifo_count;
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //ok, now have true 12-bit count of number of 16-bit readings that need to be collected
        //proceed to setup aux0 to read 

        for(uint8_t iter=0;iter<2;iter++)
        {//because readings are uint16_t, but i2c only reads uint8_t, need to execute this transaction 2 times (doubles the numberof bytes read)
          if(iter==0)
          {
            //start by taking to the FIFO address on the IMU (and mask as a restart after the prior operation to read the number of samples in the buffer)
            cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
            channel_config_set_read_increment(&cfg, false);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, true)); // TX DREQ
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            pool_start[dma_index].read_addr      = (const void*)&_get_fifo_cmd;
            pool_start[dma_index].write_addr     = (void*)i2c_data_cmd_reg;
            pool_start[dma_index].transfer_count = 1;
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;
          }
          //setup aux0 to be the one to stream data from i2c into ram
          //won't know the size of the transfer until runtime
          //1. write the transfer size into the aux0 config
          //2. write the target address within the _rx_buffer (continue where the past dma left off)
          //3. have data_chan write to aux0 config
          if(iter==0)
          {
            //configure data_chan push 0x0100 to i2c periphreal _rx_fifo_count times
            cfg=dma_channel_get_default_config(aux0_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
            channel_config_set_read_increment(&cfg, false);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, true)); 
            //channel_config_set_chain_to(&cfg, ctrl_channel); //aux0 links back to master control
            channel_config_set_enable(&cfg, true);

            //Serial.print("_aux0_write_fifo_cmd.read_addr &_read_operation: "); Serial.println((uint32_t)&_read_operation,HEX);
            //Serial.print("_aux0_write_fifo_cmd.write_addr i2c_data_cmd_reg: "); Serial.println((uint32_t)i2c_data_cmd_reg,HEX);
            _aux0_write_fifo_cmd.read_addr      = (const void*)&_read_operation;
            _aux0_write_fifo_cmd.write_addr     = (void*)i2c_data_cmd_reg;
            //Serial.print("stale tx count _aux0_write_fifo_cmd: "); Serial.println(_aux0_write_fifo_cmd.transfer_count);
            _aux0_write_fifo_cmd.transfer_count = 0; //_rx_fifo_count
            _aux0_write_fifo_cmd.config         = cfg.ctrl;

            //configure aux0_chan to read _rx_fifo_count bytes
            cfg=dma_channel_get_default_config(aux1_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
            channel_config_set_read_increment(&cfg, false);
            channel_config_set_write_increment(&cfg, true);
            channel_config_set_ring(&cfg, true, IMU_BUFFER_SIZE_LOG2);
            channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, false)); 
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            _aux1_read_fifo_cmd.read_addr      = (const void*)i2c_data_cmd_reg;
            _aux1_read_fifo_cmd.write_addr     = (void*)_rx_buffer_ptr;
            //Serial.print("stale tx count _aux1_read_fifo_cmd: "); Serial.println(_aux1_read_fifo_cmd.transfer_count);
            _aux1_read_fifo_cmd.transfer_count = 0; //_rx_fifo_count
            _aux1_read_fifo_cmd.config         = cfg.ctrl;

            //need data_chan to populate data_chan's transfer size
            cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
            channel_config_set_read_increment(&cfg, false);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            pool_start[dma_index].read_addr      = (const void*)&_rx_fifo_count;
            pool_start[dma_index].write_addr     = (void*)&_aux0_write_fifo_cmd.transfer_count;
            pool_start[dma_index].transfer_count = 1;
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;

            //transfer sizes are being set, so works up to this point

            //same for aux0_chan
            cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
            channel_config_set_read_increment(&cfg, false);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            pool_start[dma_index].read_addr      = (const void*)&_rx_fifo_count;
            pool_start[dma_index].write_addr     = (void*)&_aux1_read_fifo_cmd.transfer_count;
            pool_start[dma_index].transfer_count = 1;
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;

            //now that axu0 chan is setup where to write to, and both aux0 and data_chan's know
            //how much data to move, kick off aux0 read, then data_chan write
            //flow control will start with data_chan writing, and aux0 start receiving data
            //when aux0 done, then flow control is sent back to control_channel

            cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
            channel_config_set_read_increment(&cfg, true);
            channel_config_set_write_increment(&cfg, true);
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            pool_start[dma_index].read_addr      = (const void*)&_aux1_read_fifo_cmd;
            pool_start[dma_index].write_addr     = (void*)&dma_hw->ch[aux1_channel].read_addr;
            pool_start[dma_index].transfer_count = 4;
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;

            //with aux0 ready and idling for data, setup data_chan to start pushing to i2c

            cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
            channel_config_set_read_increment(&cfg, true);
            channel_config_set_write_increment(&cfg, true);
            //channel_config_set_chain_to(&cfg, data_channel);
            channel_config_set_enable(&cfg, true);

            //Serial.print("&_aux0_write_fifo_cmd: "); Serial.println((uint32_t)&_aux0_write_fifo_cmd,HEX);
            //Serial.print("&dma_hw->ch[data_channel].read_addr: "); Serial.println((uint32_t)&dma_hw->ch[data_channel].read_addr,HEX);
            pool_start[dma_index].read_addr      = (const void*)&_aux0_write_fifo_cmd;
            pool_start[dma_index].write_addr     = (void*)&dma_hw->ch[aux0_channel].read_addr;
            pool_start[dma_index].transfer_count = 4;
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;

          }else{//re-trigger without changing the address in the output array
            //only need to re-start what has already been done.  note: do not over-write the aux0.write_addr, since this location needs to carry over from the first transfer

            cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
            channel_config_set_read_increment(&cfg, false);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            pool_start[dma_index].read_addr      = (const void*)&_aux1_read_fifo_cmd.config;
            pool_start[dma_index].write_addr     = (void*)&dma_hw->ch[aux1_channel].ctrl_trig;
            pool_start[dma_index].transfer_count = 1;
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;

            //with aux1 ready and idling for data, setup aux0_chan to start pushing to i2c (aux1 will return control flow back to control_dma)

            cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
            channel_config_set_read_increment(&cfg, true);
            channel_config_set_write_increment(&cfg, true);
            //channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            pool_start[dma_index].read_addr      = (const void*)&_aux0_write_fifo_cmd;
            pool_start[dma_index].write_addr     = (void*)&dma_hw->ch[aux0_channel].ctrl_trig;
            pool_start[dma_index].transfer_count = 1;
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;
          }
        }
        //4. dump the aux0 pointer to RAM for the next operation to pickup where this one left off
        //_rx_buffer_ptr
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&dma_hw->ch[aux0_channel].write_addr;
        pool_start[dma_index].write_addr     = (void*)&_rx_buffer_ptr;
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;





        // too complicated to put a stop bit on the end of the varaible-size fifo read, so settle for repeated start, and use that to fetch temperature
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, true)); // TX DREQ
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&_get_temperature_cmd;
        pool_start[dma_index].write_addr     = (void*)i2c_data_cmd_reg;
        pool_start[dma_index].transfer_count = sizeof(_get_temperature_cmd)/sizeof(_get_temperature_cmd[0]);
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        // Strip values out of the RX FIFO stream
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, false)); // RX DREQ
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)i2c_data_cmd_reg;
        pool_start[dma_index].write_addr     = (void*)&_temperature[_temperature_ping_pong];
        pool_start[dma_index].transfer_count = sizeof(_get_temperature_cmd)/sizeof(_get_temperature_cmd[0])-1; //read one byte: the number of samples availabe in imu fifo
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //Serial.print("IMU DMA instruction size: "); Serial.println(dma_index); while(1);

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