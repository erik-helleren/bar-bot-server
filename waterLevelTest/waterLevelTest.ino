void setup(){
  Serial.begin(9600);
  pinMode(30,OUTPUT);
}

void loop(){
  digitalWrite(30,HIGH);
  delay(1);
  int sensorValue=analogRead(A5);
  digitalWrite(30,LOW);
  Serial.println(sensorValue);
  delay(1000);
}
