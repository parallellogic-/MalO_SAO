
//push_tone(uint32_t period,uint32_t duty,uint32_t cycle_count)
//bool play()
//is_busy()

#pragma once
#include <hardware/dma.h>
#include "dma_control_block.h"

#define MAX_PWM_CHAIN_LENGTH (256*2*8*5/4) //256 characters, 1/0, 8 bits, 20% margin

struct PulseChainConfig{
  uint8_t period; //duration of pulses in counts of system clock (or downsampled system clock) 
  uint8_t duty; //127 for 50% duty cycle, 0 for OFF
  uint16_t cycle_count; //number of pulses
};

class PulseChain : public ScatterGatherEngine {
private:
    // Example state: tracking data arrays we want the DMA to send/receive
    PIO _pio;
    int _sm;
    uint8_t _pwm_pin;
    PulseChainConfig _pwm_config[2][MAX_PWM_CHAIN_LENGTH] __attribute__((aligned(4)));
    volatile uint32_t _dma_addr_scratch=0;
    bool _is_ping_pong=false; //write to _is_ping_pong, read from !_is_ping_pong
    uint16_t _pwm_command_length[2]={0,0};//number of commands that are to be executed.  note: last command is for 0 cycle_count to kill dma chain
public:
    PulseChain() : ScatterGatherEngine() {}

    //void begin(bool is_aux);
    //void end();
    void begin(PIOProgramManager &pio_program_manager,uint8_t pwm_pin,float base_frequency_hz);

    //bool registerSource(IMultiDmaTransactionSource* source);

    const uint16_t get_max_command_length(){ return sizeof(_pwm_config[0])/sizeof(_pwm_config[0][0])-1; } //need 1 at end to account fo 0 command to kill DMA transaction

    //void compileAndRun(uint64_t frame_id) override;

    int getRequiredDescriptorCount(uint64_t frame_id) override;
    void populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) override;

    //bool is_dma_success(uint64_t frame_id) const;
    
    bool append_note(uint8_t period, uint8_t duty, uint16_t cycle_count);
    bool play();
    bool is_busy();
    void debug();
};
