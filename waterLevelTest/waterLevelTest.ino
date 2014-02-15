void setup(){
  Serial.begin(9600);
}

void loop(){
  analogWrite(A0,1023);
  int sensorValue=analogRead(A5);
  Serial.println(sensorValue);
  delay(1000);
}
