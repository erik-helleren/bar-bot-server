#include <Ethernet.h>
#include <SPI.h>

#define MAX_DRINKS 10
#define NUMBER_PUMPS 12
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

//typedef's
typedef struct Drink{//12*2=28 bytes
  short volumes[NUMBER_PUMPS];
};

//globals
Drink drinkList[ MAX_DRINKS ];
byte volumeIndex=0;//Used in interupt to check what its currently pouring
byte drinkQueueSize=0;//volitile becase the interupt might change the value as you are making a new drink.
//Also a single byte because the compiler makes byte operations atomic,
//required to prevent races with the interupts.
EthernetServer server(1234);  // create a server at port 80
void setup(){
  Serial.begin(9600);
  Serial.println("Seting up ethernet");
  //Setup for Ethernet Card
  Ethernet.begin(mac);  // initialize Ethernet device
  server.begin();           // start to listen for clients
  //End Ethernet Setup
  delay(1);
  Serial.println("Done with setup");
}

void loop(){
  //Serial.println("Entering loop");
  EthernetClient client = server.available();  // try to get client
  if(client){
    Serial.println("client gotten");
    while(client.available()==0);
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

void makeDrink(EthernetClient client){
  if(drinkQueueSize>=MAX_DRINKS){//currently has the max number of drinks queued
    Serial.println("queue full");
    client.flush();
    client.write((byte)1); delay(1); client.stop(); return;
  }
  //make sure the drink you are about to edit is empty:
  for(int i=0;i<NUMBER_PUMPS;i++){
    drinkList[drinkQueueSize].volumes[i]=0;
  }
  while(client.available()==0);
  byte numberIngerdients=client.read();
  Serial.print("Got ingeridents: ");
  Serial.print((int)numberIngerdients);
  Serial.println(' ');
  while(client.available()<numberIngerdients*2);
  Serial.println("Got avaliable bytes");
  for(int i=0;i<numberIngerdients;i++){
    int pump=client.read();
    Serial.print("Pump : ");
    Serial.print(pump);
    Serial.println(' ');
    if(pump==-1){// client ended input stream early
      Serial.println("No pump");
      client.write((byte)2);delay(1);client.stop();return;
    }else if(pump>=NUMBER_PUMPS){//invalid pump number
      client.write((byte)5);delay(1);client.stop();return;
    }
    int volume=client.read();
    Serial.print("Volume : ");
    Serial.print(volume);
    Serial.println(' ');
    if(volume==-1){//client ended input stream early
      client.write((byte)2);delay(1);client.stop();return;
    }
    drinkList[drinkQueueSize].volumes[pump]=volume;
  }
  if(client.read()>=0){//client should have ended output, return error
    client.write((byte)2);delay(1);client.stop();return;
  }   
  Serial.println("Made Drink:");
  for(int i=0;i<NUMBER_PUMPS;i++){
    Serial.print(i);
    Serial.print(" : ");
    Serial.print(drinkList[drinkQueueSize].volumes[i]);
    Serial.print(" , ");
  }
  Serial.println(' ');
  drinkQueueSize++;
  client.write((byte)1);delay(1);client.stop();return;
}

void getStatus(EthernetClient client){
  client.write((byte)0);
  client.write((byte)NUMBER_PUMPS);
  for(int i=0;i<NUMBER_PUMPS;i++)
    client.write(2);
  delay(1);
  client.stop();
}

