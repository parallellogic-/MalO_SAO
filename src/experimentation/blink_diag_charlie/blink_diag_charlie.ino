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


// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  Serial.begin();
}

// the loop function runs over and over again forever
void loop() {
  int pin[8+3+1][2]={
    /*{1,6},
    {0,3},
    {0,5},//5 works low
    {0,6},
    {0,4},
    {0,1},*/
    {0,7},/*
    {5,6},
    {4,3},
    {7,3},//7 works high
    {7,5},//broken LED?
    {5,7}*/
  };
  for(int iter=0;iter<8+3+1;iter++)
  {
    pinMode(pin[iter][0],OUTPUT);
    pinMode(pin[iter][1],OUTPUT);
    digitalWrite(pin[iter][0],HIGH);
    digitalWrite(pin[iter][1],LOW);
    delay(1);
    pinMode(pin[iter][0],INPUT);
    pinMode(pin[iter][1],INPUT);
  }
}
