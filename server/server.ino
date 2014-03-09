//Bar bot server
//All rights reserved

#include <Ethernet.h>
#include <SPI.h>
#include <TimerThree.h>


#define NUMBER_PUMPS 8
#define CLOCK_SPEED 16000000
#define FLOW_METER_TIMEOUT_MS 1000
#define ML_PER_SEC 33
//number of ML to try to pump, then give up
#define ML_TIMEOUT 30
#define PORT 1234


//Pin defines
#define D_PIN_PIPE 22
#define D_PIN_LEVEL_CRITICAL 23
#define D_PIN_LEVEL_LOW 24
#define D_PIN_LEVEL_MID 25

#define D_PIN_PUMP_1  49
#define D_PIN_PUMP_2  47
#define D_PIN_PUMP_3  45
#define D_PIN_PUMP_4  43
#define D_PIN_PUMP_5  41
#define D_PIN_PUMP_6  39
#define D_PIN_PUMP_7  37
#define D_PIN_PUMP_8  35

#define A_PIN_PUMP_LEVEL_1 48
#define A_PIN_PUMP_LEVEL_2 46
#define A_PIN_PUMP_LEVEL_3 44
#define A_PIN_PUMP_LEVEL_4 42
#define A_PIN_PUMP_LEVEL_5 40
#define A_PIN_PUMP_LEVEL_6 38
#define A_PIN_PUMP_LEVEL_7 36
#define A_PIN_PUMP_LEVEL_8 34

#define MAX_DRINKS 10
//Arrays for convenience
const int Pumps[NUMBER_PUMPS]={D_PIN_PUMP_1,D_PIN_PUMP_2,D_PIN_PUMP_3,D_PIN_PUMP_4,
    D_PIN_PUMP_5,D_PIN_PUMP_6,D_PIN_PUMP_7,D_PIN_PUMP_8};
const int PumpSensor[NUMBER_PUMPS]={A_PIN_PUMP_LEVEL_1,A_PIN_PUMP_LEVEL_2,
    A_PIN_PUMP_LEVEL_3,A_PIN_PUMP_LEVEL_4,A_PIN_PUMP_LEVEL_5,
    A_PIN_PUMP_LEVEL_6,A_PIN_PUMP_LEVEL_7,A_PIN_PUMP_LEVEL_8};
    
//Ethernet globals
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetServer server(PORT);  

//typedef's
typedef struct Drink{//12*2=28 bytes
  short volumes[NUMBER_PUMPS];
};

//globals
Drink drinkList[ MAX_DRINKS ];
volatile byte drinkQueueSize=0;//volitile becase the interupt might change the value as you are making a new drink.
//Also a single byte because the compiler makes byte operations atomic,
//required to prevent races with the interupts.
byte fluidLevels[NUMBER_PUMPS];//the last know fluid levels
byte fluidInPipes[NUMBER_PUMPS];//last Checked fluids at pump output
short fluidTimeouts[NUMBER_PUMPS];
unsigned long lastFluidCheck=0;

void setup(){
  //Pin setups:
  for(int i=0;i<NUMBER_PUMPS;i++){
    pinMode(Pumps[i],OUTPUT);
    pinMode(PumpSensor[i],INPUT);
    fluidTimeouts[i]=0;
  }
  pinMode(D_PIN_PIPE,OUTPUT);
  
  
  Serial.begin(9600);
  //Setup timer interrupt
  long desiredFrequency=ML_PER_SEC;//Set frequency to be ml/sec so
    //the interrupt calls has a 1:1 ratio of calls to ml dispensed
  int microSeconds=(1000000/desiredFrequency);
  Timer3.initialize(microSeconds);
  Timer3.attachInterrupt(timedInterupt,microSeconds);
  //End timed interrupt setup

  //Setup for Ethernet Card
  Ethernet.begin(mac);  // initialize Ethernet device
  server.begin();           // start to listen for clients
  //End Ethernet Setup
}

void loop(){
  //Serial.println("Entering loop");
  EthernetClient client = server.available();  // try to get client
  if(client){
    Serial.println("client gotten");
    if(waitForAvaliableBytes(client,1,500)==-1){
      client.stop();return;
    }
    byte requestType=client.read();
    if(requestType==0){ //return status of the device
      getStatus(client);
    }else if(requestType==1||requestType==2){//Queue a drink
      makeDrink(client);
    }else if(requestType==3){//check status of drink
      
    } 
  }
  if(millis()-lastFluidCheck>10000){//update the fluid levels ever 10 seconds
    updateFluidLevels();
    lastFluidCheck=millis();
  }
}

//timed interrupt for drink dispensing
void timedInterupt(){
  if(drinkQueueSize>0){//TODO also add a check or delay of some sort
    updateFluidInPipes();
    int numOn=0;
    for(int i=0;i<NUMBER_PUMPS;i++){
      if(drinkList[0].volumes[i]>0){
        if(fluidInPipes[i]==HIGH){//if the sensor says there is fluid in the hose
          drinkList[0].volumes[i]--;//decrement the volume
        }//if this evaluates as false, fluid is not exiting the hose, not getting to the drink
        else{
          fluidTimeouts[i]++;//increment the timeout so you know 
          if(fluidTimeouts[i]>ML_TIMEOUT){//if the timeout is large enough
            drinkList[0].volumes[i]=0;//give up on the fluid
          }
        }
        if(drinkList[0].volumes[i]>0){//if there is volume left
          digitalWrite(Pumps[i],HIGH);//turn on the pump
          numOn++;
        }else{//if there isn't any volume left
          digitalWrite(Pumps[i],LOW);//make sure the pump is off
        }
      }else{
        digitalWrite(Pumps[i],LOW);//Really make sure its off
      }
    }//end for each fluid
    if(numOn==0){//if there are no pumps on
      popDrinkQueue();//pop the drink because its finished
      //TODO any other code required here
    }
  }//end if there are drinks
}

void popDrinkQueue(){
  drinkQueueSize--;
  for(int i=0;i<MAX_DRINKS-1;i++){
    drinkList[i]=drinkList[i++]; 
  }
  for(int i=0;i<NUMBER_PUMPS;i++){
    drinkList[MAX_DRINKS-1].volumes[i]=0;
    fluidTimeouts[i]=0;
  }
}

void makeDrink(EthernetClient client){
  if(drinkQueueSize>=MAX_DRINKS){//currently has the max number of drinks queued
    client.flush();
    client.write((byte)1); delay(1); client.stop(); return;
  }
  //make sure the drink you are about to edit is empty:
  for(int i=0;i<NUMBER_PUMPS;i++){
    drinkList[drinkQueueSize].volumes[i]=0;
  }
  if(waitForAvaliableBytes(client,1,500)==-1){
    client.write((byte)2);delay(1);client.stop();return;
  }
  byte numberIngerdients=client.read();
  if(waitForAvaliableBytes(client,numberIngerdients*2,500)==-1){
    client.write((byte)2);delay(1);client.stop();return;
  }
  for(int i=0;i<numberIngerdients;i++){
    int pump=client.read();
    if(pump>=NUMBER_PUMPS){//invalid pump number
      client.write((byte)5);delay(1);client.stop();return;
    }
    int volume=client.read();
    drinkList[drinkQueueSize].volumes[pump]+=volume;
  }
  if(client.read()>=0){//client should have ended output, return error
    client.write((byte)2);delay(1);client.stop();return;
  }
  drinkQueueSize++;
  client.write((byte)0);delay(1);client.stop();return;
}

void getStatus(EthernetClient client){
  if(millis()<lastFluidCheck||lastFluidCheck==0){
    updateFluidLevels();
    lastFluidCheck=millis();
  }
  client.write((byte)0);
  client.write((byte)NUMBER_PUMPS);
  client.write(fluidLevels,NUMBER_PUMPS);
  delay(1);
  client.stop();
}

/**
Returns negitive one if timeout reached, otherwise it will
reutrn 1
*/
int waitForAvaliableBytes(EthernetClient client, int bytesToWaitFor, int timeoutms){
  unsigned long time =millis();
  while(client.available()<bytesToWaitFor){
    if(millis()-time>=timeoutms){
      return -1;
    }
  }
  return 1;
}


void updateFluidLevels(){
    digitalWrite(D_PIN_LEVEL_CRITICAL,HIGH);
    for(int i=0;i<NUMBER_PUMPS;i++){
      if(digitalRead(PumpSensor[i])){
        fluidLevels[i]=1;
      }else{
        fluidLevels[i]=0;
      }
    }
    digitalWrite(D_PIN_LEVEL_CRITICAL,LOW);
    
    digitalWrite(D_PIN_LEVEL_LOW,HIGH);
    for(int i=0;i<NUMBER_PUMPS;i++){
      if(digitalRead(PumpSensor[i])){
        fluidLevels[i]=2;
      }
    }
    digitalWrite(D_PIN_LEVEL_LOW,LOW);
    
    digitalWrite(D_PIN_LEVEL_MID,HIGH);
    for(int i=0;i<NUMBER_PUMPS;i++){
      if(digitalRead(PumpSensor[i])){
        fluidLevels[i]=3;
      }
    }
    digitalWrite(D_PIN_LEVEL_MID,LOW);
}

void updateFluidInPipes(){
  //TODO check any other sensor lines that might be on,
  //save then, and then restore them at the end
  int on=0;
  if(digitalRead(D_PIN_LEVEL_CRITICAL)==HIGH){
    on=1;
    digitalWrite(D_PIN_LEVEL_CRITICAL,LOW);
  }
  if(digitalRead(D_PIN_LEVEL_LOW)==HIGH){
    on=2;
    digitalWrite(D_PIN_LEVEL_LOWL,LOW);
  }
  if(digitalRead(D_PIN_LEVEL_MID)==HIGH){
    on=3;
    digitalWrite(D_PIN_LEVEL_MID,LOW);
  }
  
  digitalWrite(D_PIN_PIPE,HIGH);
  for(int i=0;i<NUMBER_PUMPS;i++){
    fluidInPipes[i]=digitalRead(PumpSensor[i]);
  }
  digitalWrite(D_PIN_PIPE,LOW);
  
  //restore the previously on sensor pin.  would be easier 
  //just to do this in the main loop every 30 ms or so.
  if(on==1)
    digitalWrite(D_PIN_LEVEL_CRITICAL,HIGH);
  if(on==2)
    digitalWrite(D_PIN_LEVEL_LOW,HIGH);
  if(on==3)
    digitalWrite(D_PIN_LEVEL_MID,HIGH);
}


