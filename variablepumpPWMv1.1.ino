// This #include statement was automatically added by the Particle IDE.
#include <OneWire.h>  //For Temp Sensor include this library

STARTUP(WiFi.selectAntenna(ANT_EXTERNAL)); // Use external antenna
SYSTEM_MODE(SEMI_AUTOMATIC);    // take control of the wifi behavior to avoid cloud bogging down local functions
SYSTEM_THREAD(ENABLED);    // enable so loop and setup run immediately as well as allows wait for to work properly for the timeout of wifi


// define pin names for easier reference in the code
#define pPumpRelay1     D4
#define pPumpRelay2     D5
#define pPumpRelay3     D6
#define tempPower       A0
#define tempGround      A1
#define chlorinerelay   D0
#define aeratoronrelay  D1
#define ccwvalverelay   D2
#define cwvalverelay    D3
#define poollight       D7
#define acidrelay       A3    // remember this relay is a high input on and is a seperate relay

// Pump Speed Variables
int pumpSetting = 0;
int pumpSpeed = 0;
int pumpPowerConsumption = 0;
int totalKWH = 0;

//** temp variables
OneWire ds = OneWire(A2);  //** 1-wire signal on pin A2
unsigned long lastUpdate = 0;
float lastTemp;
int tLast = -1;
int ioat = -777;

// Pool Temp & Level Variables
double poolTemp = 0.0;
int poolLevel = 0;
int checkinTime =  -1;

// Time Variables for scheduling
int month = 13;
int hour = 25;
int minute = 61;
int day = 0;
int daylasttimesync = 0;
int secCoff = 0;
int secAoff = 0;
int previousminute = 61;
bool minutechange = false;
int setting1 = 3600;  // second setting 1 happens
int setting2 = 23400;  // second that setting 2 happens
int setting3 = 28800; // second that setting 3 happens
int setting4 = 54000; // second that setting 4 happens
int setting5 = 72000;  // second that setting 5 happens
int setting6 = 84600;  // second setting 6 happens

// auto mode variable
bool automode = true;
bool autoFlag = false;

// cloud connected variable
bool cloudconnected = false;
int discTime = -1;
int discUTC = -1;
int rssi = 0;

void setup() {
Particle.connect();    // attempt to connect to the cloud
waitFor(Particle.connected, 30000); // allow 30 seconds to conncect
if (Particle.connected()){   // check if connection is true than set the variable to true
cloudconnected = true;
}
// Set Timezone
  Time.zone(-7);
// Set Pump Pins to Outputs
  pinMode(pPumpRelay1, OUTPUT); 
  pinMode(pPumpRelay2, OUTPUT); 
  pinMode(pPumpRelay3, OUTPUT); 

// Register Cloud Functions for Pump
  Particle.function("PumpSpeed", SetSpeed);
  Particle.function("automode", automodefunc);
  Particle.variable("PumpRPM", pumpSpeed);
  Particle.variable("PowerUse", pumpPowerConsumption);
  Particle.variable("TotalKWH", totalKWH);
// Register Cloud Functions for valves
  Particle.function("valves", valvefunc);
  Particle.variable("valvepos", valveposition);
//** register cloud functions for temp  
  Particle.variable("OAT", ioat);
// Cloud Variable for RSSI
  Particle.variable("RSSI", rssi);

//** temp pin setup
  pinMode(tempPower, OUTPUT); //** power temp sensor
  pinMode(tempGround, OUTPUT); //** ground temp sensor
  digitalWrite(tempPower, HIGH);
  digitalWrite(tempGround, LOW);


void loop() {
// check if Im connected
if (!Particle.connected() && cloudconnected == true){   // check once a loop for a disconnect if its not connected and the variable says it is
  Particle.connect();  // try and connect
  if(!waitFor(Particle.connected, 7000)){   // if it takes longer than 7 seconds run disconnect and move on but schedule a re connect attempt
  Particle.disconnect();
  cloudconnected = false;
  discTime = Time.local() % 86400;  // note the disconnect time
  discUTC = Time.now();  // note the time from utc to easily schedule reattempt
  }
  if (Particle.connected()){    // if the reconnect attempt worked change the variable
  cloudconnected = true;
  Particle.publish("cloudreconnect", String(discTime), PRIVATE); // let me know it went down and had to reconnect this way
  Particle.publish("refresh", "true", PRIVATE); // refresh all the shit too 
  rssi = WiFi.RSSI();
  }
}

// scheduled reconnects
int curSec = Time.now();
int tsinceDisc = (curSec - discUTC);
 
if (cloudconnected == false && tsinceDisc >= 30){
   Particle.connect();
   if(!waitFor(Particle.connected, 7000)){   // if it takes longer than 7 seconds run disconnect and move on but schedule a re connect attempt
   Particle.disconnect();
   cloudconnected = false;
   discUTC = Time.now(); // restart the counter for a recheck but leave the second of the day timestamp alone for accurate tracking
   }
   if (Particle.connected()){    // if the reconnect attempt worked change the variable
   cloudconnected = true;
   Particle.publish("cloudreconnect", String(discTime), PRIVATE); // let me know it went down and had to reconnect this way
   Particle.publish("refresh", "true", PRIVATE); // refresh all the shit too 
  }
   
}

  // check time for schedul based functions
  checktime();
  
  //** temp loop code checks the time and decides if we need to check outside temp / wifi RSSI
  int secNow = Time.local() % 86400;  // whats the current second of the day
  int tSince = (secNow - tLast);
  if (tSince < 0)                      // correct for 24 hour time clock
  {
    tSince = ((86400 - tLast) + secNow);
  }
  if (tSince >= 600 or tLast == -1)   // run every ten minutes or first boot
  {
    getOAT();
    rssi = WiFi.RSSI();
  }
  
 // Auto mode selected flag to avoid cloud delays
 if (autoFlag == true)
 {
    resumeschedule();
 }

}

// get a temp reading from the probe
void getOAT() {
byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  if ( !ds.search(addr)) {
    ds.reset_search();
    delay(250);
    return;
  }
  switch (addr[0]) {
    case 0x10:
      type_s = 1;
      break;
    case 0x28:
      type_s = 0;
      break;
    case 0x22:
      type_s = 0;
      break;
    case 0x26:
      type_s = 2;
      break;
    default:
      return;
  }
  ds.reset();               
  ds.select(addr);          
  ds.write(0x44, 0);        
  delay(1000);     
  present = ds.reset();
  ds.select(addr);
  ds.write(0xB8,0);         
  ds.write(0x00,0);         
  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE,0);         
  if (type_s == 2) {
    ds.write(0x00,0);       
  }
  for ( i = 0; i < 9; i++) {           
    data[i] = ds.read();
    }
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s == 2) raw = (data[2] << 8) | data[1];
  byte cfg = (data[4] & 0x60);
  switch (type_s) {
    case 1:
      raw = raw << 3; 
      if (data[7] == 0x10) {
        raw = (raw & 0xFFF0) + 12 - data[6];
      }
      celsius = (float)raw * 0.0625;
      break;
    case 0:
      
      if (cfg == 0x00) raw = raw & ~7;
      if (cfg == 0x20) raw = raw & ~3;
      if (cfg == 0x40) raw = raw & ~1;
      celsius = (float)raw * 0.0625;
      break;
    case 2:
      data[1] = (data[1] >> 3) & 0x1f;
      if (data[2] > 127) {
        celsius = (float)data[2] - ((float)data[1] * .03125);
      }else{
        celsius = (float)data[2] + ((float)data[1] * .03125);
      }
  }
  if((((celsius <= 0 && celsius > -1) && lastTemp > 5)) || celsius > 125) {
      celsius = lastTemp;
  }
  fahrenheit = celsius * 1.8 + 32.0;
  lastTemp = celsius;
  String oat = String(fahrenheit);
  if (cloudconnected == true){
  Particle.publish("OAT", oat, PRIVATE);
  }
  ioat = atoi(oat);
  tLast = Time.local() % 86400;
}


    
// cleaner on function
void cleanersOn() {
    if (valveposition == 1){
        digitalWrite(cwvalverelay, HIGH);
        digitalWrite(ccwvalverelay, LOW);
        delay(17500);
        digitalWrite(ccwvalverelay, HIGH);
    }
    if (valveposition == 2){
        //do nothing
    }
    if (valveposition == 3){
        digitalWrite(ccwvalverelay, HIGH);
        digitalWrite(cwvalverelay, LOW);
        delay(18000);
        digitalWrite(cwvalverelay, HIGH);
    }
    digitalWrite(aeratoronrelay, HIGH);   // shut off aerator if its running
    valveposition = 2;  // update stored valve position
    wattchange();                        // Recalculate Power Consumption for valve position

}

// runs if valve changes to update pump power use
void wattchange() {
    if (pumpSetting == 6)
    {
      if (valveposition == 1)  
      {
        pumpPowerConsumption = 972;
      }
      else
      {
        pumpPowerConsumption = 890; 
      }
    }
    if (pumpSetting == 7)
    {
      if (valveposition == 1)
      {
        pumpPowerConsumption = 1472;
      }
      else
      {
        pumpPowerConsumption = 1370; 
      }
    }
    if (pumpSetting == 1){
        if (valveposition == 3){
        pumpPowerConsumption = 62;
        }
        else{
        pumpPowerConsumption = 93;
        }
    }
    if (pumpSetting == 2){
        if (valveposition == 3){
        pumpPowerConsumption = 92;
        }
        else{
        pumpPowerConsumption = 145;
        }
    }
    if (pumpSetting == 3){
        if (valveposition == 3){
        pumpPowerConsumption = 128;
        }
        else{
        pumpPowerConsumption = 200;
        }
    }
    if (cloudconnected == true){
    Particle.publish("wattchange", "true", PRIVATE);
    }
}

// Runs when called upon in loop to check for a regularly scheduled event such as pump speed changes valve changes and chemical pump changes at set times of the day
void checktime() {
hour = Time.hour();
minute = Time.minute();
month = Time.month();
day = Time.day();
//check if the minute changed so stuff runs once
if (minute != previousminute){
    minutechange = true;
    previousminute = minute;
}
else {
    minutechange = false;
}

int cursec = Time.local() % 86400;
char Cpumpstate = digitalRead(chlorinerelay);
char Apumpstate = digitalRead(acidrelay);

//
// chlorine pump auto schedule
//

//
// pump and valve auto runs
//

//  6:30 am full speed and floor cleaners  setting 2

if (hour == 6 && minute == 30 && minutechange == true && automode == true){
    aeratorautorun = false;
    cleanersOn();
    SetSpeed("7");
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}

// 8 am 1500 rpm main returns setting 3

if (hour == 8 && minute == 0 && minutechange == true && automode == true){
    mainsOn();
    SetSpeed("2");
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}

// 3 pm off for peak except saturdays and sundays setting 4

if (hour == 15 && minute == 0 && minutechange == true && automode == true && day != 1 && day != 7){
    SetSpeed("0");
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}

// 8 pm 1500 RPM setting 5

if (hour == 20 && minute == 0 && minutechange == true && automode == true){
    mainsOn();
    SetSpeed("2");
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}

//  11:30 pm full speed and floor cleaners setting 6

if (hour == 23 && minute == 30 && minutechange == true && automode == true){
    cleanersOn();
    SetSpeed("7");
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}

//  1:00 am 1500 rpm or 1750 and aerator depending on pool temp setting 1

if (hour == 1 && minute == 0 && minutechange == true && automode == true){
    if (poolTemp < 86){ // dont run aerator if pool is cold
      mainsOn();
      SetSpeed("2");
      if (cloudconnected == true){
      Particle.publish("refresh", "true", PRIVATE);
      }
    }
    if (poolTemp >= 86){ // do run aerator if pool is warm
    SetSpeed("3");
    delay(5000); // give pump time to react before valves move
    aeratorOn();
    aeratorautorun = true;
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
    }
}
//
// update the clock once a day
//

if (cursec >= 11700 && day != daylasttimesync){ // 3:15 am but only once
Particle.syncTime();
daylasttimesync = day;
}

}

// if auto mode changes to on this gets called and it determines where in the schedule you should be and resumes the schedule
void resumeschedule() {
autoFlag = false; // reset the flag to avoid it running every loop
int checkT = Time.local() % 86400;
if (checkT >= setting1 && checkT < setting2){  // check setting 1

    if (poolTemp < 86){ // dont run aerator if pool is cold
      mainsOn();
      SetSpeed("2");
      if (cloudconnected == true){
      Particle.publish("refresh", "true", PRIVATE);
      }
    }
    if (poolTemp >= 86){ // do run aerator if pool is warm
    SetSpeed("3");
    delay(5000); // give pump time to react before valves move
    aeratorOn();
    aeratorautorun = true;
      if (cloudconnected == true){
      Particle.publish("refresh", "true", PRIVATE);
      }
    }
}

if (checkT >= setting2 && checkT < setting3){   // check setting 2
    aeratorautorun = false;
    cleanersOn();
    SetSpeed("7");
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}
if (checkT >= setting3 && checkT < setting4){   // check setting 3
    mainsOn();
    SetSpeed("2");
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}
if (checkT >= setting4 && checkT < setting5){   // check setting 4
   
    if (day !=1 && day != 7){
    SetSpeed("0");
      if (cloudconnected == true){
      Particle.publish("refresh", "true", PRIVATE);
      }
    }
    else {
    mainsOn();
    SetSpeed("2");
      if (cloudconnected == true){
      Particle.publish("refresh", "true", PRIVATE);
      }
    }
}
if (checkT >= setting5 && checkT < setting6){   // check setting 5
    mainsOn();
    SetSpeed("2");
      if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
      }
}
if (checkT >= setting6 or checkT < setting1){   // check setting 6
    cleanersOn();
    SetSpeed("7");
      if (cloudconnected == true){
      Particle.publish("refresh", "true", PRIVATE);
      }
}
}


int SetSpeed(String command)
{
// cloud trigger for pool speed
  if (command == "0") 
    {   
      digitalWrite(pPumpRelay1, HIGH);
      digitalWrite(pPumpRelay2, HIGH);
      digitalWrite(pPumpRelay3, HIGH);
      pumpSetting = 0;
      pumpSpeed = 0;
      pumpPowerConsumption = 0;
      return 0;
    }
  if (command == "1")
   {
      digitalWrite(pPumpRelay1, LOW);
      digitalWrite(pPumpRelay2, HIGH);
      digitalWrite(pPumpRelay3, HIGH);
      pumpSetting = 1;
      pumpSpeed = 1250;
      if (valveposition == 1 or valveposition == 2){
      pumpPowerConsumption = 93;
      }
      if (valveposition == 3){
      pumpPowerConsumption = 62;
      }    
      return 1;
   }
  if (command == "2")
   {
      digitalWrite(pPumpRelay1, HIGH);
      digitalWrite(pPumpRelay2, LOW);
      digitalWrite(pPumpRelay3, HIGH);
      pumpSetting = 2;
      pumpSpeed = 1500;
      if (valveposition == 1 or valveposition == 2){
      pumpPowerConsumption = 145;
      }
      if (valveposition == 3){
      pumpPowerConsumption = 92;
      }
      return 2;
   }
   if (command == "3")
   {
      digitalWrite(pPumpRelay1, LOW);
      digitalWrite(pPumpRelay2, LOW);
      digitalWrite(pPumpRelay3, HIGH);
      pumpSetting = 3;
      pumpSpeed = 1750;
      if (valveposition == 1 or valveposition == 2){
      pumpPowerConsumption = 200;
      }
      if (valveposition == 3){
      pumpPowerConsumption = 128;
      }
      return 3;
   }
   if (command == "4")
   {
      digitalWrite(pPumpRelay1, HIGH);
      digitalWrite(pPumpRelay2, HIGH);
      digitalWrite(pPumpRelay3, LOW);
      pumpSetting = 4;
      pumpSpeed = 2000;
      pumpPowerConsumption = 310;
       return 4;
   }
   if (command == "5")
   {
      if (valveposition == 3){
        mainsOn(); //shutoff aerator first to protect pipes if pump is selected above this speed 
      }
      digitalWrite(pPumpRelay1, LOW);
      digitalWrite(pPumpRelay2, HIGH);
      digitalWrite(pPumpRelay3, LOW);
      pumpSetting = 5;
      pumpSpeed = 2500;
      pumpPowerConsumption = 570;
      return 5;
   }
   if (command == "6")
   {
       if (valveposition == 3){
        mainsOn(); //shutoff aerator first to protect pipes if pump is selected above this speed 
      }
      digitalWrite(pPumpRelay1, HIGH);
      digitalWrite(pPumpRelay2, LOW);
      digitalWrite(pPumpRelay3, LOW);
      pumpSetting = 6;
      pumpSpeed = 3000;
      if (valveposition == 1)   // cleaners off power consumption is higher ill only use cleaners at 3000 oor 3450 rpm
      {
        pumpPowerConsumption = 972;
      }
      else
      {
        pumpPowerConsumption = 890; 
      }
      return 6;
   }
   if (command == "7")
   {
       if (valveposition == 3){
        mainsOn(); //shutoff aerator first to protect pipes if pump is selected above this speed 
      }
      digitalWrite(pPumpRelay1, LOW);
      digitalWrite(pPumpRelay2, LOW);
      digitalWrite(pPumpRelay3, LOW);
      pumpSetting = 7;
      pumpSpeed = 3450;
      if (valveposition == 1)   // cleaners off power consumption is higher ill only use cleaners at 3000 oor 3450 rpm
      {
        pumpPowerConsumption = 1472;
      }
      else
      {
        pumpPowerConsumption = 1370; 
      }
      return 7;
   }
   else  // bad command shutoff pump return 9
    {               
      digitalWrite(pPumpRelay1, HIGH);
      digitalWrite(pPumpRelay2, HIGH);
      digitalWrite(pPumpRelay3, HIGH);
      pumpSetting = 0;
      pumpSpeed = 0;
      pumpPowerConsumption = 0;
      return 9;
    }
}
// cloud trigger for auto mode change
int automodefunc(String command)
{
// cloud trigger for auto mode on    
    if (command == "1") 
    {   
      automode = true;
      autoFlag = true;
      return 1;
    }
// cloud trigger for auto mode off
  else 
    {               
       automode = false;
      return 0;
    }
}

