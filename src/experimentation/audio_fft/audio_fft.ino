//for rp2350B give me arduino ide code that:
//sets ADC to sample at roughly 61.44 kSps (2x pins at 30.702 kSps).  setup a DMA to record data round-robin from analog GPIOs 41 and 42 on rp2350.  
//Store the readings in a DMA buffer in memory.  At 60 Hz, average the readings from gpio41 and store that as "hall_effect_reading".  
//Apply a simple window function to the time series data from gpio42 to improve spectral purity, then use a built-in library to perform an FFT on the data 
//(change the 61.44 kSps as needed to get an even power of 2 samples in 1/60ths of a second) - report the DC to 15kHz-ish FFT as "audio_reading".
//do not pause the adc, keep it running continuously at all times (maybe double the buufer size - half for reading for this frame, and the other half that the ADC can write into)
//ex. use the dma pointer to determine which round-robin section of the dma to reference.  operate on the exact 512 most recent recent samples per pin when the 60Hz output operation occurs
//print out the single-core utilization as a percent (time spent doing FFT math vs waiting for the next 1/60 second event)
//add a bool control to toggle between 61.44 kSps and 2*61.44 kSps (in the higher rate mode, 1024 samples are processed by the FFT).  I want to see how this affects utilization

#include <Arduino.h>
#include <hardware/adc.h>
#include <hardware/dma.h>
#include <arduinoFFT.h>

// FFT & Sampling Constants
#define SAMPLES 1024             // Samples per pin (Power of 2)
#define TOTAL_SAMPLES (SAMPLES * 2) 
#define SAMPLING_FREQ_PER_PIN 61440 // Resulting in exactly ~60Hz window
#define TOTAL_SAMPLING_FREQ (SAMPLING_FREQ_PER_PIN * 2)

// ADC Pins for RP2350B
#define PIN_HALL 41 // GPIO 41 (ADC1)
#define PIN_AUDIO 42 // GPIO 42 (ADC2)

uint16_t adc_buffer[TOTAL_SAMPLES];
float vReal[SAMPLES];
float vImag[SAMPLES];

ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, SAMPLING_FREQ_PER_PIN);
int dma_chan;

void setup() {
  Serial.begin(115200);
  
  // 1. Initialize ADC
  adc_init();
  adc_gpio_init(PIN_HALL);
  adc_gpio_init(PIN_AUDIO);
  
  // Set round-robin for ADC1 and ADC2 (bits 1 and 2)
  adc_set_round_robin(0x06); 
  adc_select_input(1); // Start with ADC1 (GPIO 41)

  adc_fifo_setup(
    true,    // Write each conversion to FIFO
    true,    // Enable DMA request
    1,       // DREQ when at least 1 sample is in FIFO
    false,   // No error bit
    false    // Keep 12-bit (don't shift to 8)
  );

  // Set clock div: 48MHz / Total_Freq = clock_div
  // For 122.88kHz total: 48,000,000 / 122,880 = 390.625
  adc_set_clkdiv(390); 

  // 2. Setup DMA
  dma_chan = dma_claim_unused_channel(true);
  dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
  channel_config_set_read_increment(&cfg, false);
  channel_config_set_write_increment(&cfg, true);
  channel_config_set_dreq(&cfg, DREQ_ADC);

  dma_channel_configure(
    dma_chan, &cfg,
    adc_buffer,    // Destination
    &adc_hw->fifo, // Source
    TOTAL_SAMPLES, // Count
    true           // Start immediately
  );

  adc_run(true);
}

void loop() {
  // Wait for DMA to finish the 60Hz window
  dma_channel_wait_for_finish_blocking(dma_chan);
  adc_run(false); // Pause ADC

  long hall_sum = 0;
  for (int i = 0; i < TOTAL_SAMPLES; i += 2) {
    // GPIO 41 readings are at even indices
    hall_sum += adc_buffer[i];
    
    // GPIO 42 readings are at odd indices for FFT
    vReal[i/2] = (float)adc_buffer[i+1];
    vImag[i/2] = 0.0;
  }

  // Hall Effect Processing
  float hall_effect_reading = (float)hall_sum / SAMPLES;

  // Audio Processing (FFT)
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();
  
  // Get a representative value for "audio_reading" (e.g., peak magnitude)
  float audio_reading = 0;
  for(int i = 2; i < (SAMPLES/2); i++) { // Skip DC component
    if(vReal[i] > audio_reading) audio_reading = vReal[i];
  }

  // Output
  Serial.print("Hall: "); Serial.print(hall_effect_reading);
  Serial.print(" | Audio Peak: "); Serial.println(audio_reading);

  // Restart DMA for next 60Hz window
  dma_channel_set_write_addr(dma_chan, adc_buffer, true);
  adc_run(true);
}
