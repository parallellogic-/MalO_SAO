#include "pulse_chain.h"
#include <hardware/pwm.h>
#include "RS-FEC.h"

void PulseChain::begin(PIOProgramManager &pio_program_manager,uint8_t pwm_pin,float base_frequency_hz)
{
  //Serial.printf("PulseChain::begin...\n"); delay(1);
  _pwm_pin=pwm_pin;

  gpio_set_function(pwm_pin, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(pwm_pin);
  uint channel = pwm_gpio_to_channel(pwm_pin);
  pwm_set_wrap(slice_num, 255);//need to init to 8-bit value (upper 24 bits 0) because downstream callers only update the LSByte
  pwm_set_chan_level(slice_num, channel, 0); //downstream callers only update LSByte
  // 1. Get the current system clock frequency dynamically
  float sys_clk_hz = (float)clock_get_hz(clk_sys);
  // 2. Compute the precise divider: sys_clk / (target_hz * (TOP + 1))
  // Given target = 38000 Hz and TOP = 255 (which means 256 total steps)
  float dynamic_div = sys_clk_hz / (base_frequency_hz * 256.0f);
  // 3. Set the hardware divisor
  pwm_set_clkdiv(slice_num, dynamic_div);
  pwm_set_enabled(slice_num, true); 

  //Serial.printf("ScatterGatherEngine::begin...\n"); delay(1);
  ScatterGatherEngine::begin(false); //false means data and ctrl dma's only, no aux allocation

  _pio=pio_program_manager.get_pio();
  _sm=pio_program_manager.allocate_sm();
  _initial_pc_offset=pio_program_manager.get_offset();
  int sm_offset=pio_program_manager.get_offset();
  //static uint8_t debug=0; debug++; while(debug==2){Serial.printf("pulse_chain %d, %d, %d, %d\n",_pio,_sm,_initial_pc_offset,sm_offset);delay(100); }


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
    _pwm_config[write_buffer][idx].period = 255; 
    _pwm_config[write_buffer][idx].duty = 0; 
    _pwm_config[write_buffer][idx].cycle_count = 0; 
    
    // Swap buffers so compileAndRun reads the newly filled data partition
    //_pwm_command_length[write_buffer]=0;
    //_is_ping_pong = !_is_ping_pong;
    clear();

    // Fire the Scatter-Gather Engine compilation pass using an arbitrary frame ID
    compileAndRun(0); 
    return true;
}

bool PulseChain::clear(){
    _pwm_command_length[_is_ping_pong]=0;
    _is_ping_pong = !_is_ping_pong;
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
    return 21;//4+3+4+3+3+3+1;//11;//16;
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

    // 1. Disable the state machine to freeze execution safely
    pio_sm_set_enabled(_pio, _sm, false);
    // 2. Wipe the TX and RX FIFOs entirely
    pio_sm_clear_fifos(_pio, _sm);
    // 3. Reset internal block states (clears stall conditions, OSR/ISR counters, IRQ blocks)
    pio_sm_restart(_pio, _sm);
    // 4. Force the Program Counter (PC) back to your initial loaded program offset
    pio_sm_exec(_pio, _sm, pio_encode_jmp(_initial_pc_offset));
    // 5. Re-enable the state machine to begin processing fresh data
    pio_sm_set_enabled(_pio, _sm, true);

    dma_channel_config cfg;
    uint8_t dma_index=0;
//    _is_ping_pong=!_is_ping_pong; //lock in the data that was being written is now being read from --> swap is done in super method
    _dma_addr_scratch=(uint32_t)&_pwm_config[!_is_ping_pong][0]; //initalize address to the 0th index.  dma's will increment from here
    uint slice_num = pwm_gpio_to_slice_num(_pwm_pin);
    uint channel = pwm_gpio_to_channel(_pwm_pin);
    static const uint32_t const_period_size=sizeof(_pwm_config[0][0].period);
    static const uint32_t const_duty_size=sizeof(_pwm_config[0][0].duty);
    static const uint32_t const_cycle_count_size=sizeof(_pwm_config[0][0].cycle_count);
    static const uint32_t dummy_read=0;//precon: is 0
    volatile static uint8_t dummy_write=0;

    //initial clean to set _dma_value_scratch to 0
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (const void*)&dummy_read;
    pool_start[dma_index].write_addr     = (void*)(&_dma_value_scratch);
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

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

    //move data from _dma_addr_scratch to pwm period.  Note: value only accepted when MSByte is written, so need intermedaite step to pad 8 bits to 16 bits...
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, (const_period_size == 2) ? DMA_SIZE_16 : DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = 0; //set by instruction above
    pool_start[dma_index].write_addr     = (void*)(&_dma_value_scratch);//(&pwm_hw->slice[slice_num].top);//+(2-const_period_size));
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (const void*)&_dma_value_scratch;
    pool_start[dma_index].write_addr     = (uint32_t*)&pwm_hw->slice[slice_num].top;
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    //now increment _pwm_config address by 1... (3 commands)

    // ============================================================================
    // STEP A1: Feed 32-bit _dma_addr_scratch into PIO's TX FIFO
    // ============================================================================
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    // Pace based on when PIO TX FIFO has free room
    //channel_config_set_dreq(&cfg, pio_get_dreq(_pio, _sm, true)); 
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (uint32_t*)&_dma_addr_scratch;
    pool_start[dma_index].write_addr     = (uint32_t*)&_pio->txf[_sm];
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    // ============================================================================
    // STEP B1: Feed a constant 1 into PIO's TX FIFO
    // ============================================================================
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    // Also paced by the exact same PIO TX FIFO availability
    //channel_config_set_dreq(&cfg, pio_get_dreq(_pio, _sm, true));
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (const void*)&const_period_size; // Must point to a valid memory location containing 1
    pool_start[dma_index].write_addr     = (uint32_t*)&_pio->txf[_sm];
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    // ============================================================================
    // STEP C1: Take output from PIO's RX FIFO and put into _dma_addr_scratch
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

    //initial clean to set _dma_value_scratch to 0
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (const void*)&dummy_read;
    pool_start[dma_index].write_addr     = (void*)(&_dma_value_scratch);
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

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
    channel_config_set_transfer_data_size(&cfg, (const_duty_size == 2) ? DMA_SIZE_16 : DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = 0; //set by instruction above
    pool_start[dma_index].write_addr     = (uint32_t*)(&_dma_value_scratch+(2*channel));
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    //move data from _dma_addr_scratch to pwm duty cycle
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (const void*)(&_dma_value_scratch);
    pool_start[dma_index].write_addr     = (uint32_t*)(&pwm_hw->slice[slice_num].cc);
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    //now do dummy transfer cycle_count times...  but first need to get cycle count by doing address math (3 commands)

    // ============================================================================
    // STEP A2: Feed 32-bit _dma_addr_scratch into PIO's TX FIFO
    // ============================================================================
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    // Pace based on when PIO TX FIFO has free room
    //channel_config_set_dreq(&cfg, pio_get_dreq(_pio, _sm, true)); 
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (uint32_t*)&_dma_addr_scratch;
    pool_start[dma_index].write_addr     = (uint32_t*)&_pio->txf[_sm];
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    // ============================================================================
    // STEP B2: Feed a constant 1 into PIO's TX FIFO
    // ============================================================================
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    // Also paced by the exact same PIO TX FIFO availability
    //channel_config_set_dreq(&cfg, pio_get_dreq(_pio, _sm, true));
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (const void*)&const_duty_size; // Must point to a valid memory location containing 1
    pool_start[dma_index].write_addr     = (uint32_t*)&_pio->txf[_sm];
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    // ============================================================================
    // STEP C2: Take output from PIO's RX FIFO and put into _dma_addr_scratch
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

    //now pointing at base of cycle_count
    //so put the cycle count in the <future> dummy call to stall for PWM cycle dreq

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

    //move the address into the next command source
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, (const_cycle_count_size == 4) ? DMA_SIZE_32 : (const_cycle_count_size == 2) ? DMA_SIZE_16 : DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = 0; // set by previous instruction
    pool_start[dma_index].write_addr     = (void*)(&pool_start[dma_index+1].transfer_count);
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;


    //dummy wait for PWM cycle to complete... NOTE: if cycle_count is 0, this step will halt the DMA here --> EXIT

    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_dreq(&cfg, pwm_get_dreq(slice_num));
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (uint32_t*)&dummy_read;
    pool_start[dma_index].write_addr     = (uint32_t*)&dummy_write;
    pool_start[dma_index].transfer_count = 0; //updated from command above
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    //cleanup to move the base pointer to point at the beginning of the next _pwm_config...

    // ============================================================================
    // STEP A3: Feed 32-bit _dma_addr_scratch into PIO's TX FIFO
    // ============================================================================
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    // Pace based on when PIO TX FIFO has free room
    //channel_config_set_dreq(&cfg, pio_get_dreq(_pio, _sm, true)); 
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (uint32_t*)&_dma_addr_scratch;
    pool_start[dma_index].write_addr     = (uint32_t*)&_pio->txf[_sm];
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    // ============================================================================
    // STEP B3: Feed a constant 1 into PIO's TX FIFO
    // ============================================================================
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_chain_to(&cfg, ctrl_channel);
    // Also paced by the exact same PIO TX FIFO availability
    //channel_config_set_dreq(&cfg, pio_get_dreq(_pio, _sm, true));
    channel_config_set_enable(&cfg, true);
    pool_start[dma_index].read_addr      = (const void*)&const_cycle_count_size; // Must point to a valid memory location containing 1
    pool_start[dma_index].write_addr     = (uint32_t*)&_pio->txf[_sm];
    pool_start[dma_index].transfer_count = 1;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    // ============================================================================
    // STEP C3: Take output from PIO's RX FIFO and put into _dma_addr_scratch
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

    //now that everything is clean for the next cycle, command the data to re-load ctrl_dma back to start...
    //prepare dummy packet in RAM for data channel to load into control channel

    cfg = dma_channel_get_default_config(ctrl_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_ring(&cfg, true, 4); // Keep writes localized strictly to target registers
    //channel_config_set_chain_to(&cfg, ctrl_channel);
    channel_config_set_enable(&cfg, true);

    _ctrl_loop_cfg.read_addr      = (const void*)&pool_start[0]; //restart read commands from beginning
    _ctrl_loop_cfg.write_addr     = (void*)&dma_hw->ch[data_channel].read_addr;
    _ctrl_loop_cfg.transfer_count = 4;
    _ctrl_loop_cfg.config         = cfg.ctrl;

    //now have control data load the data packet intro the control channel, resetting the control channel back to the beginning of the command list
    cfg = dma_channel_get_default_config(data_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_enable(&cfg, true);

    pool_start[dma_index].read_addr      = (uint32_t*)&_ctrl_loop_cfg;
    pool_start[dma_index].write_addr     = (uint32_t*)&dma_hw->ch[ctrl_channel].read_addr;
    pool_start[dma_index].transfer_count = 4;
    pool_start[dma_index].config         = cfg.ctrl;
    dma_index++;

    //Serial.println(dma_index);//21

}


uint16_t encodeHamming128(uint8_t data) {
    // Extract individual bits from the 8-bit input byte
    bool d3  = (data >> 7) & 1;
    bool d5  = (data >> 6) & 1;
    bool d6  = (data >> 5) & 1;
    bool d7  = (data >> 4) & 1;
    bool d9  = (data >> 3) & 1;
    bool d10 = (data >> 2) & 1;
    bool d11 = (data >> 1) & 1;
    bool d12 = data        & 1;

    // Calculate the 4 parity bits using XOR logic
    bool p1 = d3 ^ d5 ^ d7 ^ d9  ^ d11;
    bool p2 = d3 ^ d6 ^ d7 ^ d10 ^ d11;
    bool p4 = d5 ^ d6 ^ d7 ^ d12;
    bool p8 = d9 ^ d10 ^ d11 ^ d12;

    // Pack the parity and data bits into a single 12-bit word
    // Layout: P1 P2 D3 P4 D5 D6 D7 P8 D9 D10 D11 D12
    uint16_t codeword = 0;
    codeword |= ((uint16_t)p1  << 11);
    codeword |= ((uint16_t)p2  << 10);
    codeword |= ((uint16_t)d3  << 9);
    codeword |= ((uint16_t)p4  << 8);
    codeword |= ((uint16_t)d5  << 7);
    codeword |= ((uint16_t)d6  << 6);
    codeword |= ((uint16_t)d7  << 5);
    codeword |= ((uint16_t)p8  << 4);
    codeword |= ((uint16_t)d9  << 3);
    codeword |= ((uint16_t)d10 << 2);
    codeword |= ((uint16_t)d11 << 1);
    codeword |= (uint16_t)d12;

    return codeword;
}

void PulseChain::debug(uint32_t frame_id)
{
  if(frame_id%300!=0 || frame_id==0) return;//one activity per second
  //Serial.println("PulseChain debug...");
  //Serial.println("TODO: udpate dma instsruction count");
  //uint slice_num = pwm_gpio_to_slice_num(_pwm_pin);

  const uint8_t message_length=126;
  const uint8_t ecc_length=127;
    
  uint8_t message[message_length]={};
  for(uint8_t iter=0;iter<message_length;iter++)
  {
    message[iter]=iter;
  }

  RS::ReedSolomon<message_length, ecc_length> rs;
  uint8_t encoded[message_length + ecc_length];
  rs.Encode(message, encoded); 


  append_note(255,0,1);//initial sync clear
  uint16_t expand=1;
  for(int iter=0;iter<sizeof(encoded)/sizeof(encoded[0]);iter++)
  //for(int iter=255;iter>=0;iter--)
  {
    uint8_t value=encoded[iter];
    //uint16_t encoded=encodeHamming128(iter);
    //append_note(255,127,16+2*(map_graycode2(iter>>6,true)));
    //append_note(255,0,16+2*(map_graycode6(iter&0x003F,true)));
    append_note(255,127,16);//+1*((iter>>6)&0x0003));
    append_note(255,0,24+value);
    //if((iter%64==0) && (iter>0)) append_note(255,0,500);
  }
  //for(int iter=0;iter<2;iter++)
  {//blanking chars at end
    append_note(255,127,16);  //trailing pulse to denote end of message
    append_note(255,0,1);
  }
  //append_note(255,0,1); //end clearing sync
  //append_note(255,0,2000);

  /*Serial.printf("_dma_addr_scratch_0: 0x%08X (val: %3d), _dma_value_scratch: %3u, start: 0x%08X, top: 0x%08X (val: %3u), cc: 0x%08X (val: %3u)\n",_dma_addr_scratch,*(uint8_t*)_dma_addr_scratch,_dma_value_scratch,(uint32_t)&_pwm_config[_is_ping_pong][0],(uint32_t)&pwm_hw->slice[slice_num].top,pwm_hw->slice[slice_num].top,(uint32_t)&pwm_hw->slice[slice_num].cc,pwm_hw->slice[slice_num].cc);
  Serial.printf("_pwm_config[][0].period      @0x%08X: %d\n",(uint32_t)&_pwm_config[_is_ping_pong][0].period,_pwm_config[_is_ping_pong][0].period);
  Serial.printf("_pwm_config[][0].duty        @0x%08X: %d\n",(uint32_t)&_pwm_config[_is_ping_pong][0].duty,_pwm_config[_is_ping_pong][0].duty);
  Serial.printf("_pwm_config[][0].cycle_count @0x%08X: %d\n",(uint32_t)&_pwm_config[_is_ping_pong][0].cycle_count,_pwm_config[_is_ping_pong][0].cycle_count);
  Serial.printf("_pwm_config[][1].period      @0x%08X: %d\n",(uint32_t)&_pwm_config[_is_ping_pong][1].period,_pwm_config[_is_ping_pong][1].period);
  Serial.printf("_pwm_config[][1].duty        @0x%08X: %d\n",(uint32_t)&_pwm_config[_is_ping_pong][1].duty,_pwm_config[_is_ping_pong][1].duty);
  Serial.printf("_pwm_config[][1].cycle_count @0x%08X: %d\n",(uint32_t)&_pwm_config[_is_ping_pong][1].cycle_count,_pwm_config[_is_ping_pong][1].cycle_count);
  Serial.printf("_pwm_config[][2].period      @0x%08X: %d\n",(uint32_t)&_pwm_config[_is_ping_pong][2].period,_pwm_config[_is_ping_pong][2].period);
  Serial.printf("_pwm_config[][2].duty        @0x%08X: %d\n",(uint32_t)&_pwm_config[_is_ping_pong][2].duty,_pwm_config[_is_ping_pong][2].duty);
  Serial.printf("_pwm_config[][2].cycle_count @0x%08X: %d\n",(uint32_t)&_pwm_config[_is_ping_pong][2].cycle_count,_pwm_config[_is_ping_pong][2].cycle_count);*/

  play();
  //Serial.printf("_dma_addr_scratch_1: 0x%08X (val: %3d), _dma_value_scratch: %3u\n",_dma_addr_scratch,*(uint8_t*)_dma_addr_scratch,_dma_value_scratch);
  //delay(1);
  //Serial.printf("_dma_addr_scratch_2: 0x%08X (val: %3d), _dma_value_scratch: %3u\n",_dma_addr_scratch,*(uint8_t*)_dma_addr_scratch,_dma_value_scratch);
  //debug_dma_commands();
}