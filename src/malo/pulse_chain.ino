#include "pulse_chain.h"
#include <hardware/pwm.h>

void PulseChain::begin(PIOProgramManager &pio_program_manager,uint8_t pwm_pin,float base_frequency_hz)
{
  _pwm_pin=pwm_pin;

  gpio_set_function(pwm_pin, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(pwm_pin);
  uint channel = pwm_gpio_to_channel(pwm_pin);
  pwm_set_wrap(slice_num, 255);//need to init to 8-bit value (upper 24 bits 0) because downstream callers only update the LSByte
  pwm_set_chan_level(slice_num, channel, 127); //downstream callers only update LSByte
  // 1. Get the current system clock frequency dynamically
  float sys_clk_hz = (float)clock_get_hz(clk_sys);
  // 2. Compute the precise divider: sys_clk / (target_hz * (TOP + 1))
  // Given target = 38000 Hz and TOP = 255 (which means 256 total steps)
  float dynamic_div = sys_clk_hz / (base_frequency_hz * 256.0f);
  // 3. Set the hardware divisor
  pwm_set_clkdiv(slice_num, dynamic_div);
  pwm_set_enabled(slice_num, true); 

  ScatterGatherEngine::begin(false); //false means data and ctrl dma's only, no aux allocation

  _pio=pio_program_manager.get_pio();
  _sm=pio_program_manager.allocate_sm();
  int sm_offset=pio_program_manager.get_offset();


  // 4. Configure the PIO State Machine to listen
  pio_sm_config c = pio_adder_program_get_default_config(sm_offset);
  
  sm_config_set_in_shift(&c, true, false, 32);  // Autopush OFF
  sm_config_set_out_shift(&c, true, false, 32); // Autopull OFF

  // Initialize and start the state machine
  pio_sm_init(_pio, _sm, sm_offset, &c);
  pio_sm_set_enabled(_pio, _sm, true);

  registerSource(this);//will call self to populate individual steps of ctrl_ and data_dma's
}

bool PulseChain::append_note(uint8_t period, uint8_t duty, uint16_t cycle_count) {
    bool write_buffer = _is_ping_pong; 
    uint16_t idx = _pwm_command_length[write_buffer];

    if (idx >= (get_max_command_length() - 1)) return false; // Buffer full

    _pwm_config[write_buffer][idx].period=period;
    _pwm_config[write_buffer][idx].duty=duty;
    _pwm_config[write_buffer][idx].cycle_count=cycle_count;

    _pwm_command_length[write_buffer]++;
    return true;
}

bool PulseChain::play() {
    if (is_busy()) return false;

    //abort dma's so they can be restarted

    // Append terminating zero command to kill the sequence at completion
    bool write_buffer = _is_ping_pong;
    uint16_t idx = _pwm_command_length[write_buffer];
    _pwm_config[write_buffer][idx].cycle_count = 0; 
    

    // Swap buffers so compileAndRun reads the newly filled data partition
    _pwm_command_length[write_buffer]=0;
    _is_ping_pong = !_is_ping_pong;

    // Fire the Scatter-Gather Engine compilation pass using an arbitrary frame ID
    compileAndRun(0); 
    return true;
}

bool PulseChain::is_busy() {
    // 1. Is either the control channel or data channel actively moving data?
    bool is_busy = dma_channel_is_busy(_data_chan) || dma_channel_is_busy(_ctrl_chan);
    
    // 2. Is the data channel armed and waiting for a PWM DREQ pacing signal?
    bool is_kickoff = (dma_hw->multi_channel_trigger & (1u << _data_chan)) != 0;

    // If it's physically moving bytes OR waiting on a hardware trigger, it is busy.
    // The moment a 0-count transfer executes, both 'is_busy' and 'is_kickoff' 
    // fall to false naturally on the exact same clock cycle.
    return (is_busy || is_kickoff);
}

// ============================================================================
// 2. SCATTER-GATHER INTERFACE HOOKS
// ============================================================================

int PulseChain::getRequiredDescriptorCount(uint64_t frame_id) {
  //zzstophere, blinking green
  /*while(true)
  {
    pinMode(38,OUTPUT);
    digitalWrite(38,1);
    delay(100);
    digitalWrite(38,0);
    delay(100);
  }*/
    return 7;
}

void PulseChain::populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, 
                                     int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) {
    
    //infinite loop of the following (transmitting a cycle_count of 0 terminates the sequence)

    //fetch 8 bits from _dma_addr_scratch, store into pwm_period
    //pio increment _dma_addr_scratch by 1
    //fetch 8 bits from _dma_addr_scratch, 
    //pio increment _dma_addr_scratch by 1
    //fetch 16 bits from _dma_addr_scratch, store into future command execution count
    //pio increment _dma_addr_scratch by 2
    //8 bit duty cycle pwm_duty, transaction_count number of times, gated by pwm dreq competion - control flow will halt here (no chain-to execution) on 0 transaction_count (cannot have ctrl_dma chain to data_dma and data_dma immediate chain-back to ctrl_dma)
    //set ctrl_dma config to point to pool_start[0] (no chain to, immedaite start on load)

    dma_channel_config cfg;
    uint8_t dma_index=0;
//    _is_ping_pong=!_is_ping_pong; //lock in the data that was being written is now being read from
    _dma_addr_scratch=(uint32_t)&_pwm_config[!_is_ping_pong][0]; //initalize address to the 0th index.  dma's will increment from here
    uint slice_num = pwm_gpio_to_slice_num(_pwm_pin);
    const static uint32_t const_period_size=sizeof(_pwm_config[0][0].period);
    const static uint32_t const_duty_size=sizeof(_pwm_config[0][0].duty);
    const static uint32_t const_cycle_count_size=sizeof(_pwm_config[0][0].cycle_count);

    //move the address into the next command source
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (const void*)&_dma_addr_scratch;
    pool_start[dma_index].write_addr     = (void*)&pool_start[dma_index+1].read_addr;
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    //move data from _dma_addr_scratch to pwm period
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, (const_period_size == 4) ? DMA_SIZE_32 : (const_period_size == 2) ? DMA_SIZE_16 : DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = 0; //set by instruction above
    pool_start[dma_index].write_addr     = (uint32_t*)(&pwm_hw->slice[slice_num].top+(4-const_period_size));
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    //now increment _pwm_config address by 1... (3 commands)

    // ============================================================================
    // STEP A: Feed 32-bit _dma_addr_scratch into PIO's TX FIFO
    // ============================================================================
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    // Pace based on when PIO TX FIFO has free room
    channel_config_set_dreq(&cfg, pio_get_dreq(_pio, _sm, true)); 
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (uint32_t*)&_dma_addr_scratch;
    pool_start[dma_index].write_addr     = (uint32_t*)&_pio->txf[_sm];
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    // ============================================================================
    // STEP B: Feed a constant 1 into PIO's TX FIFO
    // ============================================================================
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    // Also paced by the exact same PIO TX FIFO availability
    channel_config_set_dreq(&cfg, pio_get_dreq(_pio, _sm, true));
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (const void*)&const_period_size; // Must point to a valid memory location containing 1
    pool_start[dma_index].write_addr     = (uint32_t*)&_pio->txf[_sm];
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    // ============================================================================
    // STEP C: Take output from PIO's RX FIFO and put into _dma_addr_scratch
    // ============================================================================
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    // Pace based on when PIO RX FIFO has data available (is_tx = false)
    channel_config_set_dreq(&cfg, pio_get_dreq(_pio, _sm, false));
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (uint32_t*)&_pio->rxf[_sm];
    pool_start[dma_index].write_addr     = (uint32_t*)&_dma_addr_scratch;
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    //now repeat pwm configuration, but this time for the duty cycle rather than the period... (2 commands)

    //move the address into the next command source
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (const void*)&_dma_addr_scratch;
    pool_start[dma_index].write_addr     = (void*)&pool_start[dma_index+1].read_addr;
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    //move data from _dma_addr_scratch to pwm duty cycle
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, (const_duty_size == 4) ? DMA_SIZE_32 : (const_duty_size == 2) ? DMA_SIZE_16 : DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = 0; //set by instruction above
    pool_start[dma_index].write_addr     = (uint32_t*)(&pwm_hw->slice[slice_num].cc+(4-const_duty_size));
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    //now do dummy transfer cycle_count times...  but first need to get cycle count by doing address math (3 commands)

}


void PulseChain::debug()
{
  //Serial.println("PulseChain debug...");
  uint slice_num = pwm_gpio_to_slice_num(_pwm_pin);
  Serial.printf("_dma_addr_scratch_0: 0x%08X (val: %d), start: 0x%08X, top: 0x%08X (val: %d), cc: 0x%08X (val: %d)\n",_dma_addr_scratch,*(uint8_t*)_dma_addr_scratch,(uint32_t)&_pwm_config[_is_ping_pong][0],(uint32_t)&pwm_hw->slice[slice_num].top,pwm_hw->slice[slice_num].top,(uint32_t)&pwm_hw->slice[slice_num].cc,pwm_hw->slice[slice_num].cc);
  append_note(255,127,8);
  append_note(255,0,4);
  play();
  compileAndRun(0);
  delay(1);
  Serial.printf("_dma_addr_scratch_1: 0x%08X\n",_dma_addr_scratch);
}