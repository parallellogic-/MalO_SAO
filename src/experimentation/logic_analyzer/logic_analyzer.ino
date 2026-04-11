//this demo examines a logic analyzer pio block that uses run-length encoding (measuring the duration of 1s and 0s)
//useful for: reading cap touch buttons (connect all cap touch keys through separate 1 ohm resistors to a "ground" pin and drive the pin with a X kHz PWM,
//  read the PWM pin (routed as an input to, and read by, the pio block) for timing reference, and read the other 10 cap touch inputs to measure RC constant decay (how long to pass over schmitt trigger) after pwm toggle
//could use fastest half of cap touch keys to define half a normal distribution, and look for 1 or more keys X sigma beyond mean/median to infer touch
//can also be used to read the SAO GPIO pins to decode WS2812 protocol (and forward on to the PWM LEDs)
//can also be used to read the IR receiver
//PIO output is via DMA: the 11 LSbits are the state of the 11 GPIOs.  the upper 21 bits are a counter: 1 tick per 6 sys_clk ticks.
//states are only output from the PIO to the DMA when there is a state change (one or more GPIOs change high/low)
//if there are no state changes after (0x1FFFFF*6/(150MHz) ~= 83 ms), then the previous state is output again with the max counter value (heartbeat timeout)
//the demo below configures a PWM pin and then inspects how the half-period duration measured by the PIO block (duration LOW or duration HIGH) compares with the theoretical duration the PWM was set to
//buffer depth:
//at 38 khz and 16 cycles of that for a "1" --> 2.3khz, but inspects at 60 FPS, 400 state changes from IR (256 byte tweet * 8bits = 2048 states)
//ws2812 is 800khz (high and low state).  at 60 fps, that's 26k states per frame.  however, only have 48 red-green LEDs - assume rgb packing at 8-bit depth is 1153 states plus 50 us idle at end
//cap touch, placeholder of 4 kHz to get 99.99% settled after 0.23ms with 1Ohm and touching.  66 measurements (4khz/60fps) on 11 pins ~=700 states per frame
//--> use 4k state buffer to accomodate the various applications

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "logic_analyzer.pio.h"

uint8_t FIRST_PIN=2;
uint8_t PIN_COUNT=11;//max as implemented in PIO is 11 (1 PWM, 10 cap touch) to allow maximal number of bits for time counter

// RP2350 DMA Ring alignment must match buffer size in bytes (1024 * 4 = 4096)
//0x3ff is max last_read_idx value
#define RING_BUFFER_SIZE 4096//1024
uint32_t capture_buffer[RING_BUFFER_SIZE] __attribute__((aligned(16384)));//4096)));

int dma_chan;
uint32_t last_read_idx = 0;
bool last_gpio10_state = false;

void setup_pwm(uint gpio, float freq_hz) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(gpio);
    uint32_t sys_clk = clock_get_hz(clk_sys);
    
    float div = 250.0f; 
    uint32_t wrap = (uint32_t)((float)sys_clk / (div * freq_hz)) - 1;
    
    pwm_set_clkdiv(slice, div);
    pwm_set_wrap(slice, (uint16_t)wrap);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(gpio), wrap / 2);
    pwm_set_enabled(slice, true);
}

void setup() {
    Serial.begin(115200);
    // Wait for Serial or timeout after 5 seconds
    uint32_t start_ms = millis();
    while(!Serial && millis() - start_ms < 5000); 

    // 1. PWM Heartbeat on LED (10Hz)
    //setup_pwm(LED_BUILTIN, 10.0f); 
    pinMode(LED_BUILTIN,OUTPUT); digitalWrite(LED_BUILTIN,1);

    // 2. Generate Pilot Signals
    //setup_pwm(10, 10.0f);           
    /*pinMode(10,OUTPUT); digitalWrite(10,0);
    //setup_pwm(11, 10.0f * PI);      
    //setup_pwm(11, PI);      
    pinMode(11,OUTPUT); digitalWrite(11,0);
    //setup_pwm(12, 10.0f * 2.71828f);
    pinMode(12,OUTPUT); digitalWrite(12,0);
    pinMode(13,OUTPUT); digitalWrite(13,0);
    pinMode(14,OUTPUT); digitalWrite(14,0);
    pinMode(15,OUTPUT); digitalWrite(15,0);*/

    // 3. PIO Setup
    uint offset = pio_add_program(pio0, &logic_analyzer_program);
    uint sm = 0;

    // Enable input buffers so PIO can "see" the PWM signals
    for (int i = FIRST_PIN; i < FIRST_PIN+PIN_COUNT; i++) {
        pinMode(i,OUTPUT);
        digitalWrite(i,0);
        gpio_set_input_enabled(i, true);
    }

    pio_sm_config c = logic_analyzer_program_get_default_config(offset);
    sm_config_set_in_pins(&c, FIRST_PIN); //start at gpio 10
    sm_config_set_in_pin_count(&c, PIN_COUNT); //use 11 pins
    
    // RP2350 requires the FIFO to be joined to handle high-speed bursts
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    pio_sm_init(pio0, sm, offset, &c);
    pio_sm_set_enabled(pio0, sm, true);

    // 4. DMA Setup
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_c, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_c, false);
    channel_config_set_write_increment(&dma_c, true);
    channel_config_set_ring(&dma_c, true, 10+2+2); // 10+2 bits = 1024*4 words --> +2 fudge factor needed (for uint8 to uint32 adaption?))
    channel_config_set_dreq(&dma_c, pio_get_dreq(pio0, sm, false));

    dma_channel_configure(dma_chan, &dma_c, capture_buffer, &pio0->rxf[sm], 0xFFFFFFFF, true);

    Serial.println("--- RP2350 Analyzer Online ---");

    if(false)
    {//1khz
      analogWriteFreq(1000); 
      // Optional: Set resolution to 10-bit (0-1023) for finer control
      analogWriteRange(1000);//23); 
      pinMode(10, OUTPUT);
      // 512 is 50% duty cycle when range is 1023
      analogWrite(10, 500);//512); 
    }
    if(true){//4khz
        // 1. Initialize the GPIO for PWM function
        gpio_set_function(10, GPIO_FUNC_PWM);

        // 2. Identify which PWM slice is connected to this pin
        uint slice_num = pwm_gpio_to_slice_num(10);

        // 3. Set the clock divider to 1.0 (No division)
        // This ensures the PWM counter increments exactly once per sys_clk cycle.

        if(true)
        {
          pwm_set_clkdiv(slice_num, 100.0f);
          // 4. Set the Wrap Value (TOP)
          // The frequency will be sys_clk / (WRAP + 1).
          // For a divisor of 24,000, set wrap to 23,999.
          pwm_set_wrap(slice_num, 37499);//150mhz / 37.5k = 4khz

          // 5. Set the Duty Cycle to exactly 50%
          // Level should be half of (WRAP + 1)
          pwm_set_gpio_level(10, 37500/2);
        }
        if(false)
        {
          pwm_set_clkdiv(slice_num, 1.0f);
          pwm_set_wrap(slice_num, 999);//150mhz / 1k = 150khz
          pwm_set_gpio_level(10, 1000/2);
        }

        // 6. Start the PWM slice
        pwm_set_enabled(slice_num, true);
    }
}

void loop() {
    // 1. CONSTANTLY toggle pins so the PIO has something to see
    // This must be OUTSIDE the while(last_read_idx != current_idx) loop
    bool fast_toggle = (millis() / 80) % 2;
    bool slow_toggle = (millis() / 500) % 2;
    
    //digitalWrite(10, fast_toggle); // 1Hz Pilot
    //digitalWrite(11, slow_toggle); // 50Hz Test
    //digitalWrite(12, slow_toggle);
    digitalWrite(LED_BUILTIN, slow_toggle);

    // 2. Process whatever the PIO captured
    uint32_t write_addr = dma_hw->ch[dma_chan].write_addr;
    uint32_t current_idx = (write_addr - (uintptr_t)capture_buffer) / 4;
    //if (last_read_idx != current_idx) {
        while (last_read_idx != current_idx) {
            uint32_t data = capture_buffer[last_read_idx];
            //uint16_t pins = (data >> 21) & 0x7FF;
            //uint32_t counter_raw = data & 0x1FFFFF;
            uint16_t pins = data & 0x7FF;
            uint32_t counter_raw = data>>11;
            
            // Ticks elapsed = Start (0x1FFFFF) - End (counter_raw)
            //uint32_t elapsed_ticks = 0x1FFFFF - counter_raw;
            uint32_t elapsed_ticks = 0x200000 - counter_raw;

            String binString = String(pins, BIN);
            while(binString.length() < 11) binString = "0" + binString;

            // 150MHz / 5 cycles = 30,000 ticks per ms
            //Serial.printf("Edge! ms: %03.3f | Pins: %s\n", 
            //              (float)elapsed_ticks / 30000.0f, binString.c_str());
            Serial.printf("Edge! current_idx 0x%08x | last_read_idx 0x%08x | count: 0x%06x | Pins: %s\n", 
                          current_idx,last_read_idx, elapsed_ticks, binString.c_str());

            last_read_idx = (last_read_idx + 1) % RING_BUFFER_SIZE;
        }
    //}
}
