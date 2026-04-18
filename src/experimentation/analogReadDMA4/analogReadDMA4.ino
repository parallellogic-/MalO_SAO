/*
  RP2350B Analog Recorder - Pin 42
  Directions:
  1. Connect your audio source to Pin 41 (ensure 0-3.3V range, bias if necessary).
  2. Upload this sketch.
  3. Open Serial Monitor at 115200 baud.
  4. Type 'r' in the input box and hit Enter to start recording.
  5. The data will stream in the monitor.
  6. Copy all numbers (one per line).
  7. Paste into a text file and convert to .raw/.wav via Audacity "Import Raw Data".
  this and the following command can hear blowing on the mic:
  awk '{printf "%02x", $1}' ~/Desktop/005.txt | xxd -r -p > ~/Desktop/006.bin
  high frequency squeal due to uneven sampling or digitization or coupling?
*/

#define RECORDING_PIN 42
#define SAMPLE_RATE 8000 // 8kHz
#define BUFFER_SIZE 80000 // 1 second of audio
#define INTERVAL_US (1000000 / SAMPLE_RATE)

uint16_t buffer[BUFFER_SIZE];
bool isRecording = false;
uint32_t mean=0;

void setup() {
  Serial.begin(115200);
  pinMode(RECORDING_PIN, INPUT);
  while(!Serial); // Wait for USB
  Serial.println("Send 'r' to start recording...");
  analogReadResolution(12); 
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'r' && !isRecording) {
      recordAudio();
    }
  }
}

void recordAudio() {
  Serial.println("RECORDING START");
  
  uint32_t nextSample = micros();
  for (int i = 0; i < BUFFER_SIZE; i++) {
    while (micros() < nextSample);
    
    // RP2350 provides 12-bit analog reading (0-4095)
    buffer[i] = analogRead(RECORDING_PIN);
    mean+=buffer[i];
    
    nextSample += INTERVAL_US;
  }
  
  Serial.println("RECORDING DONE. Dumping data:");
  
  // Dump data to serial in a format for parsing
  mean/=BUFFER_SIZE;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    uint16_t value=buffer[i]-mean+0x80;
    uint8_t value2=0x00FF & value;
    Serial.println(value2);
  }
  //Serial.println("DATA END");
}