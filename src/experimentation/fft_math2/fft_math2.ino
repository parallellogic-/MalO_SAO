
#include <arm_math.h>

#define FFT_SIZE 512//0.3 ms
//#define FFT_SIZE 1024//0.7 ms
//#define FFT_SIZE 2048//1.4 ms

// CMSIS-DSP Radix-4 requires an instance structure
arm_cfft_radix4_instance_q15 fftInstance;
q15_t fftBuffer[FFT_SIZE * 2];
q15_t magnitudes[FFT_SIZE / 2];

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  delay(1000);

  while(1)
    {
    Serial.println("\n--- RP2350 Legacy Radix-4 Q15 FFT ---");

    // 1. Initialize the Radix-4 Structure
    // Parameters: (instance, fftLen, ifftFlag=0, bitReverseFlag=1)
    arm_cfft_radix4_init_q15(&fftInstance, FFT_SIZE, 0, 1);

    // 2. Mock data generation 
    uint8_t raw8BitSamples[FFT_SIZE];
    for (uint16_t i = 0; i < FFT_SIZE; i++) {
      float32_t rads = (2.0f * PI * 5.0f * i) / FFT_SIZE; 
      raw8BitSamples[i] = (uint8_t)(127.0f + 120.0f * sinf(rads));
    }

    // 3. Convert 8-bit unsigned to 16-bit signed Q15
    for (uint16_t i = 0; i < FFT_SIZE; i++) {
      int16_t centered = (int16_t)raw8BitSamples[i] - 128; 
      fftBuffer[i * 2] = centered << 8;                    
      fftBuffer[(i * 2) + 1] = 0;                          
    }

    // 4. Measure Radix-4 Execution Time
    uint32_t startTime = micros();
    
    // Execute using the Radix-4 function variant
    arm_cfft_radix4_q15(&fftInstance, fftBuffer);
    
    uint32_t endTime = micros();
    uint32_t executionTime = endTime - startTime;

    // 5. Compute Magnitudes 
    arm_cmplx_mag_q15(fftBuffer, magnitudes, FFT_SIZE / 2);

    // 6. Print Time Result
    Serial.print("Fixed-Point Radix-4 Execution Time: ");
    Serial.print(executionTime);
    Serial.println(" microseconds");

    for(int iter = 0; iter < FFT_SIZE / 2; iter++)
    {
      // Compute frequency for the current bin 
      // (Assuming a nominal sample rate, e.g., 16384 Hz as an example)
      float freq = (float)iter * 1.0*60;// / FFT_SIZE; 
      
      Serial.print("Bin ");
      Serial.print(iter);
      Serial.print(", (");
      Serial.print(freq, 2);
      Serial.print(" Hz), ");
      Serial.println(magnitudes[iter]);
    }

    delay(40000);
    }
}

void loop() {}
