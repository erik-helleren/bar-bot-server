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
#define ML_TIMEOUT 120
#define PORT 1234
#define SP(x) Serial.print(x);


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

//Drink status defines
#define QUEUED 0
#define MAKING 1
#define SUCESS 2
#define FAILURE 3
#define NO_RECORD 4
//fluid level values and pump failure for check status
#define LEVEL_CRIT 1
#define LEVEL_LOW 2
#define LEVEL_MID 3
#define LEVEL_HIGH 4
#define PUMP_FAILED 5
#define AUTH_KEY 12345
//Arrays for convenience
const int Pumps[NUMBER_PUMPS]={ D_PIN_PUMP_1,D_PIN_PUMP_2,D_PIN_PUMP_3,D_PIN_PUMP_4,
D_PIN_PUMP_5,D_PIN_PUMP_6,D_PIN_PUMP_7,D_PIN_PUMP_8 };
const int PumpSensor[NUMBER_PUMPS]={A_PIN_PUMP_LEVEL_1,A_PIN_PUMP_LEVEL_2,
A_PIN_PUMP_LEVEL_3,A_PIN_PUMP_LEVEL_4,A_PIN_PUMP_LEVEL_5,
A_PIN_PUMP_LEVEL_6,A_PIN_PUMP_LEVEL_7,A_PIN_PUMP_LEVEL_8 };

//Ethernet globals
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetServer server(PORT);  

//typedef's
typedef short did;//drink id type
typedef struct Drink{//N_PUMPS*2 + 2 bytes
	short volumes[NUMBER_PUMPS];
	did id;
};
typedef struct FinishedDrink{
	did id;
	byte code;//0 not completed, 1 completed, 2 error.
};
//globals
Drink drinkList[ MAX_DRINKS ];
FinishedDrink finishedList[MAX_DRINKS];

volatile byte finishedQueueSize=0;
volatile byte drinkQueueSize=0;//volitile becase the interupt might change the value as you are making a new drink.
//Also a single byte because the compiler makes byte operations atomic,
//required to prevent races with the interupts.
byte fluidLevels[NUMBER_PUMPS];//the last known fluid levels, check for failures before changing
volatile byte fluidInPipes[NUMBER_PUMPS];//last Checked fluids at pump output
volatile short fluidTimeout;
volatile short fluidstimedout=0;
unsigned long lastFluidCheck=0;
volatile did nextID=1;


void setup(){
	
	Serial.begin(115200);
	//Setup for Ethernet Card
	Ethernet.begin(mac);  // initialize Ethernet device
	server.begin();           // start to listen for clients
	//End Ethernet Setup
	//Pin setups:
	for(int i=0;i<NUMBER_PUMPS;i++){
		pinMode(Pumps[i],OUTPUT);
		pinMode(PumpSensor[i],INPUT);
	}
	fluidTimeout=0;
	pinMode(D_PIN_PIPE,OUTPUT);
	
	for(int i=0;i<MAX_DRINKS;i++)
		memset(drinkList[i].volumes,0,sizeof(short)*NUMBER_PUMPS);
	//Setup timer interrupt
	long desiredFrequency=ML_PER_SEC;//Set frequency to be ml/sec so
	//the interrupt calls has a 1:1 ratio of calls to ml dispensed
	int microSeconds=(1000000/desiredFrequency);
	Timer3.initialize(microSeconds);
	Timer3.attachInterrupt(timedInterupt,microSeconds);
	//End timed interrupt setup
	
	
	SP("finished setup\n");
}

void loop(){
	//Serial.println("Entering loop");
	//serialDrink(0);
	EthernetClient client = server.available();  // try to get client
	if(client){
		if(waitForAvaliableBytes(client,1,500)==-1){
			SP("No first Byte\n");
			client.stop();return;
		}
		if(authenticate(client)==0){//if authentication failed
      return;//do not proced in the loop
    }
		byte requestType=client.read();
		if(requestType==0){ //return status of the device
			SP("Status request\n");
			getStatus(client);
		}else if(requestType==1||requestType==2){//Queue a drink
			SP("make drink request\n");
			makeDrink(client);
		}else if(requestType==3){//check status of drink
			SP("Drink check\n");
			checkDrink(client);
		} 
	}
	
	if(millis()-lastFluidCheck>200){//update the fluid levels ever 10 seconds
		updateFluidLevels();
		lastFluidCheck=millis();
	}
}

//timed interrupt for drink dispensing
void timedInterupt(){
	if(drinkQueueSize>0){//TODO also add a check or delay of some sort
		updateFluidInPipes();
		int numOn=0;
		int i;
		for(i=0;i<NUMBER_PUMPS;i++){
			if(drinkList[0].volumes[i]>0){
				if(fluidInPipes[i]==HIGH){//if the sensor says there is fluid in the hose
					drinkList[0].volumes[i]--;//decrement the volume
				}//if this evaluates as false, fluid is not exiting the hose, not getting to the drink
				else{
					fluidTimeout++;//increment the timeout 
					if(fluidTimeout>ML_TIMEOUT){//if the timeout is large enough
						drinkList[0].volumes[i]=0;//give up on the fluid
						fluidstimedout++;//keep track of the fluids that timed out
						fluidLevels[i]=PUMP_FAILED;//record which pump failed so a query will let someone know what failed
					}
				}
				if(drinkList[0].volumes[i]>0){//if there is volume left
					digitalWrite(Pumps[i],HIGH);//turn on the pump
					break;//break out of loop so only one pump is on at a time
				}else{//if there isn't any volume left
					digitalWrite(Pumps[i],LOW);//make sure the pump is off
					fluidTimeout=0;//reset the timeout
					//DO NOT BREAK so the next pump can instantly come on
				}
			}else{
				digitalWrite(Pumps[i],LOW);//make sure all pumps less than the one thats on are off
			}
		}//end for each fluid
		if(i==NUMBER_PUMPS){//if there are no pumps on
			popDrinkQueue(fluidstimedout>0?FAILURE:SUCESS);//pop the drink because its finished
			//TODO any other code required here
		}else for(i++;i<NUMBER_PUMPS;i++){//turn off all the other pumps
			digitalWrite(Pumps[i],LOW);
		}
	}//end if there are drinks
	else{//there are no drinks, all pumps off
		for(int i=0;i<NUMBER_PUMPS;i++){
			digitalWrite(Pumps[i],LOW);
		}
	}
}

void popDrinkQueue(byte status){
	drinkQueueSize--;
	fluidstimedout=0;
	int prevID=drinkList[0].id;
	for(int i=0;i<MAX_DRINKS-1;i++){
		drinkList[i]=drinkList[i+1]; //pop the head of the drink list
	}
	for(int i=MAX_DRINKS-1;i>=1;i--){//remove the oldest finished drink
		finishedList[i]=finishedList[i-1];
	}
	//zero out the last entry in drinkList
	for(int i=0;i<NUMBER_PUMPS;i++){
		drinkList[MAX_DRINKS-1].volumes[i]=0;
	}
	drinkList[MAX_DRINKS-1].id=0;
	//create a new finished Drink entry
	finishedList[0].id=prevID;
	finishedList[0].code=status;
	if(finishedQueueSize<MAX_DRINKS) finishedQueueSize++;
}

void makeDrink(EthernetClient client){//TODO send drink ID to client
	if(drinkQueueSize>=MAX_DRINKS){//currently has the max number of drinks queued
		client.flush();
		client.write((byte)1); delay(1); client.stop(); return;
	}
	//make sure the drink you are about to edit is empty:
	memset(drinkList[drinkQueueSize].volumes,0,sizeof(short)*NUMBER_PUMPS);
	SP("ZERO DRINK");
	serialDrink(drinkQueueSize);
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
	drinkList[drinkQueueSize].id=nextID++;
	SP("made drink\n");
	serialDrink(drinkQueueSize);
	drinkQueueSize++;
	client.write((byte)0);
  client.write((byte)drinkList[drinkQueueSize].id/265);
  client.write((byte)drinkList[drinkQueueSize].id%256);
  delay(1);client.stop();return;
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
* Returns negative one if time out reached, otherwise it will
* return 1
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
			fluidLevels[i]=LEVEL_LOW;
		}else{
			fluidLevels[i]=LEVEL_CRIT;
		}
	}
	digitalWrite(D_PIN_LEVEL_CRITICAL,LOW);
	
	digitalWrite(D_PIN_LEVEL_LOW,HIGH);
	for(int i=0;i<NUMBER_PUMPS;i++){
		if(digitalRead(PumpSensor[i])){
			fluidLevels[i]=LEVEL_MID;
		}
	}
	digitalWrite(D_PIN_LEVEL_LOW,LOW);
	
	digitalWrite(D_PIN_LEVEL_MID,HIGH);
	for(int i=0;i<NUMBER_PUMPS;i++){
		if(digitalRead(PumpSensor[i])){
			fluidLevels[i]=LEVEL_HIGH;
		}
	}
	digitalWrite(D_PIN_LEVEL_MID,LOW);
}

void updateFluidInPipes(){
	//TODO check any other sensor lines that might be on,
	//save them, and then restore them at the end
	
	int on=0;
	if(digitalRead(D_PIN_LEVEL_CRITICAL)==HIGH){
		on=1;
		digitalWrite(D_PIN_LEVEL_CRITICAL,LOW);
	}
	if(digitalRead(D_PIN_LEVEL_LOW)==HIGH){
		on=2;
		digitalWrite(D_PIN_LEVEL_LOW,LOW);
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

void checkDrink(EthernetClient client){
	int id=0;
	if(waitForAvaliableBytes(client,2,500)==-1){
		client.stop();return;
	}//assume most significant byte first
	id+=client.read()*256;
	id+=client.read();
	
  for(int i=0;i<MAX_DRINKS && i<finishedQueueSize;i++){
		if(finishedList[i].id==id){
			client.write(((byte)finishedList[i].code));
			client.stop(); 
			return;
		}
	}
	for(int i=0;i<MAX_DRINKS && i<drinkQueueSize;i++){
    if(drinkList[i].id==id){
      if(i==0)
        client.write((byte)MAKING);
      else
        client.write((byte)QUEUED);
      client.stop();
      return;
    }
  }
	client.write((byte)0);
	client.stop();
}

void serialDrink(int drinkID){
	SP(drinkID)  SP(" id=") SP(drinkList[drinkID].id) SP(" {")
		for(int i=0;i<NUMBER_PUMPS;i++){
			SP(i) SP("=") SP((int)drinkList[drinkID].volumes[i]) SP(", ");
		}
		SP('\n');
}

//returns 1 if sucess, 0 if failure
int authenticate(EthernetClient client){
  int input=0;
  if(waitForAvaliableBytes(client,4,500)==-1){
    client.stop();return -1;
  }
  input+=client.read()*256*256*256;
  input+=client.read()*256*256;
  input+=client.read()*256;
  input+=client.read();
  if(input==AUTH_KEY){
    client.write((byte)255);
    return 1;
  }else{
    client.write((byte)254);
    return 0;
  }
}
