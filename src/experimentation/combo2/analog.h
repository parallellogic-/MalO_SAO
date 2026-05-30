#pragma once

// Determine channel counts based on the compiled RP2350 chip variant

#define ADC_CHANNEL_COUNT 9  // Channels 0-7 are GPIOs 40-47, Channel 8 is Temp Sensor
#define TEMP_SENSOR_CHANNEL 8
#define TEMP_SENSOR_PIN 48
#define ADC_OVERSAMPLE 16 //collect extra readings and average them together to get a more stable reading
//if doing a large factor like 256, this will eat up a lot of RAM for the simple purpose of averaging it later.  Would be better to refactor scatter-gatherer to use
//sniff: ie. round-robin on one channel, route all samples through sniffer set to add, then extract the sum and write to ram before moving to the next channel to measure

class Analog : public IMultiDmaTransactionSource {
private:
    //bool _is_booted=0;

    bool _ping_pong=0;
    volatile uint16_t _raw_buffer[2][ADC_CHANNEL_COUNT*ADC_OVERSAMPLE]={};
    DmaDescriptor _aux0_read_adc_cmd; //command used to double/quadruple _rx_fifo_count to support i2c operations
    
public:
    Analog();
    
    void begin();

    //pointer to where application can upload the frame information (4-bits per pixel)
    //be aware of existing dirty frame contents present in buffer
    uint16_t get_sample(uint8_t channel) const;

    float get_vcc(uint8_t gpio_pin,float ideal_v_ref) const;
    float get_hall(uint8_t gpio_pin) const;//-1.0 to 1.0
    float get_potentiometer(uint8_t gpio_pin) const;//0 to 1.0
    float get_internal_celsius(float vcc) const;

    // IMultiDmaTransactionSource Interface
    int getRequiredDescriptorCount(uint64_t frame_id) override;
    void populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) override;
};

