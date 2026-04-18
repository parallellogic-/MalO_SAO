/*
  RP2350B Analog Audio Recorder
  
  1. Connect your microphone output (pre-amplified) to Pin 41.
  2. Open Serial Monitor to see raw data during recording.
  3. After "Recording Finished", copy the raw data (excluding text) from 
     the Serial Monitor to a text file (e.g., audio.txt).
  4. Use an audio editor like Audacity: File > Import > Raw Data.
  5. Settings: Signed 16-bit PCM, 1 Channel, 8000 Hz (or matched rate).
*/

#include <Arduino.h>

// Use high RAM on RP2350 for audio buffer
#define BUFFER_SIZE 60000 
uint16_t audioBuffer[BUFFER_SIZE];
const int analogPin = 41;
const int sampleRate = 8000; // 8kHz for voice
unsigned long interval = 1000000 / sampleRate;

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for connection

  // Configure Analog Pin 41 (ADC on RP2350)
  pinMode(analogPin, INPUT);
  
  // Set ADC resolution (10-bit or 12-bit)
  analogReadResolution(12);

  Serial.println("--- Starting Record in 2s ---");
  delay(2000);
  Serial.println("RECORDING");

  // Record loop
  unsigned long startTime = micros();
  for (int i = 0; i < BUFFER_SIZE; i++) {
    unsigned long nextSample = startTime + (i * interval);
    while (micros() < nextSample); // Wait for next sample time
    
    audioBuffer[i] = analogRead(analogPin);
  }

  Serial.println("RECORDING FINISHED");
  Serial.println("--- RAW DATA START ---");
  
  // Dump data to Serial in a format Audacity can import
  for (int i = 0; i < BUFFER_SIZE; i++) {
    Serial.println(audioBuffer[i]);
  }
  
  Serial.println("--- RAW DATA END ---");
}

void loop() {
  // Nothing here
}
