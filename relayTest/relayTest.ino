#include <TimerThree.h>

#define relay1 53
#define relay2 51
#define relay3 49
#define relay4 47
#define relay5 45
#define relay6 43
#define relay7 41
#define relay8 39

#define sensor1 22
#define vout1 23

#define ml_per_sec 33
#define ml_offset 7



void setup(){
  pinMode(relay1,OUTPUT);
  pinMode(relay2,OUTPUT);
  pinMode(relay3,OUTPUT);
  pinMode(relay4,OUTPUT);
  pinMode(relay5,OUTPUT);
  pinMode(relay6,OUTPUT);
  pinMode(relay7,OUTPUT);
  pinMode(relay8,OUTPUT);
  pinMode(relay8,OUTPUT);
  pinMode(relay8,OUTPUT);
  pinMode(sensor1,INPUT);
  pinMode(vout1,OUTPUT);
  long desiredFrequency=ml_per_sec;//Set frequency to be ml/sec so
    //the interrupt calls has a 1:1 ratio of calls to ml dispensed
  int microSeconds=(1000000/desiredFrequency);
  Timer3.initialize(microSeconds);
  Timer3.attachInterrupt(timedInterupt,microSeconds);
  
}
int mL_target=10;
int mL_poured=0;
int delayCounter=0;
int iterationCount=0;
void loop(){
  
}


void timedInterupt(){
  if(delayCounter>0){
    delayCounter--;
    return;
  }
  if(mL_poured<mL_target){
    digitalWrite(vout1,HIGH);
    if(digitalRead(sensor1)==HIGH){//if the sensor says there is fluid in the hose
      mL_poured++;
    }
    digitalWrite(vout1,LOW);
    if(mL_poured<mL_target){//if there is volume left
      digitalWrite(relay1,HIGH);//turn on the pump
    }else{//if there isn't any volume left
      digitalWrite(relay1,LOW);//make sure the pump is off
    }
  }else{
    digitalWrite(relay1,LOW);//Really make sure its off
    mL_poured=0;
    if(iterationCount++>=3){
      iterationCount=0;
      mL_target+=5;
    }
    delayCounter=10*ml_per_sec;//give a ten second pause after each measurement
  }
}
