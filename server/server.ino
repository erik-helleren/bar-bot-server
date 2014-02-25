//Bar bot server
//All rights reserved

#include <Ethernet.h>
#include <SPI.h>
#include <TimerThree.h>


#define NUMBER_PUMPS 12
#define CLOCK_SPEED 16000000
#define FLOW_METER_TIMEOUT_MS 1000
#define ML_PER_SEC 33


//Pin defines
#define PIN_FLOW_METER 21
#define D_PIN_PIPE 22
#define D_PIN_LEVEL_CRITICAL 23
#define D_PIN_LEVEL_LOW 24

#define D_PIN_PUMP_1  53
#define D_PIN_PUMP_2  51
#define D_PIN_PUMP_3  49
#define D_PIN_PUMP_4  47
#define D_PIN_PUMP_5  45
#define D_PIN_PUMP_6  43
#define D_PIN_PUMP_7  41
#define D_PIN_PUMP_8  39
#define D_PIN_PUMP_9  37
#define D_PIN_PUMP_10 35
#define D_PIN_PUMP_11 33
#define D_PIN_PUMP_12 31

#define A_PIN_PUMP_LEVEL_1 52
#define A_PIN_PUMP_LEVEL_2 50
#define A_PIN_PUMP_LEVEL_3 48
#define A_PIN_PUMP_LEVEL_4 46
#define A_PIN_PUMP_LEVEL_5 44
#define A_PIN_PUMP_LEVEL_6 42
#define A_PIN_PUMP_LEVEL_7 40
#define A_PIN_PUMP_LEVEL_8 38
#define A_PIN_PUMP_LEVEL_9 36
#define A_PIN_PUMP_LEVEL_10 34
#define A_PIN_PUMP_LEVEL_11 32
#define A_PIN_PUMP_LEVEL_12 30

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
typedef struct Drink{//12*2=28 bytes
  short volumes[NUMBER_PUMPS];
};

//globals
Drink drinkList[ MAX_DRINKS ];
byte volumeIndex=0;//Used in interupt to check what its currently pouring
volatile byte drinkQueueSize=0;//volitile becase the interupt might change the value as you are making a new drink.
//Also a single byte because the compiler makes byte operations atomic,
//required to prevent races with the interupts.
int flowMeterCount=0;
unsigned long lastFlowMeterTime;//last time the flow metter was accessed.
byte fluidLevels[NUMBER_PUMPS];//the last know fluid levels
byte fluidInPipes[NUMBER_PUMPS];//last Checked fluids at pump output
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
    pinMode(FluidMeters[i],INPUT);
  }
  pinMode(D_PIN_PIPE,OUTPUT);
  
  
  Serial.begin(9600);
  //attachInterrupt(0, flowInterupt, RISING);
  //Setup timmer interupt
  long desiredFrequency=ML_PER_SEC;//Set frequency to be ml/sec so
    //the interupt calls has a 1:1 ratio of calls to ml dispensed
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
      Serial.println("Make drink");
      makeDrink(client);
    }else if(requestType==3){//check status of drink
      
    } 
  }
}

//Dummy client
int greenBlink=0;
//timmed interupt for drink dispensing
void timedInterupt(){
  if(drinkQueueSize>0){
    updateFluidInPipes();
    int numOn=0;
    for(int i=0;i<NUMBER_PUMPS;i++){
      if(drinkList[0].volumes[i]>0){
        if(fluidInPipes[i]==HIGH){//if the sensor says there is fluid in the hose
          drinkList[0].volumes[i]--;
        }//if this evaluates as false, fluid is not exiting the hose, not getting to the drink
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
  for(int i=0;i<NUMBER_PUMPS;i++)
    drinkList[MAX_DRINKS-1].volumes[i]=0;
  
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
  client.write((byte)0);
  client.write((byte)NUMBER_PUMPS);
  for(int i=0;i<NUMBER_PUMPS;i++)
    client.write(2);
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

void updateFluidInPipes(){
  digitalWrite(D_PIN_PIPE,HIGH);
  //maybeDelay
  for(int i=0;i<NUMBER_PUMPS;i++){
    fluidInPipes[i]=digitalRead(FluidMeters[i]);
  }
}


