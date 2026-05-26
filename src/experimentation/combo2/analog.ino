#include "analog.h"


Analog::Analog() {
  
}

void Analog::begin() {
    // 3. Enable the internal temperature sensor line
  adc_set_temp_sensor_enabled(true);

// 4. Reset Round Robin Mask and select ALL active channels to sequence
    // A bitmask where each high bit commands the ADC to sequence that channel number
    
    uint32_t channel_mask = 0;
  for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
      channel_mask |= (1 << i);
  }
  adc_set_round_robin(channel_mask);

    // 5. Configure the hardware internal sample FIFO
  adc_fifo_setup(
      true,    // Write converted samples directly to the FIFO
      true,    // Enable DMA request signals (DREQ) when data lands in FIFO
      1,       // Trigger DREQ assertion when at least 1 sample is present
      false,   // Do not modify error bit states
      false    // Do not truncate 12-bit resolution data down to 8-bit bytes
  );
}

int Analog::getRequiredDescriptorCount(uint64_t frame_id) {

    return 0;
}


uint16_t Analog::get_sample(uint8_t gpio_pin) const
{
  uint8_t channel=gpio_pin-ADC_BASE_PIN;
  if(channel>=ADC_CHANNEL_COUNT) return 0;
  return _buffer[_ping_pong][channel];
}

void Analog::populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) {
    
    dma_channel_config cfg;
    _ping_pong=frame_id%2;
    uint8_t dma_index=0;

    //

            //set DC LOW
            /*cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
            channel_config_set_read_increment(&cfg, false);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            pool_start[dma_index].read_addr      = &_ctrl_reg_data[0];    //set DC LOW
            pool_start[dma_index].write_addr     = (void *)_dc_pin_ctrl_reg_ptr;
            pool_start[dma_index].transfer_count = 1;
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;

            //SPI send config
            cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
            channel_config_set_read_increment(&cfg, true);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_dreq(&cfg, spi_get_dreq(_spi, true)); 
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            pool_start[dma_index].read_addr      = boot_state==1?(const void*)&init_128x128:(const void*)&contrast_command_buffer;
            pool_start[dma_index].write_addr     = (void *)&spi_get_hw(_spi)->dr;
            pool_start[dma_index].transfer_count = boot_state==1?sizeof(init_128x128):sizeof(contrast_command_buffer);
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;*/

    if(1)//dma_index>0)
    {
      Serial.print("DMA instruction size: "); Serial.println(dma_index); while(1);
    }
}