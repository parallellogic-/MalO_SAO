#include "hardware/adc.h"

void setup() {
  // put your setup code here, to run once:
  Serial.begin();
  while(!Serial);
  Serial.println("START");
  adc_init(); 
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("loop 1");
  adc_fifo_drain();
  Serial.println("loop 2");
  delay(300);
}
