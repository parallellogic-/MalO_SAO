//give me an arduino sketch that does that following for rp2350b:
//- configure gpio 41-42 for round-robing ADC capture into a DMA.  do not use PIO.  do not start/stop/interrupt the DMA, it should write into a circular buffer and data should be read out of that buffer from the proper address as needed (use the DMA's memory poitner to determine where the last data was recently saved and work from there).
//- configure the ADC to sample at 60*512 samples per second.  That is: 60*256 from gpio 41, and 60*256 from gpio 42.
//- 60 times per second do the following:
//  - from 256 most recent ADC samples from gpio 41, compute the mean and standard deviation.
//  - from the 256 recent samples from gpio 42, copy the data (applying a window in the process, like a raised cos or blackman), and perform an FFT using an arduino built-in library.
//  - do the FFT computation again using the data from gpio 42, but offset by 128 samples into the past.  Sum the two FFTs.  The output is an fft_vector of 256 values from DC to max_freq.  print the value from the FFT bins that contains 440 Hz and 1kHz to the terminal once a second.
//not seing clear response at 440/1khz

#include <Arduino.h>
#include <arduinoFFT.h>
#include "hardware/adc.h"
#include "hardware/dma.h"

// Configuration
#define ADC_PIN_1 41 // GPIO 41
#define ADC_PIN_2 42 // GPIO 42
#define SAMPLES_PER_CHAN 256
#define TOTAL_SAMPLES (SAMPLES_PER_CHAN * 2) // Round-robin
#define SAMPLE_RATE_PER_CHAN (60 * 256)      // 15,360 Hz
#define TOTAL_SAMPLE_RATE (60 * 512)         // 30,720 Hz
#define ADC_CLOCK 48000000

// DMA Buffer: Must be aligned to the buffer size for hardware ring wrapping
// Buffer size is 512 samples * 2 bytes (uint16_t) = 1024 bytes (10 bits)
uint16_t adc_raw_buffer[TOTAL_SAMPLES] __attribute__((aligned(1024)));

// FFT Objects
ArduinoFFT<float> FFT = ArduinoFFT<float>();
float vReal[SAMPLES_PER_CHAN];
float vImag[SAMPLES_PER_CHAN];
float fft_vector[SAMPLES_PER_CHAN]; // DC to Max

int dma_chan;
dma_channel_config cfg;

void setup() {
    Serial.begin(115200);
    
    // 1. Initialize ADC
    adc_init();
    adc_gpio_init(ADC_PIN_1);
    adc_gpio_init(ADC_PIN_2);
    
    // Select inputs for round-robin (GPIO 41=ADC5, 42=ADC6 on RP2350B)
    // Note: Verify ADC channel mapping for RP2350B specific pins
    adc_select_input(ADC_PIN_1 - 36); // Typical mapping offset
    adc_set_round_robin(0x60);       // Bits 5 and 6 for channels 5 & 6
    
    adc_fifo_setup(
        true,    // Write to FIFO
        true,    // Enable DMA request
        1,       // DREQ threshold
        false,   // No error bit
        false    // Full 12-bit samples
    );

    // Set sample rate
    // clk_div = (48MHz / target_rate) - 1
    float clk_div = (float)ADC_CLOCK / TOTAL_SAMPLE_RATE - 1;
    adc_set_clkdiv(clk_div);

    // 2. Configure DMA for Circular Buffer
    dma_chan = dma_claim_unused_channel(true);
    cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    // Enable ring buffer: wrap at 1024 bytes (2^10)
    channel_config_set_ring(&cfg, true, 10); 

    dma_channel_configure(
        dma_chan, &cfg,
        adc_raw_buffer,    // Destination
        &adc_hw->fifo,     // Source
        0xFFFFFFFF,        // "Infinite" transfers
        true               // Start immediately
    );

    adc_run(true);
}

void loop() {
    static uint32_t last_proc_time = 0;
    static uint32_t last_print_time = 0;

    // Run at 60Hz
    if (micros() - last_proc_time >= 16666) {
        last_proc_time = micros();

        // Find current DMA write position
        uint32_t current_idx = (uint32_t)dma_hw->ch[dma_chan].write_addr;
        uint32_t offset = (current_idx - (uint32_t)adc_raw_buffer) / 2;

        // Process GPIO 41 (Channel 0 in Round Robin pair)
        process_stats(offset);

        // Process GPIO 42 (Channel 1 in Round Robin pair) with overlap
        process_fft(offset);

        // Print specific bins once per second
        if (millis() - last_print_time >= 1000) {
            last_print_time = millis();
            float bin_res = (float)SAMPLE_RATE_PER_CHAN / SAMPLES_PER_CHAN;
            int bin_440 = (int)(440 / bin_res);
            int bin_1k = (int)(1000 / bin_res);
            Serial.printf("440Hz: %.2f | 1kHz: %.2f\n", fft_vector[bin_440], fft_vector[bin_1k]);
        }
    }
}

void process_stats(uint32_t dma_offset) {
    float sum = 0, sq_sum = 0;
    for (int i = 0; i < SAMPLES_PER_CHAN; i++) {
        // Step by 2 to grab only GPIO 41 samples from round-robin
        int idx = (dma_offset - (i * 2) + TOTAL_SAMPLES) % TOTAL_SAMPLES;
        float val = adc_raw_buffer[idx];
        sum += val;
        sq_sum += val * val;
    }
    float mean = sum / SAMPLES_PER_CHAN;
    float stdev = sqrt((sq_sum / SAMPLES_PER_CHAN) - (mean * mean));
    // Usage: use mean/stdev as needed
}

void process_fft(uint32_t dma_offset) {
    // Clear output vector for summation
    for(int i=0; i<SAMPLES_PER_CHAN; i++) fft_vector[i] = 0;

    // Perform two FFTs: Current and 128-sample offset (50% overlap)
    for (int shift = 0; shift < 2; shift++) {
        int back_offset = shift * 128 * 2; // *2 because of round-robin interlacing
        
        for (int i = 0; i < SAMPLES_PER_CHAN; i++) {
            int idx = (dma_offset - (i * 2) - back_offset + TOTAL_SAMPLES) % TOTAL_SAMPLES;
            vReal[i] = (float)adc_raw_buffer[idx];
            vImag[i] = 0;
        }

        FFT.windowing(vReal, SAMPLES_PER_CHAN, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
        FFT.compute(vReal, vImag, SAMPLES_PER_CHAN, FFT_FORWARD);
        FFT.complexToMagnitude(vReal, vImag, SAMPLES_PER_CHAN);

        for (int i = 0; i < SAMPLES_PER_CHAN; i++) {
            fft_vector[i] += vReal[i];
        }
    }
}
