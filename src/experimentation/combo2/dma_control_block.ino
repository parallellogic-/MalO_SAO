//Scatterer Gatherer

#include <Arduino.h>
#include "hardware/dma.h"
#include "dma_control_block.h"

void printDmaChannelStatus(int channel_id, const char* channel_name) {
    if (channel_id < 0 || channel_id >= NUM_DMA_CHANNELS) {
        Serial.println("[DEBUG ERROR] Invalid channel ID");
        return;
    }

    // Capture the hardware register state snapshot atomically
    dma_channel_hw_t* hw = &dma_hw->ch[channel_id];
    uint32_t current_read   = hw->read_addr;
    uint32_t current_write  = hw->write_addr;
    uint32_t current_count  = hw->transfer_count;
    uint32_t current_ctrl   = hw->ctrl_trig;

    // Decode explicit bit fields from the CTRL register map
    bool is_busy     = (current_ctrl & DMA_CH0_CTRL_TRIG_BUSY_BITS) != 0;
    bool is_enabled  = (current_ctrl & DMA_CH0_CTRL_TRIG_EN_BITS) != 0;
    uint8_t data_size = (current_ctrl & DMA_CH0_CTRL_TRIG_DATA_SIZE_BITS) >> DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB;
    uint8_t chain_to  = (current_ctrl & DMA_CH0_CTRL_TRIG_CHAIN_TO_BITS) >> DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB;
    
    // RP2350 specific enhancement: Check if the channel is currently locked in an abort routine
    bool is_aborting = (dma_hw->abort & (1u << channel_id)) != 0;

    Serial.println("==================================================");
    Serial.print(" DMA CHANNEL STATUS REPORT: "); Serial.println(channel_name);
    Serial.println("==================================================");
    Serial.print("  Hardware ID:       Ch "); Serial.println(channel_id);
    
    // Operational States
    Serial.print("  Status State:      "); 
    if (is_aborting)      Serial.println("[ABORTING/BLOCKED]");
    else if (is_busy)     Serial.println("[ACTIVE / RUNNING]");
    else if (!is_enabled) Serial.println("[DISABLED]");
    else                  Serial.println("[IDLE / WAITING]");

    // Register Hex Mappings
    Serial.print("  Current READ_ADDR:  0x"); Serial.println(current_read, HEX);
    Serial.print("  Current WRITE_ADDR: 0x"); Serial.println(current_write, HEX);
    Serial.print("  Remaining Words:    "); Serial.println(current_count & 0x0FFFFFFF); // Masks out RP2350 mode bits

    // Decoded Configurations
    Serial.print("  Data Unit Size:     ");
    switch(data_size) {
        case DMA_SIZE_8:  Serial.println("8-bit (Byte)"); break;
        case DMA_SIZE_16: Serial.println("16-bit (Half-Word)"); break;
        case DMA_SIZE_32: Serial.println("32-bit (Word)"); break;
        default:          Serial.println("Unknown Sizing Block"); break;
    }
    
    Serial.print("  Chain-To Target:    Ch "); Serial.println(chain_to);
    Serial.print("  Raw CTRL Register:  0x"); Serial.println(current_ctrl, HEX);
    Serial.println("==================================================");
}

// ==========================================
// 2. THE COMPILING SCATTER-GATHER ENGINE
// ==========================================
void ScatterGatherEngine::begin(bool is_aux) {
        _ctrl_chan = dma_claim_unused_channel(true);
        _data_chan = dma_claim_unused_channel(true);
        if(is_aux)
        {
          _aux0_chan = dma_claim_unused_channel(true);
          _aux1_chan = dma_claim_unused_channel(true);
        }
    }

bool ScatterGatherEngine::registerSource(IMultiDmaTransactionSource* source) {
        if (_registrant_count >= MAX_DMA_CONTROL_REGISTRANTS) return false;
        _registrants[_registrant_count++] = source;
        return true;
    }

void ScatterGatherEngine::compileAndRun(uint64_t frame_id,uint8_t subframe_id,uint8_t subframe_max) {
        //Serial.print("frame_id");  Serial.print(": "); Serial.println(frame_id);
        int current_pool_index = 0;

        // Step A: Allocate space and allow peripherals to build their sequences
        for (int i = 0; i < _registrant_count; i++) {
            if (_registrants[i] == nullptr) continue;

            int needed = _registrants[i]->getRequiredDescriptorCount(frame_id,subframe_id,subframe_max);
            if (needed == 0) continue;

            // Bounds check to ensure the memory buffer doesn't overflow
            if (current_pool_index + needed >= MAX_DMA_CONTROL_ACTIONS) {
                Serial.println("[DMA ERROR] Pool capacity exceeded");
                return;
            }

            // Let the object build its internal block chain directly inside the global pool
            _registrants[i]->populateDescriptors(frame_id,subframe_id,subframe_max,
                &_global_pool[current_pool_index], 
                _data_chan, 
                _aux0_chan, 
                _aux1_chan, 
                _ctrl_chan
            );

            current_pool_index += needed;
            //Serial.print("needed");  Serial.print(": "); Serial.println(needed);
        }
        
        // Update all generated data block operations to link back to control dma
        /*for (int iter = 0; iter < current_pool_index; iter++) //current_pool_index-1 to keep stale read/write/count/config values in dma registers
        {
            // 1. Instantiate a blank SDK config tracker
            dma_channel_config data_config;
            
            // 2. Assign the already populated raw register bits from your pool directly to the config struct member
            data_config.ctrl = _global_pool[iter].config;
            
            // 3. Update the chaining target to point safely to your control channel variable
            channel_config_set_chain_to(&data_config, _ctrl_chan);
            channel_config_set_enable(&data_config, true);
            
            // 4. Repack the updated bits back into the global descriptor pool
            _global_pool[iter].config = data_config.ctrl;
        }*/

        //Serial.print("current_pool_index");  Serial.print(": "); Serial.println(current_pool_index);

        if (current_pool_index == 0) return; // Quick exit if no transfers are queued

        // Step B: Append the Master Terminal Block to break the DMA execution loop
        _global_pool[current_pool_index].read_addr = nullptr;
        _global_pool[current_pool_index].write_addr = nullptr;
        _global_pool[current_pool_index].transfer_count = 0;
        _global_pool[current_pool_index].config = 0;

        // Step C: Setup and deploy the master pacing registers
        dma_channel_config c_master = dma_channel_get_default_config(_ctrl_chan);
        channel_config_set_transfer_data_size(&c_master, DMA_SIZE_32);
        channel_config_set_read_increment(&c_master, true);
        channel_config_set_write_increment(&c_master, true);
        channel_config_set_ring(&c_master, true, 4); // Keep writes localized strictly to target registers

        dma_channel_configure(
            _ctrl_chan,
            &c_master,
            &dma_hw->ch[_data_chan].read_addr,
            _global_pool,
            4,
            false
        );

        // Fire the pipeline burst
        dma_channel_abort(_data_chan);
        dma_channel_abort(_ctrl_chan);
        dma_channel_abort(_aux0_chan);
        dma_channel_abort(_aux1_chan);
        dma_channel_set_read_addr(_ctrl_chan, _global_pool, true);
        //Serial.print("dma_control_block_started");  Serial.print(": "); Serial.println("dma_control_block_started");
        
        if(0)
        {
          delay(16);
          printDmaChannelStatus(_ctrl_chan,"_ctrl_chan");
          printDmaChannelStatus(_data_chan,"_data_chan");
          printDmaChannelStatus(_aux0_chan,"_aux0_chan");
          printDmaChannelStatus(_aux1_chan,"_aux1_chan");
          for (int iter = 0; iter < current_pool_index; iter++)
          {
            Serial.print("_global_pool["); Serial.print(iter); Serial.print("].read_addr: "); Serial.println((uint32_t)_global_pool[iter].read_addr,HEX);
            Serial.print("_global_pool["); Serial.print(iter); Serial.print("].write_addr: "); Serial.println((uint32_t)_global_pool[iter].write_addr,HEX);
            Serial.print("_global_pool["); Serial.print(iter); Serial.print("].transfer_count: "); Serial.println((uint32_t)_global_pool[iter].transfer_count,HEX);
            Serial.print("_global_pool["); Serial.print(iter); Serial.print("].config: "); Serial.println((uint32_t)_global_pool[iter].config,HEX);
          }
        }
    }

int ScatterGatherEngine::getRequiredDescriptorCount(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max) {
  return 1;
}

void ScatterGatherEngine::populateDescriptors(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) {

    dma_channel_config cfg;
    uint8_t dma_index=0;

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

}

bool ScatterGatherEngine::is_dma_success(uint64_t frame_id) const{
  return frame_id==0 || _is_data_ready;
}
 


