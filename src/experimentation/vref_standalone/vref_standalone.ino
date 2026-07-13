
#include <Adafruit_TinyUSB.h>

void setup() {
  Serial.begin(115200); // Recommended to initialize serial communication
  analogReadResolution(12); 
  for(int iter=40;iter<=48;iter++) pinMode(iter, INPUT);   // Correct pin mode constant
}

void loop() {
  for(int iter=40;iter<=48;iter++) Serial.printf("%2d: %4d, ",iter,analogRead(iter)); 
  Serial.println();
  delay(100);
}

//vref ideal is 1.24V
//measure 1530 counts --> 1.23V, working normally