// This #include statement was automatically added by the Particle IDE.
#include <EmonLib.h>



//This is bscuderi13's own pool controller and does not incorporate a variable speed pump,
//but I will upgrade to that later. Instead, I will comment out any mention of variable speeds.
//This version uses PWM to control an Adafruit DRV8871 Motor controller, which in turn drives a Century Pump Vgreen 1.65 pump.
//5-97% duty cycle at 100 Hz corresponds to 600-3450 RPM.
//I have also included an Emon power monitor, to better be able to monitor power consumption accurately.
//So far, power, voltage and current are working well.
//******Temperature Monitoring and Control******


// This #include statement was automatically added by the Particle IDE.

#include <OneWire.h>        //For Temp Sensor


EnergyMonitor emon1;             // Create an instance


STARTUP(WiFi.selectAntenna(ANT_EXTERNAL)); // Use external antenna
SYSTEM_MODE(SEMI_AUTOMATIC);    // take control of the wifi behavior to avoid cloud bogging down local functions
SYSTEM_THREAD(ENABLED);    // enable so loop and setup run immediately as well as allows wait for to work properly for the timeout of wifi



// pins D0 and D1 cannot be used for relay control due to being the I2C comm pins
//When I add pH, ORP, and conductivity, they are I2C devices and may be 
//piggybacked onto the PSCREW screw terminals to communicate with, which uses D0 and D1.
//Direct Photon Relay control
int pPumpSpdpwm  = RX;
//int pPumpSpeedLo = D5; Since this is PWM, low can be connected to ground with the Adafruit DRV8871
int pCleaner     = D4;


#define cleaneronrelay  D2
#define cleaneroffrelay D3



//Temperature pins
int tempPower  = A2;
int tempGround = A3;


// Pump Speed Variables
int pumpSetting = 0;
int pumpSpeed = 0;
int pumpPowerConsumption = 0;
int totalKWH = 0;

unsigned long lastRequest  = 0;

//** temp variables
OneWire ds = OneWire(A4);  //** 1-wire signal on pin A4
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
    emon1.voltage(3, 184.986, 1);  // Voltage: input pin, calibration, phase_shift
    emon1.current(1, 17.90);       // Current: input pin, calibration.


// Set Pump Pins to Outputs
    pinMode(pPumpSpdpwm, OUTPUT);     // sets the pin as output
        analogWriteResolution(pPumpSpdpwm, 12); // sets analogWrite resolution to 12 bits
    //pinMode(pPumpRelay2, OUTPUT); 
    //pinMode(pPumpRelay3, OUTPUT); 
    
// Register Cloud Functions for Pump
    Particle.function("PumpSpeed", SetSpeed);
    Particle.function("automode", automodefunc);
    Particle.variable("PumpRPM", pumpSpeed);
    Particle.variable("PowerUse", realPower);
    Particle.variable("TotalKWH", totalKWH);
    Particle.variable("RSSI", rssi);
// Register Cloud Functions for valves
 //   Particle.function("aerator", aeratorfunc);
 //   Particle.function("floorcleaner", cleanerfunc);
// subscribe to and publish temp & levelchanges
 //   Particle.subscribe("CurrentTemperature", tempHandler, MY_DEVICES);
 //   Particle.subscribe("waterlevel", levelHandler, MY_DEVICES);
 //   Particle.variable("PoolTemp", poolTemp);
 //   Particle.variable("PoolLevel", poolLevel);
//** register cloud functions for temp  
    Particle.variable("OAT", ioat);

//** temp pin setup
    pinMode(tempPower, OUTPUT); //** power temp sensor
    pinMode(tempGround, OUTPUT); //** ground temp sensor
    digitalWrite(tempPower, HIGH);
    digitalWrite(tempGround, LOW);

// start valve position at cleaner on so easier to track  
//  cleanerOn(); // go on first to hit valve limit switch
//  cleanerOff(); // then go back to default to off
  //aeratorOff(); // start default aerator off
// let smartthings know to update valve position
}

// get a temp reading

void loop() {
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
   
   
   
   Particle.publish("RMS Volts", String(supplyVolts));  // This is what exposes the funtion to the internet!
   Particle.publish("RMS Current", String(Irms));  // NOTE: Variable names can only be a max of twelve characters!
   Particle.publish("Real Power", String(realPower));
   Particle.publish("App Power", String(apparentPwr));
   Particle.publish("Pwr Factor", String(pwerFactor));
    
   delay(10000);
}
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
  Particle.publish("OAT", oat, PRIVATE);
  ioat = atoi(oat);
  tLast = Time.local() % 86400;
}
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
   if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}

// 8 am 1500 rpm main returns setting 3

if (hour == 8 && minute == 0 && minutechange == true && automode == true){
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
    SetSpeed("2");
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}

//  11:30 pm full speed and floor cleaners setting 6

if (hour == 23 && minute == 30 && minutechange == true && automode == true){
    SetSpeed("7");
    if (cloudconnected == true){
    Particle.publish("refresh", "true", PRIVATE);
    }
}

//  1:00 am 1500 rpm or 1750 and aerator depending on pool temp setting 1

if (hour == 1 && minute == 0 && minutechange == true && automode == true){
      SetSpeed("2");
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
 double realPower       = emon1.realPower;   
hour = Time.hour();
minute = Time.minute();
month = Time.month();
day = Time.day();
  if (command == "0") 
    {   
      analogWrite(pPumpSpdpwm, LOW);
      pumpSetting = 0;
      pumpSpeed = 0;
      Particle.publish("Watts", String(realPower));
      Particle.publish("Time", Time.timeStr());
      return 0;
    }
  if (command == "1")
   {
      analogWrite(pPumpSpdpwm, 860,0);
      pumpSetting = 1;
      pumpSpeed = 1250;
      Particle.publish("Watts", String(realPower));
      Particle.publish("Time", Time.timeStr());
      return 1;
   }
  if (command == "2")
   {
      analogWrite(pPumpSpdpwm, 1190,100);
      pumpSetting = 2;
      pumpSpeed = 1500;
      Particle.publish("Watts", String(realPower));
      Particle.publish("Time", Time.timeStr());
      return 2;
   }
   if (command == "3")
   {
      analogWrite(pPumpSpdpwm, 1520,100);
      pumpSetting = 3;
      pumpSpeed = 1750;
      Particle.publish("Watts", String(realPower));
      Particle.publish("Time", Time.timeStr());
      return 3;
   }
   if (command == "4")
   {
     analogWrite(pPumpSpdpwm, 1850,100);
      pumpSetting = 4;
      pumpSpeed = 2000;
      Particle.publish("Watts", String(realPower));
      Particle.publish("Time", Time.timeStr());
       return 4;
   }
   if (command == "5")
   {
      analogWrite(pPumpSpdpwm, 2511,100);
      pumpSetting = 5;
      pumpSpeed = 2500;
      Particle.publish("Watts", String(realPower));
      Particle.publish("Time", Time.timeStr());
      return 5;
   }
   if (command == "6")
   {
     analogWrite(pPumpSpdpwm, 3172,100);
      pumpSetting = 6;
      pumpSpeed = 3000;
      Particle.publish("Watts", String(realPower));
      Particle.publish("Time", Time.timeStr());
      return 6;
   }
   if (command == "7")
   {
     analogWrite(pPumpSpdpwm, 3972,100);
      pumpSetting = 7;
      pumpSpeed = 3450;
      Particle.publish("Watts", String(realPower));
      Particle.publish("Time", Time.timeStr());
      return 7;
   }
   else  // bad command shutoff pump return 9
    {               
     analogWrite(pPumpSpdpwm, 0,0);
      pumpSetting = 0;
      pumpSpeed = 0;
      Particle.publish("Watts", String(realPower));
      Particle.publish("Time", Time.timeStr());
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





