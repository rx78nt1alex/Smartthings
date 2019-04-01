// This #include statement was automatically added by the Particle IDE.
#include <EmonLib.h>
#include <OneWire.h> 


//This is bscuderi13's own pool controller and does not incorporate a variable speed pump,
//but I will upgrade to that later. Instead, I will comment out any mention of variable speeds.
//I will be using a 10 amp relay to control a 20 amp relay, as the single speed
//pump draws about 11-15 amps, so as not to burn out my relay board.
//******Temperature Monitoring and Control******
//I am using a NCI.IO 4 channel 4-20 mA input


OneWire ds = OneWire(D4);       //For Temp Sensor
EnergyMonitor emon1;            // Create an instance


STARTUP(WiFi.selectAntenna(ANT_EXTERNAL)); // Use external antenna
SYSTEM_MODE(SEMI_AUTOMATIC);    // take control of the wifi behavior to avoid cloud bogging down local functions
SYSTEM_THREAD(ENABLED);    // enable so loop and setup run immediately as well as allows wait for to work properly for the timeout of wifi



// pins D0 and D1 cannot be used for relay control due to being the I2C comm pins
//When I add pH, ORP, and conductivity, they are I2C devices and may be 
//piggybacked onto the PSCREW screw terminals to communicate with, which uses D0 and D1.
//Direct Photon Relay control
#define pPumpSpdpwm  RX
//int pPumpSpeedLo = D5; Since this is PWM, low can be connected to ground with the Adafruit DRV8871
#define pCleaner     D0
#define tempPower    D5
#define tempGround   D3

// Pump Speed Variables
int pumpSetting = 0;
int pumpSpeed = 0;
int oat = 0;
//int pumpPowerConsumption = realPower;
int totalKWH = 0;

unsigned long lastRequest  = 0;

//** temp variables
//OneWire ds = OneWire(D4);  //** 1-wire signal on pin A4
byte addrs[2][8] = {{0x28, 0xFF, 0xAD, 0x64, 0xB1, 0X17, 0x5, 0xA3}, {0x28, 0xFF, 0xD8, 0x0F, 0xB2, 0x17, 0x5, 0xFC}};
//Since it is a 1 wire system and multiples can be used, more than one can be put on the same pin
unsigned long lastUpdate = 0;
float lastTemp;
int tLast = -1;
int ioat = -777;
//int hour = Time.hour();

double realPower       = emon1.realPower;        //extract Real Power into variable
double apparentPwr   = emon1.apparentPower;    //extract Apparent Power into variable
double pwerFactor     = emon1.powerFactor;      //extract Power Factor into Variable
double supplyVolts   = emon1.Vrms;             //extract Vrms into Variable
double Irms            = emon1.Irms;             //extract Irms into Variable

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
    Wire.setSpeed(400000);
    Wire.begin();   
    Serial.begin(9600);
    
    
    Particle.connect();    // attempt to connect to the cloud
    waitFor(Particle.connected, 30000); // allow 30 seconds to conncect
    if (Particle.connected()){   // check if connection is true than set the variable to true
    cloudconnected = true;
    }
    
// Set Timezone
    Time.zone(-5);
// Set calibration factors for energy monitor
    emon1.voltage(0, 184.986, 1);  // Voltage: input pin, calibration, phase_shift
    emon1.current(1, 17.90);       // Current: input pin, calibration.


// Set Pump Pins to Outputs
    pinMode(pPumpSpdpwm, OUTPUT);     // sets the pin as output
        analogWriteResolution(pPumpSpdpwm, 12); // sets analogWrite resolution to 12 bits
    pinMode(pCleaner, OUTPUT); 
    //pinMode(pPumpRelay3, OUTPUT); 
    
// Register Cloud Functions for Pump
    Particle.function("PumpSpeed", SetSpeed);
    Particle.function("automode", automodefunc);
    Particle.variable("PumpRPM", pumpSpeed);
    Particle.variable("PowerUse", realPower);
    Particle.variable("TotalKWH", totalKWH);
   // Particle.variable("PoolTemp", oat);
// Register Cloud Functions for valves
 //   Particle.function("aerator", aeratorfunc);
 //   Particle.function("floorcleaner", cleanerfunc);
// subscribe to and publish temp & levelchanges
 //   Particle.subscribe("CurrentTemperature", tempHandler, MY_DEVICES);
 //   Particle.subscribe("waterlevel", levelHandler, MY_DEVICES);
 //   Particle.variable("PoolTemp", poolTemp);
 //   Particle.variable("PoolLevel", poolLevel);
//** register cloud functions for temp  
     //Particle.variable("WaterTemp", ioat);
    
// Cloud Variable for RSSI
    Particle.variable("RSSI", rssi);

//** temp pin setup
    pinMode(tempPower, OUTPUT); //** power temp sensor
    pinMode(tempGround, OUTPUT); //** ground temp sensor
    digitalWrite(tempPower, HIGH);
    digitalWrite(tempGround, LOW);
    
// Set pool cleaner pin to off
    digitalWrite(pCleaner, HIGH);

}

// get a temp reading

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
   
   
   emon1.calcVI(20,2000);         // Calculate all. No.of half wavelengths (crossings), time-out
   emon1.serialprint();           // Print out all variables (realpower, apparent power, Vrms, Irms, power factor)
   double realPower       = emon1.realPower;        //extract Real Power into variable
   double apparentPwr   = emon1.apparentPower;    //extract Apparent Power into variable
   double pwerFactor     = emon1.powerFactor;      //extract Power Factor into Variable
   double supplyVolts   = emon1.Vrms;             //extract Vrms into Variable
   double Irms            = emon1.Irms;  
   
   Particle.variable("RMS Volts", supplyVolts);  // This is what exposes the funtion to the internet!
   Particle.variable("RMS Current", Irms);  // NOTE: Variable names can only be a max of twelve characters!
 
   Particle.variable("App Power", apparentPwr);
   Particle.variable("Pwr Factor", pwerFactor);
   
   
   
    Particle.publish("RMS Volts", String(supplyVolts), PRIVATE);  // This is what exposes the funtion to the internet!
    Particle.publish("RMS Current", String(Irms), PRIVATE);  // NOTE: Variable names can only be a max of twelve characters!
    Particle.publish("Real Power", String(realPower), PRIVATE);
    Particle.publish("App Power", String(apparentPwr), PRIVATE);
    Particle.publish("Pwr Factor", String(pwerFactor), PRIVATE);
    Particle.publish("Water Temp", String(lastTemp), PRIVATE);
    Particle.variable("PowerUse", realPower);
    delay(10000);
    
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
  byte addr[2];
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
   //Particle.publish("PoolTemp", String(oat), PRIVATE);
   Particle.variable("PoolTemp", String(oat));
  }
  ioat = atoi(oat);
  tLast = Time.local() % 86400;
}

//Particle.variable("OAT", ioat);
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

//  6:30 am full speed and floor cleaners
if (hour == 6 && minute == 30 && minutechange == true && automode == true){
    SetSpeed("7");
    digitalWrite(pCleaner, LOW);
   if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}

// 8 am 1500 rpm main returns setting 3

if (hour == 8 && minute == 0 && minutechange == true && automode == true){
    SetSpeed("2");
    digitalWrite(pCleaner, HIGH);
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
    SetSpeed("2");
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}

//  11:30 pm full speed and floor cleaners setting 6

if (hour == 23 && minute == 30 && minutechange == true && automode == true){
    SetSpeed("7");
    digitalWrite(pCleaner, LOW);
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}

//  1:00 am 1500 rpm or 1750 and aerator depending on pool temp setting 1

if (hour == 1 && minute == 0 && minutechange == true && automode == true){
      SetSpeed("2");
      digitalWrite(pCleaner, HIGH);
      if (cloudconnected == true){
      Particle.publish("refresh", "true", PRIVATE);
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

if (checkT >= setting2 && checkT < setting3){   // check setting 2
   
    SetSpeed("7");
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}
if (checkT >= setting3 && checkT < setting4){   // check setting 3
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
    SetSpeed("2");
      if (cloudconnected == true){
      Particle.publish("refresh", "true", PRIVATE);
      }
    }
}
if (checkT >= setting5 && checkT < setting6){   // check setting 5
    SetSpeed("2");
      if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
      }
}
if (checkT >= setting6 or checkT < setting1){   // check setting 6
    SetSpeed("7");
      if (cloudconnected == true){
      Particle.publish("refresh", "true", PRIVATE);
      }
}
}

int SetSpeed(String command)
{
// cloud trigger for pool speed
//double realPower       = emon1.realPower;   
hour = Time.hour();

minute = Time.minute();
month = Time.month();
day = Time.day();
  if (command == "0") 
    {   
      analogWrite(pPumpSpdpwm, LOW);
      pumpSetting = 0;
      pumpSpeed = 0;
     // Particle.variable("PowerUse", String(realPower), PRIVATE);
      Particle.publish("Time", Time.timeStr());
      return 0;
    }
  if (command == "1")
   {
      analogWrite(pPumpSpdpwm, 860,100);
      pumpSetting = 1;
      pumpSpeed = 1250;
      //Particle.variable("PowerUse", String(realPower));
      //Particle.variable("PumpRPM", pumpSpeed);
      Particle.publish("Time", Time.timeStr());
      return 1;
   }
  if (command == "2")
   {
      analogWrite(pPumpSpdpwm, 1190,100);
      pumpSetting = 2;
      pumpSpeed = 1500;
      //Particle.variable("PowerUse", String(realPower));
     // Particle.variable("PumpRPM", pumpSpeed);
      Particle.publish("Time", Time.timeStr());
      //Particle.publish("OAT", String(ioat));
      return 2;
   }
   if (command == "3")
   {
      analogWrite(pPumpSpdpwm, 1520,100);
      pumpSetting = 3;
      pumpSpeed = 1750;
     //Particle.variable("PowerUse", String(realPower));
     // Particle.variable("PumpRPM", pumpSpeed);
      Particle.publish("Time", Time.timeStr());
     // Particle.publish("OAT", String(ioat));
      return 3;
   }
   if (command == "4")
   {
     analogWrite(pPumpSpdpwm, 1850,100);
      pumpSetting = 4;
      pumpSpeed = 2000;
      //Particle.variable("PowerUse", String(realPower));
      Particle.publish("Time", Time.timeStr());
      //Particle.publish("OAT", String(ioat));
       return 4;
   }
   if (command == "5")
   {
      analogWrite(pPumpSpdpwm, 2511,100);
      pumpSetting = 5;
      pumpSpeed = 2500;
      //Particle.variable("PowerUse", String(realPower));
      Particle.publish("Time", Time.timeStr());
      return 5;
   }
   if (command == "6")
   {
     analogWrite(pPumpSpdpwm, 3172,100);
      pumpSetting = 6;
      pumpSpeed = 3000;
    //  Particle.variable("PowerUse", String(realPower));
      Particle.publish("Time", Time.timeStr());
      return 6;
   }
   if (command == "7")
   {
     analogWrite(pPumpSpdpwm, 3972,100);
      pumpSetting = 7;
      pumpSpeed = 3450;
    //  Particle.variable("PowerUse", String(realPower));
      Particle.publish("Time", Time.timeStr());
      return 7;
   }
   else  // bad command shutoff pump return 9
    {               
     analogWrite(pPumpSpdpwm, 0,0);
      pumpSetting = 0;
      pumpSpeed = 0;
      
    // Particle.variable("PowerUse", String(realPower));
      Particle.publish("Time", Time.timeStr());
      //Particle.publish("OAT", String(ioat));
      return 9;
    }
}
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
