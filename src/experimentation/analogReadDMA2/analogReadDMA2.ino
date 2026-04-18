/*
 * DIRECTION:
 * - Configure gpio 41-42 for round-robing ADC capture into a DMA. 
 * - Do not use PIO. 
 * - Do not start/stop/interrupt the DMA; it should write into a circular buffer.
 * - Data should be read out of that buffer from the proper address as needed 
 *   (use the DMA's memory pointer to determine where the last data was recently saved).
 * - Configure the ADC to sample at 60*512 samples per second (30,720 Hz total).
 * - 60 times per second: compute mean and standard deviation for the 256 most 
 *   recent samples of BOTH GPIO 41 and GPIO 42.
 * - Once per second, report these stats to the terminal.
 * gpio41 reads the hall effect sensor properly
 */

#include <Arduino.h>
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

// Configuration
#define ADC_PIN_1 41 
#define ADC_PIN_2 42 
#define SAMPLES_PER_CHAN 256
#define TOTAL_SAMPLES (SAMPLES_PER_CHAN * 2) 
#define TOTAL_SAMPLE_RATE (60 * 512) // 30,720 Hz
#define ADC_CLOCK 48000000

// DMA Circular Buffer
// Must be aligned to the power-of-two size in bytes (512 samples * 2 bytes = 1024)
uint16_t adc_raw_buffer[TOTAL_SAMPLES] __attribute__((aligned(1024)));

int dma_chan;
struct Stats {
    float mean;
    float stdev;
};

Stats stats41, stats42;

void setup() {
    Serial.begin(115200);
    while(!Serial); // Wait for terminal

    // 1. Initialize ADC
    adc_init();
    adc_gpio_init(ADC_PIN_1);
    adc_gpio_init(ADC_PIN_2);

    // RP2350B ADC mapping: GPIO 40-47 map to ADC channels 4-11
    // GPIO 41 = Channel 5, GPIO 42 = Channel 6
    adc_select_input(1); 
    adc_set_round_robin(0x06); // Binary 01100000 (Bits 5 and 6)

    adc_fifo_setup(
        true,    // Write to FIFO
        true,    // Enable DMA requests
        1,       // DREQ threshold
        false,   // No error bit
        false    // Full 12-bit samples (2 bytes each)
    );

    // Set sample rate (clk_div = 48MHz / target - 1)
    adc_set_clkdiv((float)ADC_CLOCK / TOTAL_SAMPLE_RATE - 1.0f);

    // 2. Configure DMA
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    // Hardware Ring Buffer: Wrap at 1024 bytes (2^10)
    channel_config_set_ring(&cfg, true, 10); 

    dma_channel_configure(
        dma_chan, &cfg,
        adc_raw_buffer,    // Destination
        &adc_hw->fifo,     // Source
        0xFFFFFFFF,        // "Infinite" transfers
        true               // Start
    );

    adc_run(true);
}

void compute_stats(uint32_t latest_idx, int channel_offset, Stats &target) {
    double sum = 0;
    double sq_sum = 0;

    latest_idx=latest_idx-(latest_idx%2);//align with 2-byte index

    for (int i = 0; i < SAMPLES_PER_CHAN; i++) {
        // Step back by 2 to stay on the same pin in the round-robin buffer
        // Apply channel_offset (0 for first pin, 1 for second pin)
        int idx = (latest_idx - (i * 2) - channel_offset + TOTAL_SAMPLES) % TOTAL_SAMPLES;
        uint16_t val = adc_raw_buffer[idx];
        sum += val;
        sq_sum += (double)val * val;
    }

    target.mean = (float)(sum / SAMPLES_PER_CHAN);
    float variance = (float)((sq_sum / SAMPLES_PER_CHAN) - (target.mean * target.mean));
    target.stdev = sqrt(max(0.0f, variance));
}

void loop() {
    static uint32_t last_proc_time = 0;
    static uint32_t last_print_time = 0;

    // Process at 60Hz (approx 16.66ms)
    if (micros() - last_proc_time >= 16666) {
        last_proc_time = micros();

        // Determine current DMA write position
        // The write_addr points to where the DMA is ABOUT to write.
        uint32_t current_write_addr = dma_hw->ch[dma_chan].write_addr;
        uint32_t latest_idx = ((current_write_addr - (uint32_t)adc_raw_buffer) / 2) - 1;

        // GPIO 41 (Channel 5) is the first in round-robin order
        compute_stats(latest_idx, 0, stats41);
        // GPIO 42 (Channel 6) is the second
        compute_stats(latest_idx, 1, stats42);

        // Report once per second
        if (millis() - last_print_time >= 1000) {
            last_print_time = millis();
            Serial.println("--- 1 Second Report ---");
            Serial.printf("GPIO 41 (ADC5) -> Mean: %7.2f, StDev: %7.2f\n", stats41.mean, stats41.stdev);
            Serial.printf("GPIO 42 (ADC6) -> Mean: %7.2f, StDev: %7.2f\n", stats42.mean, stats42.stdev);
            Serial.println();
        }
    }
}
