#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "logic_analyzer.pio.h"

// RP2350 DMA Ring alignment must match buffer size in bytes (1024 * 4 = 4096)
#define RING_BUFFER_SIZE 1024
uint32_t capture_buffer[RING_BUFFER_SIZE] __attribute__((aligned(4096)));

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
    pinMode(10,OUTPUT); digitalWrite(10,0);
    //setup_pwm(11, 10.0f * PI);      
    //setup_pwm(11, PI);      
    pinMode(11,OUTPUT); digitalWrite(11,0);
    //setup_pwm(12, 10.0f * 2.71828f);
    pinMode(12,OUTPUT); digitalWrite(12,0);
    pinMode(13,OUTPUT); digitalWrite(13,0);
    pinMode(14,OUTPUT); digitalWrite(14,0);
    pinMode(15,OUTPUT); digitalWrite(15,0);

    // 3. PIO Setup
    uint offset = pio_add_program(pio0, &logic_analyzer_program);
    uint sm = 0;

    // Enable input buffers so PIO can "see" the PWM signals
    for (int i = 10; i <= 10+12-1; i++) {
        gpio_set_input_enabled(i, true);
    }

    pio_sm_config c = logic_analyzer_program_get_default_config(offset);
    sm_config_set_in_pins(&c, 10); 
    
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
    channel_config_set_ring(&dma_c, true, 10); // 10 bits = 1024 words
    channel_config_set_dreq(&dma_c, pio_get_dreq(pio0, sm, false));

    dma_channel_configure(dma_chan, &dma_c, capture_buffer, &pio0->rxf[sm], 0xFFFFFFFF, true);

    Serial.println("--- RP2350 Analyzer Online ---");
}

void loop() {
    // 1. CONSTANTLY toggle pins so the PIO has something to see
    // This must be OUTSIDE the while(last_read_idx != current_idx) loop
    bool fast_toggle = (millis() / 1) % 2;
    bool slow_toggle = (millis() / 500) % 2;
    
    digitalWrite(10, fast_toggle); // 1Hz Pilot
    //digitalWrite(11, fast_toggle); // 50Hz Test
    //digitalWrite(12, fast_toggle);
    digitalWrite(LED_BUILTIN, slow_toggle);

    // 2. Process whatever the PIO captured
    uint32_t write_addr = dma_hw->ch[dma_chan].write_addr;
    uint32_t current_idx = (write_addr - (uintptr_t)capture_buffer) / 4;
    if (last_read_idx != current_idx) {
        while (last_read_idx != current_idx) {
            uint32_t data = capture_buffer[last_read_idx];
            uint16_t pins = (data >> 21) & 0x7FF;
            uint32_t counter_raw = data & 0x1FFFFF;
            
            // Ticks elapsed = Start (0x1FFFFF) - End (counter_raw)
            uint32_t elapsed_ticks = 0x1FFFFF - counter_raw;

            String binString = String(pins, BIN);
            while(binString.length() < 11) binString = "0" + binString;

            // 150MHz / 5 cycles = 30,000 ticks per ms
            //Serial.printf("Edge! ms: %03.3f | Pins: %s\n", 
            //              (float)elapsed_ticks / 30000.0f, binString.c_str());
            Serial.printf("Edge! count: 0x%08x | Pins: %s\n", 
                          elapsed_ticks, binString.c_str());

            last_read_idx = (last_read_idx + 1) % RING_BUFFER_SIZE;
        }
    }
}
