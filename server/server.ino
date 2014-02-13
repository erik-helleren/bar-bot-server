//Bar bot server
//All rights reserved

#include <Ethernet.h>
#include <SPI.h>


#define NUMBER_PUMPS 12
#define CLOCK_SPEED 16000000
#define FLOW_METER_TIMEOUT_MS 1000
#define ML_PER_MIN 1000


//Pin defines
#define PIN_FLOW_METER 21
#define PIN_PUMP_1  22
#define PIN_PUMP_2  23
#define PIN_PUMP_3  24
#define PIN_PUMP_4  25
#define PIN_PUMP_5  26
#define PIN_PUMP_6  27
#define PIN_PUMP_7  28
#define PIN_PUMP_8  29
#define PIN_PUMP_9  30
#define PIN_PUMP_10 31
#define PIN_PUMP_11 32
#define PIN_PUMP_12 33


//Ethernet globals
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,2, 20); // IP address, may need to change depending on network
EthernetServer server(1234);  // create a server at port 80

//typedef's
typedef struct Drink{//12*2+4=28 bytes
  short volumes[NUMBER_PUMPS];
  Drink *next;
};

//globals
Drink *drinkList;

byte volumeIndex=0;//Used in interupt to check what its currently pouring
int drinkQueueSize=0;//The size of the drink queue.  Disable interupts before changing outside of interupt
long pourTime=0;//Used in interupt to count how long it has been pouring
int flowMeterCount=0;
unsigned long lastFlowMeterTime;//last time the flow metter was accessed.


//interupt for the flow meter
void flowInterupt(){
  flowMeterCount++;
  lastFlowMeterTime=millis();
}

void setup(){
  //Dummy mode setup:
  pinMode(53,OUTPUT);
  pinMode(46,OUTPUT);
  digitalWrite(53,LOW);
  digitalWrite(46,LOW);
  
  //Pin setups:
  /*pinMode(PIN_FLOW_METER, INPUT);
  pinMode(PIN_PUMP_1 , OUTPUT);
  pinMode(PIN_PUMP_2 , OUTPUT);
  pinMode(PIN_PUMP_3 , OUTPUT);
  pinMode(PIN_PUMP_4 , OUTPUT);
  pinMode(PIN_PUMP_5 , OUTPUT);
  pinMode(PIN_PUMP_6 , OUTPUT);
  pinMode(PIN_PUMP_7 , OUTPUT);
  pinMode(PIN_PUMP_8 , OUTPUT);
  pinMode(PIN_PUMP_9 , OUTPUT);
  pinMode(PIN_PUMP_10, OUTPUT);
  pinMode(PIN_PUMP_11, OUTPUT);
  pinMode(PIN_PUMP_12, OUTPUT);
  */
  
  Serial.begin(9600);
  //attachInterrupt(0, flowInterupt, RISING);
  //Setup timmer interupt
  cli();//stop interrupts
  long desiredFrequency=1;
  int cycles=2*(CLOCK_SPEED/desiredFrequency)/1024;
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A = cycles;
  TCCR1B |= (1 << WGM12);
  // Set CS10 bit for 64 prescaler
  TCCR1B |= (1 << CS12)|(1 << CS10) ;  
  TIMSK1 |= (1 << OCIE1A);
  sei();//allow interrupts
  //End timmed interupt setup

  //Setup for Ethernet Card
  Ethernet.begin(mac);  // initialize Ethernet device
  server.begin();           // start to listen for clients
  //End Ethernet Setup
  
  //Data setup
  //drinkList=0;
}
byte b=0;
void loop(){
  EthernetClient client = server.available();  // try to get client
  if(client){
    Serial.println("Got Client");
    b=client.read();
    Serial.println(b);
    while(client.read()>=0);
    client.stop();
    for(;b>0;b--){
      digitalWrite(46,HIGH);
      delay(1000);
      digitalWrite(46,LOW);
      delay(1000);
    }
  }
}

//logic to activate a pump
void activatePump(int pumpID){

}
//Dummy client
int greenBlink=0;
int numberRed=0;
//timmed interupt for drink dispensing
ISR(TIMER1_COMPA_vect){
  if(greenBlink==0){
    digitalWrite(53,HIGH);
    greenBlink=1;
  }else{
    digitalWrite(53,LOW);
    greenBlink=0;
  }
  /*
  if(drinkList!=0){
    for(;volumeIndex<=NUMBER_PUMPS && (*drinkList).volumes[volumeIndex]==0; 
          volumeIndex++,pourTime=0);
    if(volumeIndex==NUMBER_PUMPS){//This drink is done.  Clean up!
      volumeIndex=0;
      drinkList=(*drinkList).next;
      drinkQueueSize--;//decrement the number of drinks in the drink list
    }
    else{
      pourTime++;
      if(pourTime*ML_PER_MIN/60/1000>=(*drinkList).volumes[volumeIndex]){
        (*drinkList).volumes[volumeIndex]=0;
      }
    }
  }*/
}




