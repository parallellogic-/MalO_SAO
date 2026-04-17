/*
  Play All Middle A Tones (A4 = 440Hz)
  Plays A0 through A7 (30Hz - 3520Hz) with 50% duty cycle.
*/

const int buzzerPin = 25; // Connect positive pin of buzzer to D9

// Frequencies for Middle A (A4) in different octaves
// A0 = 27.5Hz, A1 = 55Hz, A2 = 110Hz, A3 = 220Hz,
// A4 = 440Hz, A5 = 880Hz, A6 = 1760Hz, A7 = 3520Hz
const int aNotes[] = {0, 55, 110, 220, 440, 880, 1760, 3520};
const int numNotes = 8;
const int noteDuration = 1000; // 1 second per note
const int pauseDuration = 100; // 100ms pause between notes

void setup() {
  pinMode(buzzerPin, OUTPUT);
}

void loop() {
  for (int i = 0; i < numNotes; i++) {
    // tone(pin, frequency) produces a 50% duty cycle square wave
    tone(buzzerPin, aNotes[i]);
    delay(noteDuration);
    
    // Stop tone to create separation
    noTone(buzzerPin);
    delay(pauseDuration);
  }
}