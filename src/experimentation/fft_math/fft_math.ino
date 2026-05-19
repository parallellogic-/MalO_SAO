//8 ms
#include <Arduino.h>
#include <arm_math.h>

#define FFT_SIZE 512

// Buffers for Real values (In) and Complex magnitude squared (Out)
float32_t inputData[FFT_SIZE * 2]; // Needs to be 2x size for complex parts
float32_t outputData[FFT_SIZE];
arm_rfft_fast_instance_f32 fftInstance;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize the RFFT Fast instance for 512 points
  // ifftFlag = 0 means forward transform
  arm_rfft_fast_init_f32(&fftInstance, FFT_SIZE);
}

void loop() {
  // 1. Populate the 512-element array with a mock signal (e.g., sine wave + noise)
  for (int i = 0; i < FFT_SIZE; i++) {
    // 50Hz and 120Hz combined signal
    inputData[i] = 0.7 * sin(2 * PI * 50 * i / FFT_SIZE) + 0.3 * sin(2 * PI * 120 * i / FFT_SIZE);
  }

  // 2. Start timer
  uint32_t startTime = micros();

  // 3. Compute the FFT
  // For RFFT, the output array is of size 512 (contains both real/imag pairs in alternating order)
  arm_rfft_fast_f32(&fftInstance, inputData, outputData, 0);

  // 4. Stop timer
  uint32_t elapsedTime = micros() - startTime;

  // 5. Compute Magnitude (Optional step, required for spectrum analysis)
  float32_t magnitude[FFT_SIZE / 2];
  arm_cmplx_mag_f32(outputData, magnitude, FFT_SIZE / 2);

  // 6. Report the result
  Serial.print("FFT Computation took: ");
  Serial.print(elapsedTime);
  Serial.println(" microseconds.");

  delay(2000); 
}
