#pragma once

// Determine channel counts based on the compiled RP2350 chip variant
#if PICO_RP2350B
    #define ADC_CHANNEL_COUNT 9  // Channels 0-7 are GPIOs 40-47, Channel 8 is Temp Sensor
    #define TEMP_SENSOR_CHANNEL 8
#else
    #define ADC_CHANNEL_COUNT 5  // Channels 0-3 are GPIOs 26-29, Channel 4 is Temp Sensor
    #define TEMP_SENSOR_CHANNEL 4
#endif

class Analog : public IMultiDmaTransactionSource {
private:

    bool _ping_pong=0;
    volatile uint16_t _buffer[2][ADC_CHANNEL_COUNT]={};

public:
    Analog();
    
    void begin();

    //pointer to where application can upload the frame information (4-bits per pixel)
    //be aware of existing dirty frame contents present in buffer
    uint16_t get_sample(uint8_t channel) const;

    float get_hall(uint8_t gpio_pin);//-1.0 to 1.0
    float get_potentiometer(uint8_t gpio_pin);//0 to 1.0
    float get_internal_celsius();

    // IMultiDmaTransactionSource Interface
    int getRequiredDescriptorCount(uint64_t frame_id) override;
    void populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) override;
};

