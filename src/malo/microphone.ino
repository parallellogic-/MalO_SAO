#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <hardware/pwm.h>
#include <math.h>
#include "microphone.h"

// Hardware Pin Configuration
//#define _data_pin 14
//#define _clock_pin  15
//#define PIN_DEBUG_R 37
//#define PIN_DEBUG_G 38

// Global Audio DSP Metrics Accumulators
unsigned long lastReportTime = 0;
double squaredSum = 0;
long totalSamplesCount = 0;

// DC Offset Rolling Tracking Filter State
double dcFilterState = 0;

// Your Optimized PIO Instructions with 8-bit initialization and autopush
static const uint16_t pdm2pcm_optimized_instructions[] = {
    0xe020, //  0: set    x, 0                       
    0xa0e9, //  1: mov    osr, !x                    
    0x6028, //  2: out    x, 8                       
    0xa041, //  3: mov    y, x                       
    0x208f, //  4: wait   1 gpio, 15                 
    0x200f, //  5: wait   0 gpio, 15                 
    0x00c8, //  6: jmp    pin, 8                     
    0x0048, //  7: jmp    x--, 8                     
    0x0084, //  8: jmp    y--, 4                     
    0x4028, //  9: in     x, 8                       
    0x0000, // 10: jmp    0         
};

static const struct pio_program pdm2pcm_program = {
    .instructions = pdm2pcm_optimized_instructions,
    .length = 11,
    .origin = -1
};

Microphone::Microphone(PIO pio,uint8_t data_pin,uint8_t clock_pin): _pio(pio), _data_pin(data_pin), _clock_pin(clock_pin){}

void Microphone::begin() {

  // ==========================================
  // MIMIC: Claim Unused State Machine
  // ==========================================
  uint program_offset = pio_add_program(_pio, &pdm2pcm_program);//precon: only one mic, and calling this once
  _sm = pio_claim_unused_sm(_pio, true);

  // ==========================================
  // MIMIC: Loop to initialize the pins
  // ==========================================
  // Pin init for _clock_pin (GP15)
  gpio_disable_pulls(_clock_pin);
  pio_gpio_init(_pio, _clock_pin);
  gpio_set_input_enabled(_clock_pin, true);
  gpio_disable_pulls(_clock_pin);

  // Pin init for _data_pin (GP14)
  gpio_disable_pulls(_data_pin);
  pio_gpio_init(_pio, _data_pin);
  gpio_set_input_enabled(_data_pin, true);
  gpio_disable_pulls(_data_pin);

  // ==========================================
  // MIMIC: 1. Configure the Pin Mux for PWM Output
  // ==========================================
  gpio_set_function(_clock_pin, GPIO_FUNC_PWM);

  // ==========================================
  // MIMIC: 2. Force the input buffer ON so PIO can "see" the pin state
  // ==========================================
  gpio_set_input_enabled(_clock_pin, true);
  gpio_disable_pulls(_clock_pin);

  // ==========================================
  // MIMIC: 3. Configure the PWM Peripheral
  // ==========================================
  uint slice_num = pwm_gpio_to_slice_num(_clock_pin);
  uint channel = pwm_gpio_to_channel(_clock_pin);

  // Divider 1.0f for maximum clock precision
  pwm_set_clkdiv(slice_num, 1.0f);

  // 150,000,000 / 2.8MSps = 
  uint32_t wrap_value = 63;
  pwm_set_wrap(slice_num, wrap_value);                 // Set frequency period
  pwm_set_chan_level(slice_num, channel, wrap_value / 2); // 50% Duty cycle
  pwm_set_enabled(slice_num, true);                    // Start generating PWM

  // ==========================================
  // MIMIC: 4. Configure the PIO State Machine to listen
  // ==========================================
  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap(&c, program_offset + 0, program_offset + 10);

  // Set the IN pins to start at our Data pin
  sm_config_set_in_pins(&c, _data_pin);
  
  // Set shift directions, configure Autopush at 8 bits, and enable Autopull
  sm_config_set_in_shift(&c, false, true, 8);
  sm_config_set_out_shift(&c, true, true, 32);
  sm_config_set_clkdiv(&c, 1.0f);

  // Safe macro-based alternative for explicit JMP / EXEC assignment
  sm_config_set_jmp_pin(&c, _data_pin);

  // Join the FIFOs to maximize RX capacity for high-speed bursts
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

  // Initialize and start the state machine
  pio_sm_init(_pio, _sm, program_offset, &c);
  
  // Prime the TX FIFO with initial value before starting machine execution loops
  pio_sm_put(_pio, _sm, 0x00000000);
  
//fill in _data_dma claim, ring buffer as a function of MICROPHONE_SAMPLE_BUFFER (DO NOT hard-code power-of-2 ring buffer size), dma start with dreq on the above pio
  // ==========================================
  // NEW: Configure DMA Ring Buffer Pipeline
  // ==========================================
  _data_dma = dma_claim_unused_channel(true);
  dma_channel_config dma_c = dma_channel_get_default_config(_data_dma);
  
  // Set transfer properties (8-bit data transfers coming from PIO RX FIFO)
  channel_config_set_transfer_data_size(&dma_c, DMA_SIZE_8);
  channel_config_set_read_increment(&dma_c, false);
  channel_config_set_write_increment(&dma_c, true);
  
  // Tie DMA pacing to the PIO State Machine RX Data Request (DREQ)
  channel_config_set_dreq(&dma_c, pio_get_dreq(_pio, _sm, false));

  // Dynamically calculate the ring buffer size bits (log2 of MICROPHONE_SAMPLE_BUFFER)
  // For 512 bytes, this yields 9 (meaning a 512-byte naturally aligned window)
  channel_config_set_ring(&dma_c, true, __builtin_ctz(sizeof(_cyclical_buffer)));

  // Initialize tracking pointer to the start of our memory buffer
  _last_dma_update_address = (uint32_t)&_cyclical_buffer[0];

  // Configure and instantly kick-off infinite looping DMA engine
  dma_channel_configure(
    _data_dma,
    &dma_c,
    &_cyclical_buffer[0],         // Write address destination target
    &_pio->rxf[_sm],         // Source hardware pointer address
    0xFFFFFFFF,              // Maximum transfers (essentially running forever)
    true                     // Trigger/start immediately
  );

  pio_sm_set_enabled(_pio, _sm, true);
}

void Microphone::update(){
  //iterate over every address between _last_dma_update_address and current_dma_address, add val*val to running sum, normalize by number of samples, save as _mean_square

  // Read the active live write destination address register from the DMA hardware
  uint32_t current_dma_address = dma_hw->ch[_data_dma].write_addr;//get the latest _data_dma write address
  
  // If no new samples have landed since the last loop iteration, abort early to save CPU
  if (current_dma_address == _last_dma_update_address) {
    return;
  }

  uint32_t start_ptr = _last_dma_update_address;
  uint32_t end_ptr = current_dma_address;
  
  uint32_t buffer_start_addr = (uint32_t)&_cyclical_buffer[0];
  uint32_t buffer_end_addr = buffer_start_addr + sizeof(_cyclical_buffer);

  uint32_t squared_sum = 0;
  uint32_t samples_processed = 0;

  // Scenario A: Linear read (The DMA pointer did not wrap around the ring boundary)
  if (end_ptr > start_ptr) {
    uint8_t* ptr = (uint8_t*)start_ptr;
    while ((uint32_t)ptr < end_ptr) {
      // 1. Read raw byte exactly as it exists in memory (0 to 255 spectrum)
      uint8_t raw_byte = *ptr; 

      // 2. Mirror your old working code calculation exactly: add 128 to shift bias
      int32_t filtered_sample = (int32_t)raw_byte - 128; 

      // 3. Compile square sum cleanly inside 64-bit register space
      squared_sum += ((int64_t)filtered_sample * filtered_sample);
      
      samples_processed++;
      ptr++;
    }
  } 
  // Scenario B: Wrapped read (The DMA pointer wrapped back around to the front of the array)
  else {
    // Phase 1: Process from previous position up to the absolute end of the array buffer
    uint8_t* ptr = (uint8_t*)start_ptr;
    while ((uint32_t)ptr < buffer_end_addr) {
      uint8_t raw_byte = *ptr; 
      int32_t filtered_sample = (int32_t)raw_byte - 128; 
      squared_sum += ((int64_t)filtered_sample * filtered_sample);
      samples_processed++;
      ptr++;
    }
    // Phase 2: Process from the beginning of array buffer up to the current active DMA target
    ptr = (uint8_t*)buffer_start_addr;
    while ((uint32_t)ptr < end_ptr) {
      uint8_t raw_byte = *ptr; 
      int32_t filtered_sample = (int32_t)raw_byte - 128; 
      squared_sum += ((int64_t)filtered_sample * filtered_sample);
      samples_processed++;
      ptr++;
    }
  }

  // Compute Mean Square value safely to protect against any edge case division-by-zero errors
  if (samples_processed > 0) {
    _mean_square = ((float)squared_sum / (float)samples_processed);
  }

  // Save progress tracking address for the next frame iteration pass
  _last_dma_update_address = current_dma_address;
}


void Microphone::end(){
  
}

float Microphone::get_mean_square(){ return _mean_square; }

