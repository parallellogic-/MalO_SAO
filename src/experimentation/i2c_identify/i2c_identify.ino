#include <PDM.h>

// Configuration based on your hardware pins
#define PDM_DATA_PIN 14
#define PDM_CLK_PIN  15

// Audio configuration
#define SAMPLE_RATE  16000  // 16 kHz standard for PDM voice capture
#define CHANNELS     1      // Mono channel operation

// Buffer to store 16-bit PCM audio samples
short sampleBuffer[512];
volatile int samplesRead = 0;

// Timing tracking for 100ms interval reports
unsigned long lastReportTime = 0;
double squaredSum = 0;
long totalSamplesCount = 0;

// Callback function triggered when PDM data is ready
void onPDMdata() {
  int bytesAvailable = PDM.available();
  
  // Read available bytes into our 16-bit PCM sample buffer
  int samplesCount = bytesAvailable / 2; 
  PDM.read(sampleBuffer, bytesAvailable);
  
  // Accumulate squared values for RMS calculation
  for (int i = 0; i < samplesCount; i++) {
    double sample = sampleBuffer[i];
    squaredSum += sample * sample;
  }
  totalSamplesCount += samplesCount;
}

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for Serial monitor to open

  Serial.println("Initializing PDM Microphone...");

  // Assign the hardware pins to the PDM driver
  PDM.setDATA(PDM_DATA_PIN);
  PDM.setCK(PDM_CLK_PIN);

  // Initialize PDM library with mono channel at 16kHz
  if (!PDM.begin(CHANNELS, SAMPLE_RATE)) {
    Serial.println("Failed to initialize PDM microphone!");
    while (1);
  }

  // Register the interrupt callback helper
  PDM.onReceive(onPDMdata);
  
  lastReportTime = millis();
}

void loop() {
  // Check if 100 milliseconds have elapsed
  if (millis() - lastReportTime >= 100) {
    // Disable interrupts briefly to safely copy and reset data variables
    noInterrupts();
    double currentSquaredSum = squaredSum;
    long currentTotalSamples = totalSamplesCount;
    squaredSum = 0;
    totalSamplesCount = 0;
    interrupts();

    // Calculate Root Mean Square (RMS) if samples were captured
    if (currentTotalSamples > 0) {
      double meanSquare = currentSquaredSum / currentTotalSamples;
      double rms = sqrt(meanSquare);

      // Print the RMS audio amplitude to the Serial Terminal
      Serial.print("RMS Audio Level: ");
      Serial.println(rms, 2);
    }

    lastReportTime = millis();
  }
}