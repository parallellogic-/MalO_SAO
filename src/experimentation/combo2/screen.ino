#include "screen.h"

Screen::Screen(spi_inst_t* spi_port) {

}

void Screen::begin() {
}

int Screen::getRequiredDescriptorCount(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max) {
    if (subframe_id > 0) return 0;

    switch(_get_boot_state(frame_id))
    {
      case 0:  return 0;
      case 1:  return 0;
      case 2:  return 0;
      default: return 0;
    }
}

uint8_t Screen::_get_boot_state(uint64_t frame_id) const
{
    if(frame_id==7) return 1;//initial boot, need >30ms for screen to boot up stable, otherwise comes up with inverted or offset colors (?)
    if(frame_id==14) return 2;
    if(frame_id<14) return 0;//gap between boot steps
    return 3;
}

void Screen::populateDescriptors(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) {
    if(subframe_id>0) return;

    dma_channel_config cfg;
    _screen_ping_pong=frame_id%2;
    uint8_t dma_index=0;

    switch(_get_boot_state(frame_id)){
      case 0: return;
      case 1:{

      }break;
      case 2:{

      }break;
      case 3:{

      }break;
    }
}