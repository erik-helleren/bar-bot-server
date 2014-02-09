//Bar bot server
//All rights reserved

#include <Ethernet.h>
#include <SPI.h>


#define NUMBER_PUMPS 12
#define CLOCK_SPEED 16000000
#define FLOW_METER_TIMEOUT_MS 1000
#define ML_PER_MIN 1000
//Defines for the buffer array
#define VOL_INDEX buffer[0]
#define NUM_DRINKS buffer[1]
#define POUR_TIME buffer[2]




//Ethernet globals
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(10, 0, 0, 20); // IP address, may need to change depending on network
EthernetServer server(80);  // create a server at port 80

//typedef's
typedef struct Drink{
  short volumes[NUMBER_PUMPS];
  Drink *next;
};

//globals
Drink *drinkList;
//assored bytes used as a compact buffer for counters etc.  
//Do not use any index in multiple interupts/main loop, see defines on top for more info
byte buffer[4];

void setup(){
  buffer[0]=0;
  //Setup timmer interupt
  cli();//stop interrupts
  //desired inturupt frequency in Hz
  long desiredFrequency=1000;
  //calcualte the clock cycles between interupts
  int cycles=(CLOCK_SPEED/desiredFrequency)/64;
  //set timer1 interrupt at 1Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  
  OCR1A = cycles;// = (16*10^6) / (1*265) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 bit for 64 prescaler
  TCCR1B |= (1 << CS11)|(1 << CS10) ;  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei();//allow interrupts
  //End timmed interupt setup
  
  //Setup for Ethernet Card
  Ethernet.begin(mac, ip);  // initialize Ethernet device
  server.begin();           // start to listen for clients
  //End Ethernet Setup
  
  drinkList=0;
}

void loop(){
  EthernetClient client = server.available();  // try to get client
  if(client){
    //try to avoid storing the response if possible.
  }
}

//logic to activate a pump
void activatePump(int pumpID){
  
}


//timmed interupt for drink dispensing
ISR(TIMER1_COMPA_vect){
  if(drinkList!=0){
    //set buffer 0 to be the frist non-zero volume in the drink.
    for(;VOL_INDEX<=NUMBER_PUMPS && (*drinkList).volumes[VOL_INDEX]==0; 
          VOL_INDEX++,POUR_TIME=0);
    if(buffer[VOL_INDEX]==NUMBER_PUMPS){//This drink is done.  Clean up!
      VOL_INDEX=0;
      drinkList=(*drinkList).next;
      NUM_DRINKS--;//decrement the number of drinks in the drink list
    }else{
       POUR_TIME++;
       if(POUR_TIME*ML_PER_MIN/60/1000>=(*drinkList).volumes[VOL_INDEX]){
         (*drinkList).volumes[VOL_INDEX]=0;
       }
    }
    
  }
}
