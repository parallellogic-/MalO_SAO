/*
  RP2350B 44.1kHz 12-bit ADC to DMA
  Output: 8-bit normalized (0x80 biased)
*/

#include "hardware/adc.h"
#include "hardware/dma.h"

#define ADC_NUM 2                // GPIO 29 is ADC 4 (Physical Pin 42)
#define RECORDING_PIN 42         
#define SAMPLE_RATE (44100/2)
#define BUFFER_SIZE (88200*2)        // 2 seconds

uint16_t capture_buffer[BUFFER_SIZE]; // 12-bit raw storage
int dma_chan;

const int buzzerPin = 25; // Connect positive pin of buzzer to D9

void setup() {
  Serial.begin(1000000);
  while(!Serial);

  pinMode(buzzerPin, OUTPUT);

  adc_init();
  adc_gpio_init(RECORDING_PIN);
  adc_select_input(ADC_NUM); 

  adc_fifo_setup(
    true,    // Write to FIFO
    true,    // Enable DMA
    1,       // DREQ at 1 sample
    false,   // No error bit
    false    // Keep full 12-bit (No bit-shifting)
  );

  //adc_set_clkdiv(1088.43f); // 44.1kHz
  adc_set_clkdiv(48000000.0f/(float)SAMPLE_RATE - 1.0f);

  dma_chan = dma_claim_unused_channel(true);
  dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
  
  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16); // 16-bit for 12-bit data
  channel_config_set_read_increment(&cfg, false);
  channel_config_set_write_increment(&cfg, true);
  channel_config_set_dreq(&cfg, DREQ_ADC);

  dma_channel_configure(
    dma_chan, &cfg,
    capture_buffer,  // Destination
    &adc_hw->fifo,   // Source
    BUFFER_SIZE,     // Count
    false            // Don't start
  );

  //Serial.println("Ready. Send 'r' to record.");
}

void recordAudio() {
  //Serial.println("RECORDING...");
  
  adc_fifo_drain();
  adc_run(true);
  dma_channel_start(dma_chan);


  const int numNotes = 7;
  int aNotes[] = {55, 110, 220, 440, 880, 1760, 3520};
  for (int i = 0; i < numNotes; i++) {
    // tone(pin, frequency) produces a 50% duty cycle square wave
    delay(1000);
    tone(buzzerPin, aNotes[i]);
  }

  dma_channel_wait_for_finish_blocking(dma_chan);
  noTone(buzzerPin);

  adc_run(false);
  dma_channel_abort(dma_chan);

  // Calculate Mean
  uint64_t sum = 0;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    sum += capture_buffer[i];
  }
  uint32_t mean = sum / BUFFER_SIZE;

  //Serial.println("DONE. Dumping Data:");
  uint32_t this_min=-1;
  uint32_t this_max=0;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    // 1. Subtract mean (DC Offset removal)
    // 2. Scale 12-bit (0-4095) down to 8-bit (0-255)
    // 3. Add 0x80 bias
    if(capture_buffer[i]>this_max) this_max=capture_buffer[i];
    if(capture_buffer[i]<this_min) this_min=capture_buffer[i];
    int32_t sample = capture_buffer[i] - mean;
    
    // Scale: (sample / 16) shifts 12-bit range to 8-bit range
    int16_t biased = sample + 0x80;

    // Constrain to 0-255 to prevent wrapping errors
    if (biased > 255) biased = 255;
    if (biased < 0)   biased = 0;

    Serial.println((uint8_t)biased);
  }
  Serial.println("Stats:");
  Serial.println(this_min);
  Serial.println(this_max);
  Serial.println(this_max-this_min);
}

void loop() {
  if (Serial.available() > 0) {
    if (Serial.read() == 'r') recordAudio();
  }
}
