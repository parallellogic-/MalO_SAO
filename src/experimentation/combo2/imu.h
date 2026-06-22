#pragma once

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
#define REG_INT2_CTRL 0x0E

// ----

/* Hz   | config
 * 12.5 | 0b0001
 * 26   | 0b0010
 * 52   | 0b0011
 * 104  | 0b0100
 * 208  | 0b0101
 * 416  | 0b0110
 * 833  | 0b0111
 * 1660 | 0b1000
*/
#define IMU_HZ          208
#define IMU_MS          (1000.0/IMU_HZ)
#define HZ_CONFIG       0b0101

/* Low pass filter (ex. 100 Hz 3dB BW for 208 Hz sampling: factor of 2 offset/Nyquist)
 * Hz  | config
 * 50  | 0b11
 * 100 | 0b10
 * 200 | 0b01
 * 400 | 0b00
*/
#define ACCEL_LPF_CONFIG     0b11

/* GYRO_RANGE_DEG_SEC | GYRO_RANGE_CONFIG
 *  250               | 0b00
 *  500               | 0b01
 * 1000               | 0b10
 * 2000               | 0b11
 */
#define GYRO_RANGE_DEG_SEC 2000
#define GYRO_RANGE_CONFIG 0b11
#define GYRO_DEG_SEC_PER_LSB (GYRO_RANGE_DEG_SEC*1.0/0x8000)

/* G's | config
 * 2   | 0b00
 * 4   | 0b10 //beware out-of-order bits (bit flip in design?)
 * 8   | 0b11
 * 16  | 0b01
*/
#define ACCEL_RANGE_G          16
#define ACCEL_RANGE_G_PER_LSB  (ACCEL_RANGE_G*1.0/0x8000)
#define ACCEL_RANGE_CONFIG     0b01

// ----

#define IMU_BUFFER_SIZE (1<<12) // 4096 elements (uint16_t) //gyro and accel data: 2 bytes gyro_x, _y, _z, accel_x, _y, _z
#define IMU_BUFFER_SIZE_BYTES (IMU_BUFFER_SIZE * sizeof(uint16_t))

#define IMU_DMA_SCRATCH_REG pwm_hw->slice[11].div //need somewhere to write the 12-bit fifo size to where it can be operated on (set/clear/xor).  pwm/div has the bonus of have only 12 bits viable anyway - automatically filtering out the upper 4 bits without a separate operation

class IMU : public IMultiDmaTransactionSource {
private:
    i2c_inst_t* _i2c;
    bool _is_booted=false;

    bool _imu_ping_pong=0;

    float _state_quaternion[2][4]={{1.0,0.0,0.0,0.0},{1.0,0.0,0.0,0.0}};
    float _gyro_accel_reading[2][6]={};//gyro deg/sec, accel g's - lastest reading average (16 ms avg)
    //float _accel_g[2][4]={};
    //float _gyro_deg_sec[2][4]={};
    
    //to be verified: The I2C hardware macro block cannot change its target address (IC_TAR) while the peripheral is actively enabled. Attempting to write to IC_TAR via DMA while the block is active will cause the hardware to silently ignore the transaction.
    uint32_t _i2c_disable       __attribute__((aligned(4))) = 0;
    uint32_t _i2c_target_addr   __attribute__((aligned(4))) = LSM6DS_ADDR;
    uint32_t _i2c_enable        __attribute__((aligned(4))) = 1;
    volatile bool _is_dma_done=false;

    //I2C: IC_DATA_CMD Register
    //The RESTART is only required when changing direction from a write to a read.

    // list of register-value pairs to write to i2c periphreal on boot
    // to make this more portable: would need to make this list varaible and populate in the constructor
    static constexpr uint16_t _boot_cmd[8][2] __attribute__((aligned(4))) ={
        // 1. Reset device
      {REG_CTRL3_C            ,  0x01 },//reboot
      {REG_WHO_AM_I   | 0x0400,  0x0100 }, //reboot needs 50 us to clear, downstream code will insert a delay after this read operation
      {REG_CTRL3_C    | 0x0400, (0x01 << 6) | (0x01 << 2) }, //block data update (new), read_increment_enabled (default)
      {REG_FIFO_CTRL3 | 0x0400,  0x09 }, //no FIFO decimation
      {REG_CTRL1_XL   | 0x0400, (HZ_CONFIG << 4) | (ACCEL_RANGE_CONFIG << 2) | ACCEL_LPF_CONFIG  }, //XL Hz, range, LPF
      {REG_CTRL2_G    | 0x0400, (HZ_CONFIG << 4) | (GYRO_RANGE_CONFIG  << 2)   }, //gyro Hz, range

      {REG_INT2_CTRL  | 0x0400, 0xFF   }, // PROTOTYPE ONLY - set INT2 HIGH so it doesn't connect the GND/3V3 rails

      
        // 4. Set FIFO Mode to "Continuous Mode" (overwrites old data if full)
        // FIFO_CTRL5: FIFO_Mode[2:0] = 110 (Continuous Mode), 0x01 for FIFO mode. Stops collecting data when FIFO is full
      {REG_FIFO_CTRL5 | 0x0400, (HZ_CONFIG << 3) | 0x06 | 0x0200 },// 0x0200 (I2C STOP bit)
    };

    static constexpr uint16_t _boot_check_cmd[2] __attribute__((aligned(4)))={
      REG_FIFO_CTRL5 | 0x0400, 0x0100 | 0x0200 //restart, read+stop
    }; //to cleanly switch between i2c targets, need to end with a read operation to ensure fifos are empty before moving on
    uint8_t _boot_check=0;

    static constexpr uint16_t _get_fifo_size_cmd[3] __attribute__((aligned(4))) ={
      REG_FIFO_STATUS1,0x0100,0x0100
    };

    static constexpr uint16_t _get_fifo_cmd[4] __attribute__((aligned(4*sizeof(uint16_t)))) ={
      REG_FIFO_DATA_OUT_L | 0x0400, 0x0100,
      REG_FIFO_DATA_OUT_H | 0x0400, 0x0100 //could do start adress with auto-incrementto support 3 commands rather than 4, but then wouldn't align with ring buffer size
    };//alignment needed for ring looping buffer

    static constexpr uint16_t _get_temperature_cmd[3] __attribute__((aligned(4))) = {
      0x20 | 0x0400,0x0100,0x0100 | 0x0200
    };

    // Target buffer for incoming RX FIFO data (doubles as the read request - in-place morphing buffer) --> skip this functionality, just send 0x0100 X times, then 0x0300
    volatile uint16_t _rx_fifo_count=0; //number of unread words (16-bit axes) stored in FIFO (qty 6 is one accel+gyro reading).  must be 32-bit to pass through watchdog scratch register in order to do CLEAR operation on upp 4 bits of uint16_t
    volatile uint16_t _rx_fifo_count_x2=0;
    volatile uint16_t _rx_fifo_count_x4=0;
    volatile int16_t _rx_buffer[IMU_BUFFER_SIZE] __attribute__((aligned(IMU_BUFFER_SIZE_BYTES))); //holds gryo/accel samples

    //bool _temperature_ping_pong=0;
    volatile int16_t _temperature[2] __attribute__((aligned(4)));//0xFE70 is 0degC, 0x0000 is 25degC, 0x0190 is 50degC

    volatile DmaDescriptor _aux0_sum_cmd; //command used to double/quadruple _rx_fifo_count to support i2c operations
    volatile DmaDescriptor _aux0_fifo_ram_to_i2c_cmd; //command the i2c periphreal to read X bytes from imu
    volatile DmaDescriptor _aux1_fifo_i2c_to_ram_cmd; //the data the i2c periphreal spits out is read into buffer in local RAM

    uint16_t _update_from_index=0;//the index within _rx_buffer that has been processed by accel/gyro/quat state estimate

    volatile bool _is_data_ready=0;//flag from DMA to core1 to perform math for update()
public:
//    float _gyro_accel_reading[2][6]={};//gyro deg/sec, accel g's - lastest reading average (16 ms avg)
    IMU(i2c_inst_t* i2c_hardware = i2c0);
    
    void begin();
    void end();

    uint16_t get_fifo_sample_count() const; //number of samples (axes) the imu periphreal reports in its memory
    float get_accel(uint8_t xyz) const; //m/s, xyz: 0=x, 1=y, 2=z
    float get_gyro(uint8_t xyz) const; //deg/sec, xyz: 0=x, 1=y, 2=z
    float get_quaternion(uint8_t wxyz) const; //wxyz: 0-3 as index
    float get_celsius() const;

    // IMultiDmaTransactionSource Interface
    int getRequiredDescriptorCount(uint64_t frame_id) override;
    void populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) override;

    bool update(); //the populateDescriptors operation is a DMA to get data from the external periphreal into local RAM.  update() method converts data (if any is available) into a format usable by downstream processing.   mult/div/float operations are in update()

};


