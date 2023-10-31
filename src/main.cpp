#include <Arduino.h>

int flashPin = 4;

void setup() {
  pinMode(flashPin, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  Serial.println("Hello World!");
  digitalWrite(flashPin, HIGH);
  delay(16);
  digitalWrite(flashPin, LOW);
  delay(1000);
}
