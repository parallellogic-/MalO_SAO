
//FUTURE: refactor the sniff --> pio_addr to avoid using limited resource (better to keep free for checksum verification)

#include "imu.h"

IMU::IMU(i2c_inst_t* i2c_hardware) : _i2c(i2c_hardware)  {

    //memset((void*)_state_quaternion, 0, sizeof(_state_quaternion));
}

void IMU::begin() {
}
void IMU::end() {
}

uint16_t IMU::get_fifo_sample_count() const{
  return _rx_fifo_count;
}

float IMU::get_celsius() const{
  return _temperature[!_imu_ping_pong]/16.0;
}

int IMU::getRequiredDescriptorCount(uint64_t frame_id) {
    
    //if(frame_id<2) return 0; //skip first 20 ms (2 framees at 60 FPS) to allow IMU to boot properly

    // We add 3 additional descriptor operations ahead of the data blocks to modify the I2C block target configuration 
    if (!_is_booted) {
        return 3 + sizeof(_boot_cmd)/sizeof(_boot_cmd[0]) +4;//+4; // 3 Address configs + Boot execution payload blocks
    } else {
        return 20; // 3 Address configs + 1 fill mutable buffer with 0x0100, 1 fill mutable with 0x0300, 1 fill start address
    }
}

void IMU::populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) {
    
    dma_channel_config cfg;
    _imu_ping_pong=frame_id%2;//_temperature_ping_pong
    uint8_t dma_index=0;

    static uint32_t dummy_reg_read=0x00;
    static uint32_t dummy_reg_write=0x00;

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

            if(iter==1)
            {//precon: command 0 is a reboot, command 1 is a read - after read then wait for reboot to finish
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
              pool_start[dma_index].transfer_count = 1;
              pool_start[dma_index].config         = cfg.ctrl;
              dma_index++;

              //after reboot command, allow >50us to stabalize
              cfg = dma_channel_get_default_config(data_channel);
              channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
              channel_config_set_read_increment(&cfg, false);
              channel_config_set_write_increment(&cfg, false);
              channel_config_set_chain_to(&cfg, ctrl_channel);
              channel_config_set_enable(&cfg, true);

              pool_start[dma_index].read_addr      = (const void*)&dummy_reg_read;
              pool_start[dma_index].write_addr     = (void*)&dummy_reg_write;
              pool_start[dma_index].transfer_count = 25000; //factor of ~2 margin on reboot time
              pool_start[dma_index].config         = cfg.ctrl;
              dma_index++;
            }
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

          //read will hang dma until byte is read from i2c->ram
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

          //Serial.print("DMA instruction size: "); Serial.println(dma_index); while(1);
          //Also reset address pointer on FIFO
          _aux1_fifo_i2c_to_ram_cmd.write_addr=(void*)&_rx_buffer[0];
          _update_from_index=0;
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

        //ok, now have _rx_fifo_count = 12-bit count of number of uint16_t readings that need to be collected
        //need to compute _rx_fifo_count_x2 (to convert uint16_t sample count to uint8_ byte read count)
        //also _4x for i2c_cmd which takes 4 bytes to write to fetch one sample (LSB, read, MSB, read)

        //setup sniff to add values passed through it from aux0, init sniff to 0 sum
        static struct SniffDescriptor sniff_config;
        sniff_config.control = 
        (DMA_SNIFF_CTRL_CALC_VALUE_SUM << DMA_SNIFF_CTRL_CALC_LSB) |
        (aux0_channel << DMA_SNIFF_CTRL_DMACH_LSB) | //going to feed _rx_fifo_count through aux0 a few times later to actuate the sum operation
        DMA_SNIFF_CTRL_EN_BITS;

        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&sniff_config;
        pool_start[dma_index].write_addr     = (void*)&dma_hw->sniff_ctrl; //DMA_BASE + DMA_SNIFF_DATA_OFFSET;
        pool_start[dma_index].transfer_count = 2;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //prepare aux0 adder
        cfg=dma_channel_get_default_config(aux0_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_sniff_enable(&cfg, true); 
        channel_config_set_enable(&cfg, true);

        _aux0_sum_cmd.read_addr      = (const void*)&_rx_fifo_count;
        _aux0_sum_cmd.write_addr     = (void *)&dummy_reg_write;
        _aux0_sum_cmd.transfer_count = 2; //_rx_fifo_count
        _aux0_sum_cmd.config         = cfg.ctrl;

        //data_chan triggers aux0
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, true);
        //channel_config_set_chain_to(&cfg, ctrl_channel); //aux0 loops back flow control to ctrl_chan
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&_aux0_sum_cmd;
        pool_start[dma_index].write_addr     = (void*)&dma_hw->ch[aux0_channel].read_addr;
        pool_start[dma_index].transfer_count = 4;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;
        
        //now read value out of sniff
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&dma_hw->sniff_data;
        pool_start[dma_index].write_addr     = (void*)&_rx_fifo_count_x2; //DMA_BASE + DMA_SNIFF_DATA_OFFSET;
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //data_chan triggers aux0 (double the value)
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, true);
        //channel_config_set_chain_to(&cfg, ctrl_channel); //aux0 loops back flow control to ctrl_chan
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&_aux0_sum_cmd;
        pool_start[dma_index].write_addr     = (void*)&dma_hw->ch[aux0_channel].read_addr;
        pool_start[dma_index].transfer_count = 4;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //read the value out of sniff for x4
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&dma_hw->sniff_data;
        pool_start[dma_index].write_addr     = (void*)&_rx_fifo_count_x4; //DMA_BASE + DMA_SNIFF_DATA_OFFSET;
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //Serial.print("_rx_fifo_count_x2: "); Serial.print(_rx_fifo_count_x2);
        //Serial.print(", _rx_fifo_count_x4: "); Serial.println(_rx_fifo_count_x4);

        //now that 2x and 4x have been computed, apply them to i2c transfer...

        //prepare the RAM->i2c transfer
        cfg=dma_channel_get_default_config(aux0_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_ring(&cfg, false, __builtin_ctz(sizeof(_get_fifo_cmd)));
        channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, true)); 
        //channel_config_set_chain_to(&cfg, ctrl_channel); //i2c->RAM operation is the one that links back to ctrl_chan
        channel_config_set_enable(&cfg, true);

        _aux0_fifo_ram_to_i2c_cmd.read_addr      = (const void*)&_get_fifo_cmd;
        _aux0_fifo_ram_to_i2c_cmd.write_addr     = (void*)i2c_data_cmd_reg;
        _aux0_fifo_ram_to_i2c_cmd.transfer_count = 0; //_rx_fifo_count
        _aux0_fifo_ram_to_i2c_cmd.config         = cfg.ctrl;
            
        //prepare i2c->RAM transfer
        cfg=dma_channel_get_default_config(aux1_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_ring(&cfg, true, __builtin_ctz(sizeof(_rx_buffer)));
        channel_config_set_dreq(&cfg, i2c_get_dreq(_i2c, false)); 
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        _aux1_fifo_i2c_to_ram_cmd.read_addr      = (const void*)i2c_data_cmd_reg;
        //_aux1_fifo_i2c_to_ram_cmd.write_addr     //set on reboot to start of list, updated after every dma transfer
        _aux1_fifo_i2c_to_ram_cmd.transfer_count = 0; //_rx_fifo_count
        _aux1_fifo_i2c_to_ram_cmd.config         = cfg.ctrl;

        //update _rx_fifo_count into aux0 transfer_count
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&_rx_fifo_count_x4;
        pool_start[dma_index].write_addr     = (void*)&_aux0_fifo_ram_to_i2c_cmd.transfer_count;
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //update _rx_fifo_count into aux0 transfer_count
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&_rx_fifo_count_x2;
        pool_start[dma_index].write_addr     = (void*)&_aux1_fifo_i2c_to_ram_cmd.transfer_count;
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //kick-off aux1 i2c->ram first so it idles while waiting for aux0 to start
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&_aux1_fifo_i2c_to_ram_cmd;
        pool_start[dma_index].write_addr     = (void*)&dma_hw->ch[aux1_channel].read_addr;
        pool_start[dma_index].transfer_count = 4;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;
            
        //kick-off aux0 ram->i2c.  beware aux1 completing immedaitely and moving to ctrl_chan, while data_chan is writing to aux0
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, true);
        //channel_config_set_chain_to(&cfg, ctrl_channel); //aux1 will return flow control back to ctrl_chan
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&_aux0_fifo_ram_to_i2c_cmd;
        pool_start[dma_index].write_addr     = (void*)&dma_hw->ch[aux0_channel].read_addr;
        pool_start[dma_index].transfer_count = 4;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //control flow only resumes here once aux1 completes and kicks back to ctrl_chan

        //dangling command to update _aux1_fifo_i2c_to_ram_cmd.write_addr
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&dma_hw->ch[aux1_channel].write_addr;
        pool_start[dma_index].write_addr     = (void*)&_aux1_fifo_i2c_to_ram_cmd.write_addr;
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        // too complicated to put a stop bit on the end of the variable-size fifo read, so settle for repeated start, and use that to fetch temperature
        // would need to alter sniff by writing 0xFFFFFFFF to subtract 1 from bytes sent, and then add an extra 0x0300 at end...
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
        pool_start[dma_index].write_addr     = (void*)&_temperature[_imu_ping_pong];
        pool_start[dma_index].transfer_count = sizeof(_get_temperature_cmd)/sizeof(_get_temperature_cmd[0])-1; //read one byte: the number of samples availabe in imu fifo
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        _is_data_ready=0;
        //assert data is ready flag when dma operations are complete
        const static bool is_data_ready=1;
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&is_data_ready;
        pool_start[dma_index].write_addr     = (void*)&_is_data_ready;
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //Serial.print("DMA instruction size: "); Serial.println(dma_index); while(1);

    }
}

//do math on imu readings
bool IMU::update()
{
  if(!_is_data_ready) return false;
  //there are samples in the buffer to be processed

  //find the location where the DMA wants to write to on the next frame (serves as the endpoint of the update list)
  uint16_t update_to_index=((uint32_t)_aux1_fifo_i2c_to_ram_cmd.write_addr-(uint32_t)&_rx_buffer)/sizeof(_rx_buffer[0]);

  //be aware of wrap around in circular buffer
  //Serial.printf("loop 1: %d\n",update_to_index);
  if(update_to_index<_update_from_index) update_to_index+=IMU_BUFFER_SIZE;
  //Serial.printf("_update_from_index: %d, update_to_index: %d\n",_update_from_index,update_to_index);
  uint8_t sample_count=0;
  int32_t imu_sample[6]={};
  while((_update_from_index+6)<=update_to_index)
  {//for every 6-axis sample: x_ y_ z_ gyro, x_, y_, z_ accel
    for(uint8_t idx=0;idx<6;idx++) imu_sample[idx]+=_rx_buffer[(_update_from_index+idx)%IMU_BUFFER_SIZE];
    sample_count++;
    

    int16_t gyro_x=_rx_buffer[(_update_from_index+0)%IMU_BUFFER_SIZE];//units are in counts, ref _boot_cmd for gyro/accel gain settings to map to deg/sec, g's
    int16_t gyro_y=_rx_buffer[(_update_from_index+1)%IMU_BUFFER_SIZE];
    int16_t gyro_z=_rx_buffer[(_update_from_index+2)%IMU_BUFFER_SIZE];
    int16_t accl_x=_rx_buffer[(_update_from_index+3)%IMU_BUFFER_SIZE];
    int16_t accl_y=_rx_buffer[(_update_from_index+4)%IMU_BUFFER_SIZE];
    int16_t accl_z=_rx_buffer[(_update_from_index+5)%IMU_BUFFER_SIZE];
    //Serial.printf("gyro, %d, %d, %d, accel, %d, %d, %d, %d, %08X\n",gyro_x,gyro_y,gyro_z,accl_x,accl_y,accl_z,sample_count,(uint32_t)_aux1_fifo_i2c_to_ram_cmd.write_addr);
    /*Serial.printf("gyro: %.2f, %.2f, %.2f deg/sec, accel: %.2f, %.2f, %.2f g's\n",
      gyro_x*GYRO_DEG_SEC_PER_LSB,
      gyro_y*GYRO_DEG_SEC_PER_LSB,
      gyro_z*GYRO_DEG_SEC_PER_LSB,
      accl_x*ACCEL_RANGE_G_PER_LSB,
      accl_y*ACCEL_RANGE_G_PER_LSB,
      accl_z*ACCEL_RANGE_G_PER_LSB);*/

    
    _update_from_index+=6;
  }
  for(uint8_t idx=0;idx<6;idx++){
      _gyro_accel_reading[_imu_ping_pong][idx]=(sample_count==0)?_gyro_accel_reading[!_imu_ping_pong][idx]:
        (imu_sample[idx]*(idx<3?GYRO_DEG_SEC_PER_LSB:ACCEL_RANGE_G_PER_LSB)/(float)sample_count); //convert raw gryo/accel counts to an average in engineering units
        //if no data, use stale data (rather than snapping around at zeros)
  }
  
  //_imu_ping_pong=!_imu_ping_pong;

  //Serial.println("loop 2");
  while(update_to_index>=IMU_BUFFER_SIZE) update_to_index-=IMU_BUFFER_SIZE;
  _update_from_index=update_to_index;

  //hold off on updating the ping-pong buffer until the frame boundary to avoid shearing state estaimte mid-frame

  _is_data_ready=false;//clear data-ready flag
  return true;
}

float IMU::get_accel(uint8_t xyz) const //m/s, xyz: 0=x, 1=y, 2=z
{
  if(xyz>3) return 0;
  return _gyro_accel_reading[!_imu_ping_pong][xyz+3];
}
float IMU::get_gyro(uint8_t xyz) const //deg/sec, xyz: 0=x, 1=y, 2=z
{
  if(xyz>3) return 0;
  return _gyro_accel_reading[!_imu_ping_pong][xyz];
}