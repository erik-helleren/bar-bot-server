//Bar bot server
//All rights reserved

#include <Ethernet.h>
#include <SPI.h>
#include <TimerThree.h>

//Ethernet globals
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,2, 20); // IP address, may need to change depending on network
EthernetServer server(1234);  // create a server at port 80


void setup(){
  //Dummy mode setup:
  pinMode(53,OUTPUT);
  pinMode(46,OUTPUT);
  digitalWrite(53,LOW);
  digitalWrite(46,LOW);
  
  Serial.begin(9600);
  //attachInterrupt(0, flowInterupt, RISING);
  //Setup timmer interupt
  long desiredFrequency=1;
  int microSeconds=(1000000/desiredFrequency);
  Timer3.attachInterrupt(timedInterupt,microSeconds);
  Timer3.start();
  //End timmed interupt setup

  //Setup for Ethernet Card
  Ethernet.begin(mac);  // initialize Ethernet device
  server.begin();           // start to listen for clients
  //End Ethernet Setup
}

void loop(){
  EthernetClient client = server.available();  // try to get client
  if(client){
    //blink code
    Serial.println("Got Client");
    byte b=client.read();
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
}
