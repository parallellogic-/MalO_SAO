#ifndef IMU_H
#define IMU_H

// LSM6DS3TR I2C Configuration
#define LSM6DS_ADDR 0x6B //0xd6/0x67 for W/R

#define REG_WHO_AM_I 0x0F
#define REG_CTRL1_XL 0x10
#define REG_CTRL2_G 0x11
#define REG_CTRL3_C 0x12
#define REG_FIFO_CTRL3 0x08
//#define REG_FIFO_CTRL4 0x09
#define REG_FIFO_CTRL5 0x0A
#define REG_FIFO_STATUS1 0x3A
#define REG_FIFO_STATUS2 0x3B
#define REG_FIFO_DATA_OUT_L 0x3E
#define REG_FIFO_DATA_OUT_H 0x3F

#define IMU_BUFFER_SIZE_LOG2 12 //ring buffer size
#define IMU_BUFFER_SIZE (1<<IMU_BUFFER_SIZE_LOG2) // 2048 elements (uint16_t) //gyro and accel data: 2 bytes gyro_x, _y, _z, accel_x, _y, _z
#define IMU_BUFFER_SIZE_BYTES (IMU_BUFFER_SIZE * sizeof(uint16_t))

#define IMU_DMA_SCRATCH_REG pwm_hw->slice[11].div //need somewhere to write the 12-bit fifo size to where it can be operated on (set/clear/xor).  pwm/div has the bonus of have only 12 bits viable anyway - automatically filtering out the upper 4 bits without a separate operation

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

    //I2C: IC_DATA_CMD Register
    //The RESTART is only required when changing direction from a write to a read.

    // list of register-value pairs to write to i2c periphreal on boot
    const uint16_t _boot_cmd[6][2] __attribute__((aligned(4))) ={
        // 1. Reset device
      {REG_CTRL3_C,0x01}, //needs 50 us after reboot
      //{REG_CTRL3_C,0x01}, //rigger reboot
      {REG_CTRL3_C,(0x01 << 6) | (0x01 << 2)}, //block data update (new), read_increment_enabled (default)
        // 2. Set Accel ODR to 104 Hz (>60 Hz) and Anti-Aliasing filter to 50 Hz
        // CTRL1_XL: ODR[7:4] = 0101 (104 Hz), FS[3:2] = 01 (±16g), LPF2[1] = 0 (Filter BW = ODR/9)
      {REG_FIFO_CTRL3  | 0x0400,0x09 /*(0x01 << 3) | 0x01 */ },
      {REG_CTRL1_XL   | 0x0400,0x55 /*(0x05 << 4) | (0x01 << 2) | 0x01*/   }, //XL Hz, range, LPF
      {REG_CTRL2_G  | 0x0400,0x5C  /*(0x05 << 4) | (0x03 << 2) */  }, //gyro Hz, range
        // 3. Configure FIFO Decimation
        // FIFO_CTRL3: Accel decimation = 1 (1 sample), Gyro decimation = 0 (Disabled)
      
        // 4. Set FIFO Mode to "Continuous Mode" (overwrites old data if full)
        // FIFO_CTRL5: FIFO_Mode[2:0] = 110 (Continuous Mode), 0x01 for FIFO mode. Stops collecting data when FIFO is full
      {REG_FIFO_CTRL5  | 0x0400,/*0x2E*/(0x01 << 3) | 0x06 | 0x0200 },// 0x0200 (I2C STOP bit)
    };

    // list of register-value pairs to write to i2c periphreal on boot
    /*const uint16_t _boot_cmd[2][2] __attribute__((aligned(4))) ={
      {0x10,0x54},
      {0x11,(0x05 << 4) | (0x03 << 2) | 0x0200},
    };*/

    const uint16_t _get_fifo_size_cmd[3] __attribute__((aligned(4))) ={
      //REG_WHO_AM_I, 0x0300 // test
      //0x0A,0x0300, //test
      0x3A,0x0100,0x0100
    };

    const uint16_t _get_fifo_cmd[4] __attribute__((aligned(4*sizeof(uint16_t)))) ={
      REG_FIFO_DATA_OUT_L | 0x0400, 0x0100,
      REG_FIFO_DATA_OUT_H | 0x0400, 0x0100 //could do start adress with auto-incrementto support 3 commands rather than 4, but then wouldn't align with ring buffer size
    };//alignment needed for ring looping buffer

    const uint16_t _get_temperature_cmd[3] __attribute__((aligned(4))) = {
      0x20 | 0x0400,0x0100,0x0300
    };
    
    //const uint16_t _get_fifo_cmd = REG_FIFO_DATA_OUT_L | 0x0400;
    const uint16_t _read_operation = 0x0100; //read one uin8_t from FIFO

    // Target buffer for incoming RX FIFO data (doubles as the read request - in-place morphing buffer) --> skip this functionality, just send 0x0100 X times, then 0x0300
    //const uint16_t _rx_fifo_count_mask=0xFFFFF000;//which bits to zero-out (the fifo status) when using the fifo count register
    volatile uint16_t _rx_fifo_count; //number of unread words (16-bit axes) stored in FIFO (qty 6 is one accel+gyro reading).  must be 32-bit to pass through watchdog scratch register in order to do CLEAR operation on upp 4 bits of uint16_t
    volatile uint16_t _rx_buffer[IMU_BUFFER_SIZE] __attribute__((aligned(IMU_BUFFER_SIZE_BYTES))); 
    volatile uint32_t _rx_buffer_ptr=(uint32_t)&_rx_buffer[0];

    bool _temperature_ping_pong;
    volatile uint16_t _temperature[2] __attribute__((aligned(4)));

    volatile DmaDescriptor _aux0_write_fifo_cmd; //command the i2c periphreal to read X bytes from imu
    volatile DmaDescriptor _aux1_read_fifo_cmd; //the data the i2c periphreal spits out is read into buffer

public:
    IMU(i2c_inst_t* i2c_hardware = i2c0);
    
    void begin();

    uint16_t get_fifo_sample_count() const; //number of samples (axes) the imu periphreal reports in its memory
    float get_accel(uint8_t xyz) const; //m/s, xyz: 0=x, 1=y, 2=z
    float get_gyro(uint8_t xyz) const; //deg/sec, xyz: 0=x, 1=y, 2=z
    float get_quaternion(uint8_t wxyz) const; //wxyz: 0-3 as index
    float get_celsius() const;

    // IMultiDmaTransactionSource Interface
    int getRequiredDescriptorCount(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max) override;
    void populateDescriptors(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) override;

};


#endif