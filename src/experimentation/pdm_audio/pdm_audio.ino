#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <hardware/pwm.h>
#include <math.h>

// Hardware Pin Configuration
#define PDM_DATA_PIN 14
#define PDM_CLK_PIN  15
#define PIN_DEBUG_R 37
#define PIN_DEBUG_G 38

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

PIO pio_hw = pio0;
uint pio_sm = 0;
uint program_offset = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("\n=== Launching Optimized PDM2PCM Engine with 38kHz PWM ===");

  // ==========================================
  // MIMIC: Claim Unused State Machine
  // ==========================================
  program_offset = pio_add_program(pio_hw, &pdm2pcm_program);
  pio_sm = pio_claim_unused_sm(pio_hw, true);

  // ==========================================
  // MIMIC: Loop to initialize the pins
  // ==========================================
  // Pin init for PDM_CLK_PIN (GP15)
  gpio_disable_pulls(PDM_CLK_PIN);
  pio_gpio_init(pio_hw, PDM_CLK_PIN);
  gpio_set_input_enabled(PDM_CLK_PIN, true);
  gpio_disable_pulls(PDM_CLK_PIN);

  // Pin init for PDM_DATA_PIN (GP14)
  gpio_disable_pulls(PDM_DATA_PIN);
  pio_gpio_init(pio_hw, PDM_DATA_PIN);
  gpio_set_input_enabled(PDM_DATA_PIN, true);
  gpio_disable_pulls(PDM_DATA_PIN);

  // ==========================================
  // MIMIC: 1. Configure the Pin Mux for PWM Output
  // ==========================================
  gpio_set_function(PDM_CLK_PIN, GPIO_FUNC_PWM);

  // ==========================================
  // MIMIC: 2. Force the input buffer ON so PIO can "see" the pin state
  // ==========================================
  gpio_set_input_enabled(PDM_CLK_PIN, true);
  gpio_disable_pulls(PDM_CLK_PIN);

  // ==========================================
  // MIMIC: 3. Configure the PWM Peripheral
  // ==========================================
  uint slice_num = pwm_gpio_to_slice_num(PDM_CLK_PIN);
  uint channel = pwm_gpio_to_channel(PDM_CLK_PIN);

  // Divider 1.0f for maximum clock precision
  pwm_set_clkdiv(slice_num, 1.0f);

  // 150,000,000 / 2.8M = 3947.36 -> Wrap value is 3946
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
  sm_config_set_in_pins(&c, PDM_DATA_PIN);
  
  // Set shift directions, configure Autopush at 8 bits, and enable Autopull
  sm_config_set_in_shift(&c, false, true, 8);
  sm_config_set_out_shift(&c, true, true, 32);
  sm_config_set_clkdiv(&c, 1.0f);

  // Safe macro-based alternative for explicit JMP / EXEC assignment
  sm_config_set_jmp_pin(&c, PDM_DATA_PIN);

  // Join the FIFOs to maximize RX capacity for high-speed bursts
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

  // Initialize and start the state machine
  pio_sm_init(pio_hw, pio_sm, program_offset, &c);
  
  // Prime the TX FIFO with initial value before starting machine execution loops
  pio_sm_put(pio_hw, pio_sm, 0x00000000);
  
  pio_sm_set_enabled(pio_hw, pio_sm, true);

  Serial.println("PIO Core active and listening to GP15...");
  lastReportTime = millis();

  //feedback led:
    // 1. Tell the RP2350 to hand control of this GPIO pin over to the PWM hardware
  gpio_set_function(PIN_DEBUG_R, GPIO_FUNC_PWM);

  // 2. Find out which native PWM slice and channel map to your chosen GPIO pin
  slice_num = pwm_gpio_to_slice_num(PIN_DEBUG_R);
  uint chan = pwm_gpio_to_channel(PIN_DEBUG_R);

  // 3. Set the maximum counter limit (Wrap value) to establish the scale
  // A value of 999 creates a range from 0 to 999 (1000 total clock cycles)
  pwm_set_wrap(slice_num, 999);

  // 4. Set the native Duty Cycle level for the channel
  // 500 out of 999 wraps equates exactly to a 50% duty cycle
  pwm_set_chan_level(slice_num, chan, 500);

  // 5. Fire up the PWM slice hardware to begin broadcasting the wave
  pwm_set_enabled(slice_num, true);
}

void loop() {
  bool report_now = (millis() - lastReportTime >= 16);
  uint32_t current_pc = 0;
  
  if (report_now) {
    current_pc = pio_sm_get_pc(pio_hw, pio_sm);
  }

  // Read any decimated audio density sample bytes pushed into the RX FIFO
  while (!pio_sm_is_rx_fifo_empty(pio_hw, pio_sm)) {
    uint32_t fifoValue = pio_sm_get(pio_hw, pio_sm);
    //Serial.printf("%08X\n",fifoValue);
    int8_t xByteValue = (uint8_t)(fifoValue & 0xFF);

    //int16_t rawSample = (int16_t)(((xByteValue * 65535) / 255) - 32768);
    xByteValue+=128;
    //erial.println(xByteValue);
    dcFilterState=0;
    double filteredSample = (int16_t)xByteValue;// - dcFilterState;
    dcFilterState = dcFilterState + 0.0005 * filteredSample;

    squaredSum += filteredSample * filteredSample;
    totalSamplesCount++;
  }

  if (report_now) {
    uint32_t relative_instruction = (current_pc >= program_offset) ? (current_pc - program_offset) : current_pc;

    if (totalSamplesCount > 0) {
      double meanSquare = squaredSum / totalSamplesCount;
      double rms = sqrt(meanSquare);
      Serial.print("RMS Audio Level: ");
      Serial.print(rms, 2);
      Serial.print(" | Current PIO Instruction: ");
      Serial.println(relative_instruction);
      
  uint slice_num = pwm_gpio_to_slice_num(PIN_DEBUG_R);
  uint chan = pwm_gpio_to_channel(PIN_DEBUG_R);
  pwm_set_chan_level(slice_num, chan, (uint16_t)(rms*rms*10));
    } else {
      Serial.print("Waiting for clock transitions... | Stuck at PIO Instruction: ");
      Serial.println(relative_instruction);
    }

    // Reset loop variables
    squaredSum = 0;
    totalSamplesCount = 0;
    lastReportTime = millis();
  }
}
