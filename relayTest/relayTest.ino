#define relay1 53
#define relay2 51
#define relay3 49
#define relay4 47
#define relay5 45
#define relay6 43
#define relay7 41
#define relay8 39

void setup(){
 pinMode(relay1,OUTPUT);
 pinMode(relay2,OUTPUT);
 pinMode(relay3,OUTPUT);
 pinMode(relay4,OUTPUT);
 pinMode(relay5,OUTPUT);
 pinMode(relay6,OUTPUT);
 pinMode(relay7,OUTPUT);
 pinMode(relay8,OUTPUT);
   digitalWrite(relay1,HIGH);
  delay(10000);
  digitalWrite(relay1,LOW);
  delay(10000);
}
void loop(){

  
    digitalWrite(relay1,HIGH);
  delay(10000);
  digitalWrite(relay1,LOW);
  delay(20000);
}
