#include "oled.h"
#include "hardware/clocks.h"

OLED::OLED(spi_inst_t* spi_port,uint32_t baud,uint8_t dc_pin) : _spi(spi_port), _baud(baud) {
  _dc_pin_ctrl_reg_ptr=(uint32_t *)&io_bank0_hw->io[dc_pin].ctrl;
}

void OLED::begin() {
  spi_init(spi1, SSD1327_SPI1_BAUD);
  gpio_set_function(SSD1327_SPI1_SCLK, GPIO_FUNC_SPI);
  gpio_set_function(SSD1327_SPI1_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(SSD1327_SPI1_CS,   GPIO_FUNC_SPI);
  gpio_init(SSD1327_SPI1_DC);//is needed for proper OLED operation
  gpio_set_dir(SSD1327_SPI1_DC, GPIO_OUT);
  gpio_put(SSD1327_SPI1_DC,HIGH);
}
void OLED::end() {
}

int OLED::getRequiredDescriptorCount(uint64_t frame_id) {

    switch(_get_boot_state(frame_id))
    {
      case 1: case 2:  return 2;
      case 3:  return 5;
      default: return 0;
    }
}

uint8_t OLED::_get_boot_state(uint64_t frame_id) const
{
    if(frame_id==1) return 1;//initial boot, need >30ms for OLED to boot up stable, otherwise comes up with inverted or offset colors (?).  WAS 7
    if(frame_id==2) return 2;
    if(frame_id<3) return 0;//gap between boot steps.  WAS 14, IS 8
    return 3; //normal operation
}

uint8_t* OLED::get_frame_buffer(){ return _frame_buffer[_screen_ping_pong]; }

void OLED::flush(){ _is_flush=true; }

void OLED::populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) {

    uint8_t boot_state= _get_boot_state(frame_id);
    if(boot_state==0) return;

    dma_channel_config cfg;
    if(_is_flush)
    {//only change frame buffer at frame boundary
      _is_flush=false;
      _screen_ping_pong^=1;
    }
    uint8_t dma_index=0;

    static uint32_t _ctrl_reg_data[2] = { //configure gpio override for LOW or HIGH output
      ( IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_SIOB_PROC_0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB ) |
      ( IO_BANK0_GPIO1_CTRL_OUTOVER_VALUE_LOW << IO_BANK0_GPIO1_CTRL_OUTOVER_LSB ),
      ( IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_SIOB_PROC_0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB ) |
      ( IO_BANK0_GPIO1_CTRL_OUTOVER_VALUE_HIGH << IO_BANK0_GPIO1_CTRL_OUTOVER_LSB ),
    };

    switch(boot_state){
      case 1: case 2:{

            //set DC LOW
            cfg = dma_channel_get_default_config(data_channel);
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
            dma_index++;

      }break;
      case 3:{
            //set DC LOW
            //SPI send config
            //SPI wait for FIFO to empty (dead-reckoning wait)
            //set DC HIGH
            //spi send frame

            //set DC LOW
            cfg = dma_channel_get_default_config(data_channel);
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

            pool_start[dma_index].read_addr      = (const void*)&frame_command_buffer;
            pool_start[dma_index].write_addr     = (void *)&spi_get_hw(_spi)->dr;
            pool_start[dma_index].transfer_count = sizeof(frame_command_buffer);
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;

            //SPI wait for FIFO to empty (dead-reckoning wait)
            uint32_t sys_clk_hz = clock_get_hz(clk_sys); //150'000'000 Hz
            uint32_t wait_steps=(uint32_t)(9.0*sizeof(frame_command_buffer)*sys_clk_hz/_baud);//how long to wait for spi fifo to emptyall the bits.  Note: 8 bits and a CS toggle per default SPI mode
            wait_steps+=wait_steps/4;//give some margin

            static constexpr uint8_t dummy_reg_read=0;
            static uint8_t dummy_reg_write;

            cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
            channel_config_set_read_increment(&cfg, false);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            pool_start[dma_index].read_addr      = (const void*)&dummy_reg_read;
            pool_start[dma_index].write_addr     = (void*)&dummy_reg_write;
            pool_start[dma_index].transfer_count = wait_steps; //factor of ~2 margin on reboot time
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;

            //set DC HIGH
            cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
            channel_config_set_read_increment(&cfg, false);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            pool_start[dma_index].read_addr      = &_ctrl_reg_data[1];    //set DC HIGH
            pool_start[dma_index].write_addr     = (void *)_dc_pin_ctrl_reg_ptr;
            pool_start[dma_index].transfer_count = 1;
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;

            //spi send frame
            cfg = dma_channel_get_default_config(data_channel);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
            channel_config_set_read_increment(&cfg, true);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_dreq(&cfg, spi_get_dreq(_spi, true)); 
            channel_config_set_chain_to(&cfg, ctrl_channel);
            channel_config_set_enable(&cfg, true);

            pool_start[dma_index].read_addr      = (const void *)&_frame_buffer[!_screen_ping_pong];
            pool_start[dma_index].write_addr     = (void *)&spi_get_hw(_spi)->dr;
            pool_start[dma_index].transfer_count = sizeof(_frame_buffer[0])/sizeof(_frame_buffer[0][0]);
            pool_start[dma_index].config         = cfg.ctrl;
            dma_index++;



            


            //Serial.print("DMA instruction size: "); Serial.println(dma_index); while(1);

      }break;
      default: return;
    }
    if(0)//dma_index>0)
    {
      Serial.print("boot_state: "); Serial.println(boot_state);
      Serial.print("DMA instruction size: "); Serial.println(dma_index); while(1);
    }
}

