/* PC Based Time and Options Settings For Geiger Shield  (Ver 3.0)                    (9/7/12) 
 * TO USE: Load sketch, open Serial Monitor - 9600 baud NO LINE ENDING - answer + Enter
 * This version will optionally set the time, shield options, and run a set-up the Ultimate GPS
 * Note, changing the Time Zone will not affect the time shown here. It's only used with the GPS 
 */

#include <Wire.h> 
#include <EEPROM.h>                     // ATmega328's EEPROM used to store settings
#define RTC_ADDR 0x68
#define DDMMYY            1             // used with DATE_FMT below
#define YYMMDD            2             // used with DATE_FMT below
#define MMDDYY            3             // used with DATE_FMT below

#define DATE_FMT         MMDDYY         // pick date format from #defines above
#define AM_PM_FMT        1              // 0 if 24H / 1 if 12H format

// Shield EEPROM Address for menu inputs - common for RTC and GPS 
#define DISP_PERIOD_ADDR  0
#define DOSE_MODE_ADDR    2
#define LOG_PERIOD_ADDR   4
#define USV_RATIO_ADDR    6
#define ALARM_SET_ADDR    8
#define ZONE_ADDR         10

// Basic Kit EEPROM Address for menu inputs
#define _DISP_PERIOD_ADDR  0  // unsigned int - 2 bytes
#define _LOG_PERIOD_ADDR   2  // unsigned int - 2 bytes
#define _ALARM_SET_ADDR    6  // unsigned int - 2 bytes
#define _DOSE_UNIT_ADDR    8 // byte - 1 byte 
#define _ALARM_UNIT_ADDR   10 // boolean - 1 byte
#define _SCALER_PER_ADDR   12 // unsigned int - 2 bytes
#define _BARGRAPH_MAX_ADDR 14 // unsigned int - 2 bytes
#define _TONE_SENS_ADDR    16 // unsigned int - 2 bytes
#define _RADLOGGER_ADDR    44 // boolean - 1 byte
#define _PRI_RATIO_ADDR    60 // float - 4 bytes
#define _SEC_RATIO_ADDR    64 // float - 4 bytes

char timeString[10];                    // time format determined by #defines above
char dateString[14];                    // time format determined by #defines above
byte seconds,minutes,hours,day_of_week,days,months,years,DST,PM;
byte setMin,setHour,setDay,setMonth,setYear,setDOW;
char serInStr[6];                       // array that holds the serial input string
char tempChar;                          // used for Y/N questions
boolean setupShield;                    // if TRUE setup shield, if FALSE setup basic kit


void setup() {
  Serial.begin(9600);
  Wire.begin(); 

  Serial.println("NOTE: Set Serial Monitor to 9600 baud and ANY LINE ENDING\r\n");
  Serial.print("Setup Shield or GK-B5 Kit? (S = shield / G = GK-B5 kit) => ");
  tempChar = Serial_To_Int(serInStr);              // use just the string not the Int
  Serial.println(char(serInStr[0]));
  if (serInStr[0] == 's' || serInStr[0] == 'S') setupShield = true;
  else setupShield = false;

  if (setupShield) {                      // only display time if shield
    //Set_Square_Wave(0);                 // (if needed) 0=1Hz, 1=4KHz, 2=8KHz, 3=32KHz
    Get_Time(DATE_FMT);                   // read the formatted current date and time
    Serial.println("CURRENT DATE & TIME . . .");
    Serial.print(dateString);
    Serial.print(" ");
    Serial.print(timeString);
    if (AM_PM_FMT){                       // print AM/PM if defined
      if (PM) Serial.print("pm"); 
      else Serial.print("am");
    }
    Serial.println("");
    // if (DST) Serial.println("DST FLAG SET");
    // else Serial.println("DST FLAG IS NOT SET"); 
  }

  Serial.println("");
  Serial.println("CURRENT SETTINGS . . .");
  if (setupShield) Get_Settings(); // Display current settings from EEPROM for Shield
  else Get_SettingsGK_B5();        // Display current settings from EEPROM for Basic Kit
  Serial.println("");

  if (setupShield) {                      // only set time if shield
    Serial.print("Set Date/Time? (Y/N) => ");
    tempChar = Serial_To_Int(serInStr);              // use just the string not the Int
    Serial.println(char(serInStr[0]));
    if (serInStr[0] == 'y' || serInStr[0] == 'Y'){   // do the rest of this section

      Serial.print("Day Of Week (1=Mon) => ");
      setDOW = Serial_To_Int(serInStr);   // for the rest, get just the Int
      Serial.println(setDOW,DEC);

      Serial.print("Month => ");
      setMonth = Serial_To_Int(serInStr);
      Serial.println(setMonth,DEC);

      Serial.print("Date => ");
      setDay = Serial_To_Int(serInStr);
      Serial.println(setDay,DEC);

      Serial.print("Year (YY) => ");
      setYear = Serial_To_Int(serInStr);
      Serial.println(setYear,DEC);

      Serial.print("Hour (0-23) => ");
      setHour = Serial_To_Int(serInStr);
      Serial.println(setHour,DEC);

      Serial.print("Minutes (& set) => ");
      setMin = Serial_To_Int(serInStr);
      Serial.println(setMin,DEC);

      Set_Time(setMin,setHour,setDay,setMonth,setYear,setDOW); // do the deed!
    } 

    Get_Time(DATE_FMT);                   // read the date and time again
    DST_Flag();                           // Set flag in RTC NVRAM if DST
    Serial.println("");
  }

  Serial.print("Change Settings ? (Y/N) => ");
  Serial_To_Int(serInStr);              // use just the string not the Int
  Serial.println(char(serInStr[0]));
  if (serInStr[0] == 'y' || serInStr[0] == 'Y'){
    Serial.println("");

    if (setupShield) {                      // set these parames if shield
      Serial.print("SEC DISP PERIOD (5 sec recommended) => ");
      writeParam(Serial_To_Int(serInStr) * 1000, DISP_PERIOD_ADDR);
      Serial.println(readParam(DISP_PERIOD_ADDR) / 1000,DEC);

      Serial.print("DOSE MODE (1=ON, 0=OFF) => ");
      if(Serial_To_Int(serInStr)) writeParam(true, DOSE_MODE_ADDR);
      else writeParam(false, DOSE_MODE_ADDR);
      Serial.println(readParam(DOSE_MODE_ADDR),DEC);

      Serial.print("LOGGING PERIOD (sec) => ");
      writeParam(Serial_To_Int(serInStr), LOG_PERIOD_ADDR);
      Serial.println(readParam(LOG_PERIOD_ADDR),DEC);

      Serial.print("CPM TO uSv RATIO => ");
      writeParam(Serial_To_Int(serInStr), USV_RATIO_ADDR);
      Serial.println(readParam(USV_RATIO_ADDR),DEC);

      Serial.print("ALARM THRESHOLD (CPM) => ");
      writeParam(Serial_To_Int(serInStr), ALARM_SET_ADDR);
      Serial.println(readParam(ALARM_SET_ADDR),DEC);

      Serial.print("TIME ZONE (For GPS, add 12 for + zones) => ");
      writeParam(Serial_To_Int(serInStr), ZONE_ADDR);
      Serial.println(readParam(ZONE_ADDR),DEC);

      Serial.println("");
      Serial.println("NEW SETTINGS . . .");
      Get_Settings(); // Display current settings from EEPROM
      Serial.println("");
    }

    else {                      // Set Params for BASIC KIT
      Serial.print("SEC DISP PERIOD (5 sec recommended) => ");
      writeParam(Serial_To_Int(serInStr) * 1000, _DISP_PERIOD_ADDR);
      Serial.println(readParam(_DISP_PERIOD_ADDR) / 1000,DEC);

      Serial.print("LOGGING PERIOD (sec) => ");
      writeParam(Serial_To_Int(serInStr), _LOG_PERIOD_ADDR);
      Serial.println(readParam(_LOG_PERIOD_ADDR),DEC);

      Serial.print("ALARM THRESHOLD => ");
      writeParam(Serial_To_Int(serInStr),_ALARM_SET_ADDR);
      Serial.println(readParam(_ALARM_SET_ADDR),DEC);

      Serial.print("DOSE UNIT (0= uSv/h, 1= uR/h, 2= mR/h) => ");
      writeParam(Serial_To_Int(serInStr),_DOSE_UNIT_ADDR);
      tempChar = readParam(_DOSE_UNIT_ADDR);
      if (tempChar == 0) Serial.println("uSv/h");
      if (tempChar == 1) Serial.println("uR/h");
      if (tempChar == 2) Serial.println("mR/h");
      if (tempChar >2) Serial.println("Bad Data!");

      Serial.print("ALARM UNIT (1= CPM, 0= Dose unit) => ");
      writeParam(Serial_To_Int(serInStr),_ALARM_UNIT_ADDR);
      tempChar = readParam(_ALARM_UNIT_ADDR);
      if (tempChar == 1) Serial.println("CPM");
      else Serial.println("Dose unit");

      Serial.print("SCALER PERIOD => ");
      writeParam(Serial_To_Int(serInStr),_SCALER_PER_ADDR);
      Serial.println(readParam(_SCALER_PER_ADDR),DEC);

      Serial.print("BARGRAPH MAX => ");
      writeParam(Serial_To_Int(serInStr),_BARGRAPH_MAX_ADDR);
      Serial.println(readParam(_BARGRAPH_MAX_ADDR),DEC);

      Serial.print("TONE SENSITIVITY => ");
      writeParam(Serial_To_Int(serInStr),_TONE_SENS_ADDR);
      Serial.println(readParam(_TONE_SENS_ADDR),DEC);

      Serial.print("RADLOGGER ON (1=ON, 0=OFF) => ");
      if(Serial_To_Int(serInStr)) writeParam(true, _RADLOGGER_ADDR);
      else writeParam(false, _RADLOGGER_ADDR);
      if (readParam(_RADLOGGER_ADDR)== 0) Serial.println("OFF");
      else Serial.println("ON");

      Serial.print("PRIMARY CPM TO DOSE RATIO (no dec) => ");
      writeFloatParam(Serial_To_Int(serInStr),_PRI_RATIO_ADDR);
      Serial.println(readFloatParam(_PRI_RATIO_ADDR),DEC);

      Serial.print("SECONDARY CPM TO DOSE RATIO (no dec) => ");
      writeFloatParam(Serial_To_Int(serInStr),_SEC_RATIO_ADDR);
      Serial.println(readFloatParam(_SEC_RATIO_ADDR),DEC);

      Serial.println("");
      Serial.println("NEW SETTINGS . . .");
      Get_SettingsGK_B5(); // Display current settings from EEPROM
      Serial.println("");
    }
  }
}



void loop(){
  if (setupShield) {                      // show running time if shield
    Get_Time(DATE_FMT);                   // just loop displaying time
    Serial.print(dateString);
    Serial.print(" ");
    Serial.print(timeString);
    if (AM_PM_FMT){                       // print AM/PM if defined
      if (PM) Serial.print("pm"); 
      else Serial.print("am");
    }
    Serial.println("");
    delay(5000);
  }
  else {
    Serial.print("DONE!");
    while (1);
  }
}

unsigned int Serial_To_Int(char *str){ 
  // FOR SERIAL MONITORS WHERE ALL INPUT IS AT ONCE. (Like the IDE)
  // Now workes with all line endings - none, CR, LF, CR/LF
  // Read from serial and passs back string & and also return as an integer
  // Returns 0 if non-numerc input but string keeps chars.
  byte idx = 0;

  while(!Serial.available());             // wait for input
  while (Serial.available()){             // have input
    str[idx] = Serial.read();             // build string
    delay(10);                            // MUST have this!
    if (str[idx] == '\r' || str[idx] == '\n') break; // don't add to string
    idx++;
  }
  str[idx] = '\0';                        // indicate end of read string
// flush serial buffer in case of CR/LF - Serial.flush() no longer does this.
  while(Serial.available() > 0) char t = Serial.read();
  return atoi(str);                       // return the integer - 0 if none in string
}

///////////////////////////// DS1307 RTC Functions ////////////////////////////////

void Get_Time(byte format){  // get the time and date from RTC lib and format it
  byte hour12;                          // local vars

  memset(timeString,0,sizeof(timeString));  // initialize the strings
  memset(dateString,0,sizeof(dateString));
  Wire.beginTransmission(0x68); 
  Wire.write(0); 
  Wire.endTransmission(); 
  Wire.requestFrom(0x68, 7);            // request 7 bytes from DS1307
  seconds = bcdToDec(Wire.read());      
  minutes = bcdToDec(Wire.read());      
  hours = bcdToDec(Wire.read());        
  day_of_week = Wire.read();    
  days = bcdToDec(Wire.read());          
  months = bcdToDec(Wire.read());      
  years = bcdToDec(Wire.read());  

  // deal with AM/PM global and 12 hour clock no matter what format
  if (hours >= 12) PM = true;
  else PM = false;
  if (hours > 12)hour12 = hours - 12;
  else hour12 = hours;
  if (hours == 0) hour12 = 12;

  // make time string
  if (AM_PM_FMT)AppendToString (hour12,timeString);        // add 12 hour time to string
  else AppendToString (hours,timeString);                  // add 24 hour time to string
  strcat(timeString,":");
  if (minutes < 10) strcat(timeString,"0");
  AppendToString (minutes,timeString);                     // add MINUTES to string
  strcat(timeString,":");
  if (seconds < 10) strcat(timeString,"0");
  AppendToString (seconds,timeString);                     // add SECONDS to string

    // make date string
  if (format == DDMMYY)AppendToString (days,dateString);          // add DAY to string
  else if(format == MMDDYY)  AppendToString (months,dateString);  // add MONTH to string  
  else {                                                          // add YEAR to string
    if (years < 10) strcat(dateString,"0");
    AppendToString (years,dateString);
  }  
  strcat(dateString,"/");
  if (format == MMDDYY)AppendToString (days,dateString);          // add DAY to string
  else AppendToString (months,dateString);                        // add MONTH to string  
  strcat(dateString,"/");
  if (format == YYMMDD)AppendToString (days,dateString);          // add DAY to string
  else {
    if (years < 10) strcat(dateString,"0");
    AppendToString (years,dateString);                            // add YEAR to string
  }
  if (Read_NVRAM(0)== 0) DST = 0;
  else DST = 1; 
}


void Set_Time(byte Min,byte Hour,byte Day,byte Month,byte Year, byte DOW){
  Hour = ((Hour/10)*16)+(Hour % 10);    // convert each arg from decimal to BCD
  Min = ((Min/10)*16)+(Min % 10);
  Day = ((Day/10)*16)+(Day % 10);
  Month = ((Month/10)*16)+(Month % 10);
  Year = ((Year/10)*16)+(Year % 10);

  Wire.beginTransmission(0x68);         // slave address of DS1307 is 68H
  Wire.write(0);                        // begin register address 00H
  Wire.write(0x00);  // defult to 0 seconds
  Wire.write(Min);   // set minutes
  Wire.write(Hour);  // set hour (24 hour clock)
  Wire.write(DOW);   // set day of week (can use for "day 1", "day 2", etc.
  Wire.write(Day);   // set date
  Wire.write(Month); // set month
  Wire.write(Year);  // set year
  Wire.endTransmission(); 
}


// This function is not used in this sketch
void DST_Check(){ // Automatically changes the time based on DST

  // check if entering DST (2nd Sun in Mar.) . . .
  if (months == 3 && day_of_week == 7 && days > 7 && days <= 14 && hours >= 2) {
    if (!Read_NVRAM(0)){                // if not currently in DST 
      Adj_Hour(1);                      // set the clock ahead 1 hour
      Write_NVRAM(0,1);                 // set the DST flag in EEPROM
    }
  }
  // check if leaving DST (1st Sun in Nov.) . . .
  if (months == 11 && day_of_week == 7 && days <= 7 && hours >= 2) {
    if (Read_NVRAM(0)){                 // if currently in DST 
      Adj_Hour(-1);                     // set the clock back 1 hour
      Write_NVRAM(0,0);                 // clear the DST flag in EEPROM
    }
  }
}


void DST_Flag(){  // Sets the DST Flag when setting date / time
  // Does not allow for changing time / date on the days where DST changes
  byte DST_Set = false;                 // set if DST is set in RTC EEPROM
  byte In_DST = false;                  // assume not in DST until proven otherwise
  byte DOWinDST;                        // in DST based on 2nd Sun in Mar. or 1st in Nov.
  char nextSun;                         // the date of the next Sunday

  if (Read_NVRAM(0)) DST_Set = true ;   // see if DST set in RTC EEPROM

  // Prequalify for in DST in the widest range (any date between 3/8 and 11/7) 
  if ((months == 3 && days >= 8) || (months > 3 && months < 11) || (months == 11 && days <= 7) ){
    // DST start could be as late as 3/14 and end as early as 11/1
    DOWinDST = true;                    // assume it's in DST until proven otherwise
    nextSun = days + (7 - day_of_week); // find the date of the next Sunday
    if (nextSun == days) nextSun += 7;  // take care of today being Sunday
    if (months == 3 && (days <= nextSun)) DOWinDST = false;     // it's before the 2nd Sun in March
    if (months == 11 && (days >= nextSun -7)) DOWinDST = false; // it's after the 1st Sun in Nov.
    if (DOWinDST){                      // DOW is OK so it's in DST
      In_DST = true;                    // set flag so not in DST can be tested
      Write_NVRAM(0,1);                 // set the DST flag in EEPROM
    }
  }
  // Assumed not in DST, clear DST flag . . .
  if (In_DST == false) {
    Write_NVRAM(0,0);                   // clear the DST flag in EEPROM
  }
}


// This function is not used in this sketch
void Adj_Hour (int UpDown){
  Wire.beginTransmission(0x68);         // slave address of DS1307 is 68H
  Wire.write(0x02);                     // Hours register address is 2H
  Wire.write(hours + UpDown);           // adjust Hours up or down
  Wire.endTransmission(); 
}


void AppendToString (byte bValue, char *pString){ // appends a byte to string passed
  char tempStr[6];
  memset(tempStr,'\0',sizeof(tempStr));
  itoa(bValue,tempStr,10);
  strcat(pString,tempStr);
}


byte bcdToDec(byte val) {               // Convert binary coded decimal to normal decimal numbers
  return ((val / 16 * 10) + (val % 16));
}

// This function is not used in this sketch
void Write_NVRAM(int write_slot, int data){// write to the NV RAM on the DS1307 chip                       
  // 56 one Byte slots available. Battery must be connected to retain data
  if (write_slot > 55) write_slot = 0;  // stop from looping back and writing clock data
  Wire.beginTransmission(0x68);         // slave address of DS1307 is 68H
  Wire.write(8+write_slot);             // RAM is from 08H to 3FH - offset slot #
  Wire.write(data);                     // write data to that slot
  Wire.endTransmission(); 
}


int Read_NVRAM(int read_slot){ // raed from NV RAM on the DS1307 chip                       
  int rdata;                            // declare the return
  Wire.beginTransmission(0x68);         // slave address of DS1307 is 68H
  Wire.write(8+read_slot);              // set the pointer to the slot #
  Wire.endTransmission();               // done setting pointer
  Wire.requestFrom(0x68, 1);            // request 1 bytes from DS1307 at the pointer
  if (Wire.available()) rdata = Wire.read();
  return rdata;
}


// This function is not required in this sketch
void Set_Square_Wave(int rate){  // set the square wave output on pin 7 of the DS1307 chip
  // NOTE: Open collector - an external pullup resistor must be connected to this pin.
  rate = rate + 144;                    // add 0x90 (dec 144) to rate
  Wire.beginTransmission(0x68);         // write the control register
  Wire.write(0x07);                     // register address 07H)
  Wire.write(rate);                     // 0=1Hz, 1=4KHz, 2=8KHz, 3=32KHz
  Wire.endTransmission();
} 

void Get_Settings(){ // Get settings for SHIELD
  long EEPROM_Read;

  EEPROM_Read = readParam(DISP_PERIOD_ADDR);
  Serial.print("SEC DISPLY PERIOD = ");
  Serial.println(EEPROM_Read / 1000,DEC);

  EEPROM_Read = readParam(DOSE_MODE_ADDR);
  Serial.print("DOSE MODE = ");
  Serial.println(EEPROM_Read,DEC);

  EEPROM_Read = readParam(LOG_PERIOD_ADDR);
  Serial.print("LOGGING PERIOD (sec) = ");
  Serial.println(EEPROM_Read,DEC);

  EEPROM_Read = readParam(USV_RATIO_ADDR);
  Serial.print("CPM TO uSv RATIO = ");
  Serial.println(EEPROM_Read,DEC);

  EEPROM_Read = readParam(ALARM_SET_ADDR);
  Serial.print("ALARM THRESHOLD (CPM) = ");
  Serial.println(EEPROM_Read,DEC);

  EEPROM_Read = readParam(ZONE_ADDR);
  Serial.print("TIME ZONE (GPS) = ");
  int GMT_Offset = EEPROM_Read;
  if (GMT_Offset >12) GMT_Offset = (GMT_Offset - 12);
  else GMT_Offset = GMT_Offset * -1;
  Serial.println(GMT_Offset,DEC);
}


void Get_SettingsGK_B5(){ // Get settings for BASIC KIT
  long EEPROM_Read;

  EEPROM_Read = readParam(_DISP_PERIOD_ADDR);
  Serial.print("SEC DISPLY PERIOD = ");
  Serial.println(EEPROM_Read / 1000,DEC);

  EEPROM_Read = readParam(_LOG_PERIOD_ADDR);
  Serial.print("LOGGING PERIOD (sec) = ");
  Serial.println(EEPROM_Read,DEC);


  EEPROM_Read = readParam(_ALARM_SET_ADDR);
  Serial.print("ALARM THRESHOLD = ");
  Serial.println(EEPROM_Read,DEC);

  EEPROM_Read = readParam(_DOSE_UNIT_ADDR);
  Serial.print("DOSE UNIT = ");
  if (EEPROM_Read == 0) Serial.println("uSv/h");
  if (EEPROM_Read == 1) Serial.println("uR/h");
  if (EEPROM_Read == 2) Serial.println("mR/h");
  if (EEPROM_Read >2) Serial.println("Bad Data!");

  EEPROM_Read = readParam(_ALARM_UNIT_ADDR);
  Serial.print("ALARM UNIT = ");
  if (EEPROM_Read == 1) Serial.println("CPM");
  else Serial.println("Dose unit");

  EEPROM_Read = readParam(_SCALER_PER_ADDR);
  Serial.print("SCALER PERIOD = ");
  Serial.println(EEPROM_Read,DEC);

  EEPROM_Read = readParam(_BARGRAPH_MAX_ADDR);
  Serial.print("BARGRAPH MAX = ");
  Serial.println(EEPROM_Read,DEC);

  EEPROM_Read = readParam(_TONE_SENS_ADDR);
  Serial.print("TONE SENSITIVITY = ");
  Serial.println(EEPROM_Read,DEC);

  EEPROM_Read = readParam(_RADLOGGER_ADDR);
  Serial.print("RADLOGGER ON? = ");
  if (EEPROM_Read == 0) Serial.println("OFF");
  else Serial.println("ON");

  EEPROM_Read = readFloatParam(_PRI_RATIO_ADDR); // float
  Serial.print("PRIMARY CPM TO DOSE RATIO = ");
  Serial.println(EEPROM_Read,DEC);

  EEPROM_Read = readFloatParam(_SEC_RATIO_ADDR); // float
  Serial.print("SECONDARY CPM TO DOSE RATIO = ");
  Serial.println(EEPROM_Read,DEC);
}


void writeParam(unsigned int value, unsigned int addr){ // Write menu entries to EEPROM
  unsigned int a = value/256;
  unsigned int b = value % 256;
  EEPROM.write(addr,a);
  EEPROM.write(addr+1,b);
}


unsigned int readParam(unsigned int addr){ // Read previous menu entries from EEPROM
  unsigned int a=EEPROM.read(addr);
  unsigned int b=EEPROM.read(addr+1);
  return a*256+b; 
}


//////////////////////////////////////////////////////////////////////////
void writeFloatParam(float value, unsigned int addr) {
  const byte* p = (const byte*)(const void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(addr++, *p++);
  return;
}

static float readFloatParam(unsigned int addr) {
  float value;
  byte* p = (byte*)(void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(addr++);
  return value;
}














