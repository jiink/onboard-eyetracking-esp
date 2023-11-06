#include <Arduino.h>

//int flashPin = 4;
int t = 0;

void setup() {
  //pinMode(flashPin, OUTPUT);
  Serial.begin(115200);
}

void loop() {
  t = (t + 1) % 999;
  char message[100];
  sprintf(message, "hello: %d", t);
  Serial.println(message);
  //digitalWrite(flashPin, HIGH);
  //delay(1);
  //digitalWrite(flashPin, LOW);
  delay(500);
}