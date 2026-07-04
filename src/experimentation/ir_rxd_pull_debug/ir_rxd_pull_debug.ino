#include <Adafruit_TinyUSB.h>

void setup() {
  // put your setup code here, to run once:
  Serial.begin(1'000'000);
  pinMode(43,INPUT_PULLUP);
}

bool was=false;

void loop() {
  // put your main code here, to run repeatedly:
  bool is=digitalRead(43);
  if(is!=was)
  {
    Serial.println(time_us_64());
    was=is;
  }
}
