/*
 * Setup GPIO24 as a PWM output at 38 kHz, 50% duty cycle
 * Setup GPIO43 as digital input
 * In the loop: test different lengths of GPIO24 (ex. 100x cycles, 20x cycles, 5x cycles), 
 * observe the length (if any) that GPIO43 is high as a result 
 * (i.e. measure GPIO43 while GPIO24 is pulsing)
 * Run these tests 3 times a second and print to the terminal
 * Do not use pwm_set_freq_duty, do not use RP2040_PWM library
 result: resdiaul is a fraction of a 38kHz pulse width, so duration of ir received from self is on par with what was sent
 */

const int PWM_PIN = 24;
const int INPUT_PIN = 43;
const uint32_t TARGET_FREQ = 38000; // 38 kHz
const int DUTY_50_PERCENT = 127;    // 50% of 255 (8-bit range)
const int led_pin=38;

// Array of cycle counts to test
const int testCycles[] = {10000,100, 20, 5,4,3,2,1};
const int numTests = 8;

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for terminal

  pinMode(INPUT_PIN, INPUT);
  pinMode(led_pin, OUTPUT);
  
  // Native Philhower core functions to set hardware PWM
  analogWriteFreq(TARGET_FREQ);
  analogWriteRange(255);
  
  // Ensure PWM starts LOW
  analogWrite(PWM_PIN, 0);
  
  Serial.println("RP2350B IR TX/RX Experiment Started");
}

void loop() {
  for (int i = 0; i < numTests; i++) {
    int cyclesToRun = testCycles[i];
    
    // Calculate approximate duration in microseconds
    // 1 cycle at 38kHz is approx 26.3us
    uint32_t pulseDurationUs = (1000000 / TARGET_FREQ) * cyclesToRun;
    uint32_t settleUs = (1000000 / TARGET_FREQ) * 30;

    uint32_t highCount = 0;
    uint32_t totalSamples = 0;

    // Start PWM
    analogWrite(PWM_PIN, DUTY_50_PERCENT);
    
    uint32_t startTime = micros();
    // Sample the input while the PWM is active
    while (micros() - startTime < pulseDurationUs) {
      if (!digitalRead(INPUT_PIN)) {
        highCount++;
      }
      totalSamples++;
      digitalWrite(led_pin,!digitalRead(INPUT_PIN)); //echo IR state to indicator LED
    }
    
    // Stop PWM
    analogWrite(PWM_PIN, 0);

    //also look for settlign time on back-end
    while (micros() - startTime < (pulseDurationUs + settleUs) ) {
      if (!digitalRead(INPUT_PIN)) {
        highCount++;
      }
      //totalSamples++;
      digitalWrite(led_pin,!digitalRead(INPUT_PIN)); //echo IR state to indicator LED
    }

    // Print results
    Serial.print("Test "); 
    Serial.print(cyclesToRun);
    Serial.print(" cycles: GPIO43 was LOW for ");
    Serial.print(highCount);
    Serial.print("/");
    Serial.print(totalSamples);
    Serial.print(" samples.  Residual: ");
    Serial.print(totalSamples-highCount);
    Serial.println();

    delay(100); // Small gap between different cycle tests
  }

  Serial.println("---");
  delay(333); // Run the full set of tests ~3 times per second
}
