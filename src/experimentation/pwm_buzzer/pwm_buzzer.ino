//give me arduino sketch for pwm code (50% duty cycle cosntantly) to step through 100 hz to 1000 hz in 1 hz steps, then 1khz to 4 khz in 500 khz steps, one step per 250 ms for rp2350b

// Target Pin (Adjust as needed for your RP2350 board)
const int pwmPin = 25; 

void setup() {
  pinMode(pwmPin, OUTPUT);
  Serial.begin(115200);
}

void loop() {
  // Phase 1: 100 Hz to 1000 Hz in 1 Hz steps
  Serial.println("Starting Phase 1: 100Hz - 1000Hz (1Hz steps)");
  for (int freq = 100; freq < 1000; freq += 100) {
    playFrequency(freq);
  }

  // Phase 2: 1000 Hz to 4000 Hz (4 kHz) in 500 Hz steps
  // Note: Your request mentioned "500 khz steps" which likely meant 500 Hz 
  // given the 4 kHz ceiling.
  Serial.println("Starting Phase 2: 1000Hz - 4000Hz (500Hz steps)");
  for (int freq = 1000; freq <= 4000; freq += 500) {
    playFrequency(freq);
  }

  // Stop after completing the sequence or remove noTone() to loop forever
  noTone(pwmPin);
  delay(250);
  Serial.println("Sequence Complete.");
  //while(1); 
}

void playFrequency(int frequency) {
  // tone() automatically generates a 50% duty cycle square wave
  tone(pwmPin, frequency); 
  
  Serial.print("Current Frequency: ");
  Serial.print(frequency);
  Serial.println(" Hz");
  
  delay(250); // One step per 250ms
}