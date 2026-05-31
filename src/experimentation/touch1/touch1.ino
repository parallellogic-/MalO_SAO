/*
  Blink

  Turns an LED on for one second, then off for one second, repeatedly.

  Most Arduinos have an on-board LED you can control. On the UNO, MEGA and ZERO
  it is attached to digital pin 13, on MKR1000 on pin 6. LED_BUILTIN is set to
  the correct LED pin independent of which board is used.
  If you want to know what pin the on-board LED is connected to on your Arduino
  model, check the Technical Specs of your board at:
  https://docs.arduino.cc/hardware/

  modified 8 May 2014
  by Scott Fitzgerald
  modified 2 Sep 2016
  by Arturo Guadalupi
  modified 8 Sep 2016
  by Colby Newman

  This example code is in the public domain.

  https://docs.arduino.cc/built-in-examples/basics/Blink/
*/

#define PIN_DEBUG_R 37
#define PIN_DEBUG_G 38

#define PIN_PWM 26
#define PIN_CT0 27
#define PIN_CT9 36

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  Serial.begin();
  pinMode(PIN_DEBUG_R, OUTPUT);
  pinMode(PIN_DEBUG_R, OUTPUT);
  pinMode(PIN_PWM, OUTPUT);
  for(int iter=PIN_CT0;iter<=PIN_CT9;iter++) pinMode(iter, INPUT);
}

// the loop function runs over and over again forever
void loop() {
  digitalWrite(PIN_DEBUG_R, HIGH);  // change state of the LED by setting the pin to the HIGH voltage level
  //digitalWrite(PIN_PWM, HIGH);  // change state of the LED by setting the pin to the HIGH voltage level
  pinMode(PIN_PWM, INPUT_PULLUP);  // change state of the LED by setting the pin to the HIGH voltage level
  Serial.print("H: ");
  for(int iter=PIN_CT0;iter<=PIN_CT9;iter++) Serial.print(digitalRead(iter));
  Serial.println();
  delay(300);                      // wait for a second
  Serial.print("H: "); 
  for(int iter=PIN_CT0;iter<=PIN_CT9;iter++) Serial.print(digitalRead(iter));
  Serial.println();
  digitalWrite(PIN_DEBUG_R, LOW);   // change state of the LED by setting the pin to the LOW voltage level
  pinMode(PIN_PWM, INPUT_PULLDOWN);  // change state of the LED by setting the pin to the HIGH voltage level
  Serial.print("L: ");
  for(int iter=PIN_CT0;iter<=PIN_CT9;iter++) Serial.print(digitalRead(iter));
  Serial.println();
  delay(300);                      // wait for a second
  Serial.print("L: ");
  for(int iter=PIN_CT0;iter<=PIN_CT9;iter++) Serial.print(digitalRead(iter));
  Serial.println();
}
