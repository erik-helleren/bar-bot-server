//Bar bot server
//All rights reserved

#include <Ethernet.h>
#include <SPI.h>
#include <TimerThree.h>


#define NUMBER_PUMPS 12
#define CLOCK_SPEED 16000000
#define FLOW_METER_TIMEOUT_MS 1000
#define ML_PER_MIN 1000


//Pin defines
#define PIN_FLOW_METER 21
#define D_PIN_PUMP_1  22
#define D_PIN_PUMP_2  23
#define D_PIN_PUMP_3  24
#define D_PIN_PUMP_4  25
#define D_PIN_PUMP_5  26
#define D_PIN_PUMP_6  27
#define D_PIN_PUMP_7  28
#define D_PIN_PUMP_8  29
#define D_PIN_PUMP_9  30
#define D_PIN_PUMP_10 31
#define D_PIN_PUMP_11 32
#define D_PIN_PUMP_12 33

#define A_PIN_PUMP_LEVEL_1 1
#define A_PIN_PUMP_LEVEL_2 2
#define A_PIN_PUMP_LEVEL_3 3
#define A_PIN_PUMP_LEVEL_4 4
#define A_PIN_PUMP_LEVEL_5 5
#define A_PIN_PUMP_LEVEL_6 6
#define A_PIN_PUMP_LEVEL_7 7
#define A_PIN_PUMP_LEVEL_8 8
#define A_PIN_PUMP_LEVEL_9 9
#define A_PIN_PUMP_LEVEL_10 10
#define A_PIN_PUMP_LEVEL_11 11
#define A_PIN_PUMP_LEVEL_12 12

#define MAX_DRINKS 10
//Arrays for convenience
const int Pumps[NUMBER_PUMPS]={D_PIN_PUMP_1,D_PIN_PUMP_2,D_PIN_PUMP_3,D_PIN_PUMP_4,
    D_PIN_PUMP_5,D_PIN_PUMP_6,D_PIN_PUMP_7,D_PIN_PUMP_8,D_PIN_PUMP_9,
    D_PIN_PUMP_10,D_PIN_PUMP_11,D_PIN_PUMP_12};
const int FluidMeters[NUMBER_PUMPS]={A_PIN_PUMP_LEVEL_1,A_PIN_PUMP_LEVEL_2,A_PIN_PUMP_LEVEL_3,A_PIN_PUMP_LEVEL_4,
    A_PIN_PUMP_LEVEL_5,A_PIN_PUMP_LEVEL_6,A_PIN_PUMP_LEVEL_7,A_PIN_PUMP_LEVEL_8,A_PIN_PUMP_LEVEL_9,
    A_PIN_PUMP_LEVEL_10,A_PIN_PUMP_LEVEL_11,A_PIN_PUMP_LEVEL_12};
    
//Ethernet globals
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,2, 20); // IP address, may need to change depending on network
EthernetServer server(1234);  // create a server at port 80

//typedef's
typedef struct Drink{//12*2+4=28 bytes
  short volumes[NUMBER_PUMPS];
};

//globals
Drink drinkList[ MAX_DRINKS ];
byte volumeIndex=0;//Used in interupt to check what its currently pouring
int drinkQueueSize=0;//The size of the drink queue.  Disable interupts before changing outside of interupt
long pourTime=0;//Used in interupt to count how long it has been pouring
int flowMeterCount=0;
unsigned long lastFlowMeterTime;//last time the flow metter was accessed.
byte fluidLevels[NUMBER_PUMPS];//the last know fluid levels

//interupt for the flow meter
void flowInterupt(){
  flowMeterCount++;
  lastFlowMeterTime=millis();
}

void setup(){
  //Pin setups:
  pinMode(PIN_FLOW_METER, INPUT);
  for(int i=0;i<NUMBER_PUMPS;i++){
    pinMode(Pumps[i],OUTPUT);
  }
  
  
  Serial.begin(9600);
  //attachInterrupt(0, flowInterupt, RISING);
  //Setup timmer interupt
  long desiredFrequency=1;
  int microSeconds=(1000000/desiredFrequency);
  Timer3.initialize(microSeconds);
  Timer3.attachInterrupt(timedInterupt,microSeconds);
  //End timmed interupt setup

  //Setup for Ethernet Card
  Ethernet.begin(mac);  // initialize Ethernet device
  server.begin();           // start to listen for clients
  //End Ethernet Setup
}

void loop(){
  EthernetClient client = server.available();  // try to get client
  if(client){
    byte requestType=client.read();
    if(requestType==0){ //return status of the device
      getStatus(client);
    }else if(requestType==1||requestType==2){//Queue a drink
      makeDrink(client);
    }else if(requestType==3){//check status of drink
      
    } 
  }
}

//logic to activate a pump
void activatePump(int pumpID){
    for(int i=0;i<NUMBER_PUMPS;i++){
        digitalWrite(Pumps[i],LOW);
    }
    digitalWrite(Pumps[pumpID],HIGH);
}

//Dummy client
int greenBlink=0;
//timmed interupt for drink dispensing
void timedInterupt(){
  if(greenBlink==0){
    digitalWrite(53,HIGH);
    greenBlink=1;
  }else{
    digitalWrite(53,LOW);
    greenBlink=0;
  }
  /*
  if(drinkQueueSize>0){
    for(;volumeIndex<=NUMBER_PUMPS && drinkList[0].volumes[volumeIndex]==0; 
          volumeIndex++,pourTime=0);
    if(volumeIndex==NUMBER_PUMPS){//This drink is done.  Clean up!
      volumeIndex=0;
      drinkQueueSize--;
      for(int i=0;i<MAX_DRINKS-1;i++){
          drinkList[i]=drinkList[i++]; 
      }
    }
    else{
      pourTime++;
      if(pourTime*ML_PER_MIN/60/1000>=drinkList[0].volumes[volumeIndex]){
        drinkList[0].volumes[volumeIndex]=0;
      }
    }
  }
  */
}


void makeDrink(EthernetClient client){
  if(drinkQueueSize>=MAX_DRINKS){//currently has the max number of drinks queued
    client.flush();
    client.write((byte)01); delay(1); client.stop(); return;
  }
  //make sure the drink you are about to edit is empty:
  for(int i=0;i<NUMBER_PUMPS;i++){
    drinkList[drinkQueueSize].volumes[i]=0;
  }
  
  byte numberIngerdients=client.read();
  for(int i=0;i<numberIngerdients;i++){
    int pump=client.read();
    if(pump==-1){// client ended input stream early
      client.write((byte)2);delay(1);client.stop();return;
    }else if(pump>=NUMBER_PUMPS){//invalid pump number
      client.write((byte)5);delay(1);client.stop();return;
    }
    int volume=client.read();
    if(volume==-1){//client ended input stream early
      client.write((byte)2);delay(1);client.stop();return;
    }
    drinkList[drinkQueueSize].volumes[pump]=volume;
  }
  if(client.read()>=0){//client should have ended output, return error
    client.write((byte)2);delay(1);client.stop();return;
  }
  drinkQueueSize++;
}

void getStatus(EthernetClient client){
  client.write((byte)0);
  client.write((byte)NUMBER_PUMPS);
  client.write(fluidLevels,NUMBER_PUMPS);
  delay(1);
  client.stop();
}
//Will take a minimum of 4ms to execute.
void updateFluidLevels(){
    //probably turn on a digital pin to prevent the wire from being on for a long time
    for(int i=0;i<NUMBER_PUMPS;i++){
        fluidLevels[i]=readLevelSensor(i);
    }
    //turn off the digital pin
}
byte readLevelSensor(int pump){
    int val=analogRead(FluidMeters[pump]);
    return (byte)(val*255/1024);//scale val to a byte and return it
}


