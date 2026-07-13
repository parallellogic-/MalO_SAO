#include "analog.h"

Analog::Analog() {
  
}

void Analog::begin() {
  adc_gpio_init(PIN_V_REF);//is motor on prototype
  adc_gpio_init(PIN_HALL);
  adc_gpio_init(PIN_POTENTIOMETER);

      // 1. Explicitly stop the free-running clock/sequencer
    adc_run(false);
    
    // 2. Disable round-robin pacing if you are sampling multiple pins
    adc_set_round_robin(0);
    
    // 3. Forcefully pop entries directly from the register until empty
    // This bypasses the blocking status checks in adc_fifo_drain()
    while (adc_fifo_get_level() > 0) {
        (void)adc_hw->fifo; // Directly read and discard from the hardware register
    }
    
    // 4. Clear any lingering error or conversion flags
    // (Overday/Underday flags inside the control register)
    adc_hw->fcs |= (ADC_FCS_OVER_BITS | ADC_FCS_UNDER_BITS);

    // 3. Enable the internal temperature sensor line
    adc_init();
  //adc_set_temp_sensor_enabled(true); //just a map into adc_hw->cs

// 4. Reset Round Robin Mask and select ALL active channels to sequence
    // A bitmask where each high bit commands the ADC to sequence that channel number
    
    /*uint32_t channel_mask = 0;
  for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
      channel_mask |= (1 << i);
  }
  adc_set_round_robin(channel_mask);*/

    // 5. Configure the hardware internal sample FIFO
  adc_fifo_setup(
      true,    // Write converted samples directly to the FIFO
      true,    // Enable DMA request signals (DREQ) when data lands in FIFO
      1,       // Trigger DREQ assertion when at least 1 sample is present
      false,   // Do not modify error bit states
      false    // Do not truncate 12-bit resolution data down to 8-bit bytes
  );

  //adc_fifo_drain(); //optional on boot?
  //adc_run(false); 
}

void Analog::end() {
    // 1. Forcefully stop the free-running clock/sequencer
    adc_run(false);
    
    // 2. Clear the round-robin mask so no channels are selected for sequencing
    adc_set_round_robin(0);
    
    // 3. Disable the hardware FIFO and its DMA request generation
    // This turns off the DREQ signals that drive your ScatterGatherEngine
    adc_fifo_setup(
        false,   // Disable writing converted samples to the FIFO
        false,   // Disable DMA request signals (DREQ)
        1,       // Threshold (ignored when disabled)
        false,   // Error bits (ignored when disabled)
        false    // 8-bit truncation (ignored when disabled)
    );
    
    // 4. Forcefully pop any trailing data out of the hardware register
    while (adc_fifo_get_level() > 0) {
        (void)adc_hw->fifo; 
    }
    
    // 5. Clear lingering error flags (Overflow/Underflow bits)
    adc_hw->fcs |= (ADC_FCS_OVER_BITS | ADC_FCS_UNDER_BITS);
}

uint16_t Analog::get_sample(uint8_t gpio_pin) const
{
  uint8_t channel=gpio_pin-ADC_BASE_PIN;
  if(channel>=ADC_CHANNEL_COUNT) return 0;
  uint32_t out=0;
  for(uint16_t iter=channel;iter<sizeof(_raw_buffer[0])/sizeof(_raw_buffer[0][0]);iter+=ADC_CHANNEL_COUNT) out+=_raw_buffer[!_ping_pong][iter];//sum the readings from the same channel each time it's collected from teh round-robin sampling
  out/=ADC_OVERSAMPLE;//if oversample by a factor of 256, then shift right 8 bits
  return (uint16_t)out;
}

float Analog::get_vcc(uint8_t gpio_pin,float ideal_v_ref) const//around 3.0~3.3V
{
  uint16_t reading=get_sample(gpio_pin);
  return 0x0FFF*ideal_v_ref/reading;
}

//-1.0 is -500Gauss (South), 1.0 is +500 Gauss (North)
float Analog::get_hall(uint8_t gpio_pin) const//-1.0 to 1.0
{
  uint16_t reading=get_sample(gpio_pin);
  return 2.0f*reading/0x0FFF-1.0;
}

float Analog::get_potentiometer(uint8_t gpio_pin) const//0 to 1.0
{
  uint16_t reading=get_sample(gpio_pin);
  return 1.0f*reading/0x0FFF;
}

float Analog::get_internal_celsius(float vcc) const
{
  uint16_t raw_val=_raw_buffer[!_ping_pong][TEMP_SENSOR_CHANNEL];
    // 12-bit conversion scale to reference voltage (3.3V)
    float voltage = (float)raw_val * (vcc / 0x0FFF);
    // Formula derived from the RP2350 Hardware Architecture documentation
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}


int Analog::getRequiredDescriptorCount(uint64_t frame_id) {

    //if(!_is_booted) return 1;
    return 3;
}

void Analog::populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) {
    
    dma_channel_config cfg;
    _ping_pong=frame_id%2;
    uint8_t dma_index=0;

    begin();
    
    /*if(!_is_booted)
    {
        static const uint32_t ADC_START_MASK = ADC_CS_START_MANY_BITS;

        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void *)&ADC_START_MASK;    //set DC LOW
        pool_start[dma_index].write_addr     = (void *)(&adc_hw->cs + REG_ALIAS_SET_BITS);
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;
      
       _is_booted=true;
      return;
    }*/

      // 1. Force halt any free-running conversions immediately
      /*adc_hw->cs &= ~ADC_CS_START_MANY_BITS; 

      // 2. Clear the EN bit in FIFO Control Register to flush internal pointers
      adc_hw->fcs &= ~ADC_FCS_EN_BITS;

      // 3. Re-enable the FIFO write capability so it is instantly ready for your next DMA run
      adc_hw->fcs |= ADC_FCS_EN_BITS;

      // Force hardware multiplexer register to begin at the lowest indexed active channel
      adc_select_input(0); // is just part of &adc_hw->cs
      */
      //if(frame_id>10) adc_fifo_drain();

      

        // STEP 2: parpare the command to kick the ADC 
        // into action right after the data channel is loaded and armed.
        // use a static helper memory address so the DMA core can safely reference it.
        //static const uint32_t FORCE_CONVERSION_START = ADC_CS_START_MANY_BITS;
        static const uint32_t FORCE_CONVERSION_START = 
          ADC_CS_RROBIN_BITS | // Enable round robin for all 9 channels
          ADC_CS_START_MANY_BITS |
          ADC_CS_TS_EN_BITS |
          ADC_CS_EN_BITS;        // Set the continuous sampling engine to run

        static const uint32_t FORCE_CONVERSION_STOP = 0x00;


        cfg = dma_channel_get_default_config(aux0_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_dreq(&cfg, DREQ_ADC);         // Sync pace directly to ADC hardware
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        _aux0_read_adc_cmd.read_addr      = (const void *)&adc_hw->fifo;    
        _aux0_read_adc_cmd.write_addr     = (void *)&_raw_buffer[_ping_pong];
        _aux0_read_adc_cmd.transfer_count = sizeof(_raw_buffer[_ping_pong])/sizeof(_raw_buffer[_ping_pong][0]);
        _aux0_read_adc_cmd.config         = cfg.ctrl;

        //data_chan triggers aux0 (sits idle waiting for samples)
        cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_chain_to(&cfg, ctrl_channel);
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void*)&_aux0_read_adc_cmd;
        pool_start[dma_index].write_addr     = (void*)&dma_hw->ch[aux0_channel].read_addr;
        pool_start[dma_index].transfer_count = 4;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //data_chan starts adc collecting samples
         cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg, false); // Read from fixed register address
        channel_config_set_write_increment(&cfg, false);  // Step through linear array array pointers
        //channel_config_set_chain_to(&cfg, ctrl_channel); //aux0 loops back flow control after reading samples
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void *)&FORCE_CONVERSION_START;    
        pool_start[dma_index].write_addr     = (void *)(&adc_hw->cs);// + REG_ALIAS_SET_BITS);
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;

        //data_chan starts adc collecting samples
         cfg = dma_channel_get_default_config(data_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&cfg, false); // Read from fixed register address
        channel_config_set_write_increment(&cfg, false);  // Step through linear array array pointers
        channel_config_set_chain_to(&cfg, ctrl_channel); 
        channel_config_set_enable(&cfg, true);

        pool_start[dma_index].read_addr      = (const void *)&FORCE_CONVERSION_STOP;    
        pool_start[dma_index].write_addr     = (void *)(&adc_hw->cs);// + REG_ALIAS_SET_BITS);
        pool_start[dma_index].transfer_count = 1;
        pool_start[dma_index].config         = cfg.ctrl;
        dma_index++;


    if(0)//dma_index>0)
    {
      Serial.print("DMA instruction size: "); Serial.println(dma_index); while(1);
    }
}

void Analog::debug()
{
  Serial.printf("VRef: %d/4096, VCC: %5.1f, Hall: %5.1f, Pot: %5.1f, degC: %5.1f\n",get_sample(PIN_V_REF),get_vcc(),get_hall(),get_potentiometer(), get_internal_celsius());
}
