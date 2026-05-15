//drive pin high, let it leak to ground through 1 Mohm resistor +/-1%, measure decay time to pass schmitt trigger - if finger is present, the delay time gets drawn out.  Online estiamted
//10% change in capacitance, but seeing closer to 100% in this setup, making reading higher reliability
//couldn't get pio to work for this (presuambly a logic bug somewhere), so manual approach shows the concept/approach has merit if needed
//but would prefer drive ground like cap touch on face (ex. get 2 readings: one on rising and one on falling, to make measurement more symmetric/stable (?))

//const int pin=36; //28-36
void setup() {
  // put your setup code here, to run once:
  Serial.begin(1'000'000);
}

void loop() {
  // put your main code here, to run repeatedly:
  long start_tms=millis();
  for(int pin=28;pin<=36;pin++)
  {
    int run_sum=0;
    for(int iter=0;iter<100;iter++)
    {
      pinMode(pin,OUTPUT);
      digitalWrite(pin,HIGH);
      int counter=0;
      pinMode(pin,INPUT);
      while(true)
      {
        counter++;
        if(!digitalRead(pin))
        {
          run_sum+=counter;
          break;
        }
      }
    }
    run_sum/=100;
    switch(pin)
    {
      case 28: run_sum-=79; break; 
      case 29: run_sum-=88; break; 
      case 30: run_sum-=99; break; 
      case 31: run_sum-=78; break; 
      case 32: run_sum-=76; break; 
      case 33: run_sum-=98; break; 
      case 34: run_sum-=84; break; 
      case 35: run_sum-=94; break; 
      case 36: run_sum-=95; break; 
    }
    run_sum+=20;
    Serial.print(pin);
    Serial.print(", ");
    Serial.println(run_sum);
  }
  Serial.println(millis()-start_tms);
  delay(200);
}
