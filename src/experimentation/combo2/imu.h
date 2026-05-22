#ifndef IMU_H
#define IMU_H

// LSM6DS3TR I2C Configuration
#define LSM6DS_ADDR 0x6B // Default I2C address (SA0 grounded)
#define REG_WHO_AM_I 0x0F
#define REG_CTRL1_XL 0x10
#define REG_CTRL3_C 0x12
#define REG_FIFO_CTRL3 0x09
#define REG_FIFO_CTRL5 0x0A
#define REG_FIFO_STATUS1 0x3A
#define REG_FIFO_STATUS2 0x3B
#define REG_FIFO_DATA_OUT_L 0x3E

#define IMU_BUFFER_SIZE (256 * 8) // 2048 elements (uint16_t) //gyro and accel data: 2 bytes gyro_x, _y, _z, accel_x, _y, _z
#define IMU_BUFFER_SIZE_BYTES (IMU_BUFFER_SIZE * sizeof(uint16_t))


class IMU : public IMultiDmaTransactionSource {
private:
    i2c_inst_t* _i2c;
    bool _is_booted=false;

    float _state_quaternion[2][4];
    
    //to be verified: The I2C hardware macro block cannot change its target address (IC_TAR) while the peripheral is actively enabled. Attempting to write to IC_TAR via DMA while the block is active will cause the hardware to silently ignore the transaction.
    uint32_t _i2c_disable       __attribute__((aligned(4))) = 0;
    uint32_t _i2c_target_addr   __attribute__((aligned(4))) = LSM6DS_ADDR;
    uint32_t _i2c_enable        __attribute__((aligned(4))) = 1;
    volatile bool _is_dma_done=false;

    // list of register-value pairs to write to i2c periphreal on boot
    const uint16_t _boot_cmd[4][2] __attribute__((aligned(4))) ={
        // 1. Reset device
      {REG_CTRL3_C,0x01},
        // 2. Set Accel ODR to 104 Hz (>60 Hz) and Anti-Aliasing filter to 50 Hz
        // CTRL1_XL: ODR[7:4] = 0101 (104 Hz), FS[3:2] = 00 (±2g), LPF2[1] = 0 (Filter BW = ODR/9)
      {REG_CTRL1_XL,0x50},
        // 3. Configure FIFO Decimation
        // FIFO_CTRL3: Accel decimation = 1 (1 sample), Gyro decimation = 0 (Disabled)
      {REG_FIFO_CTRL3,0x01},
        // 4. Set FIFO Mode to "Continuous Mode" (overwrites old data if full)
        // FIFO_CTRL5: FIFO_Mode[2:0] = 110 (Continuous Mode)
      {REG_FIFO_CTRL5,0x06 | 0x0200},// 0x0200 (I2C STOP bit)
    };

    // list of register-value pairs to write to i2c periphreal on boot
    /*const uint16_t _boot_cmd[2][2] __attribute__((aligned(4))) ={
      {0x10,0x40},
      {0x11,0x40},
    };*/

    const uint16_t _get_fifo_size_cmd[2] __attribute__((aligned(4))) ={
      REG_WHO_AM_I/*REG_FIFO_STATUS1*/, 0x0300// 0x0200 (I2C STOP bit)
    };
    
    const uint32_t _read_operation = 0x0100; //read one uin8_t from FIFO
    const uint32_t _read_operation_stop = 0x0300; //read uin8_t from FIFO and stop

    // Target buffer for incoming RX FIFO data (doubles as the read request - in-place morphing buffer)
    volatile uint8_t _rx_fifo_count; //number of unread words (16-bit axes) stored in FIFO (qty 6 is one accel+gyro reading)
    volatile uint8_t _rx_fifo_count_decrement;//because I2C needs to end with 
    volatile uint16_t _rx_buffer[IMU_BUFFER_SIZE] __attribute__((aligned(IMU_BUFFER_SIZE_BYTES))); 

    volatile DmaDescriptor _aux0_fifo_size_write;
    volatile DmaDescriptor _aux1_fifo_size_read;
    uint32_t _aux0_aux1_trigger_mask;

public:
    IMU(i2c_inst_t* i2c_hardware = i2c0);
    
    void begin();

    uint8_t get_fifo_sample_count() const; //number of samples (axes) the imu periphreal reports in its memory
    float get_accel(uint8_t xyz) const; //m/s, xyz: 0=x, 1=y, 2=z
    float get_gyro(uint8_t xyz) const; //deg/sec, xyz: 0=x, 1=y, 2=z
    float get_quaternion(uint8_t wxyz) const; //wxyz: 0-3 as index

    // IMultiDmaTransactionSource Interface
    int getRequiredDescriptorCount(uint32_t frame_id, uint8_t subframe_id, uint8_t subframe_max) override;
    void populateDescriptors(uint32_t frame_id, uint8_t subframe_id, uint8_t subframe_max, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) override;

};


#endif