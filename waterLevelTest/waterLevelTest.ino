int vouts[]={32,34,36};

void setup(){
  Serial.begin(9600);
  pinMode(30,INPUT);
  for(int i=0;i<3;i++){
    pinMode(vouts[i],OUTPUT);
  }
}

void loop(){
  int sum=0;
  for(int i=0;i<3;i++){
    digitalWrite(vouts[i],HIGH);
    sum+=digitalRead(30);
    digitalWrite(vouts[i],LOW);
  }
  Serial.println(sum);
  delay(1000);
}
