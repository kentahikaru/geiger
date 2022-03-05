/* GK to Radmon.org & Xively & Sparkfun via WIZnet or ENC28J60                  bHogan 7/15/14
 *
 * DESCRIPTION:
 * Reads serial output from Geiger Kit FTDI port - sends to Radmon.org and/or Xively
 * and/or Sparkfun. Supports either the WIZnet (suggested) OR the ENC28J60 ethernet modules.
 * For details refer to - https://dl.dropboxusercontent.com/u/3572198/GK%20Net%20v2/GKnet%20Build%20Instructions%20v2.pdf
 * Thanks to a great lib - UIPEthernet, by Norbert Truchsess switching from WIZnet to ENC28J60
 * simply means replacing the Ethernet lib with his UIPEthernet lib. (Due to the sorry state of
 * the pre-compiler in the Arduino IDE you must comment out the appropriate lib.)
 *
 * WHAT IS NEW in V4.0:
 * - Added support for https://data.sparkfun.com
 * - Posting interval is now controlled by the Geiger kit. Use the setup menu to set the Log Period.
 *   When new data arrives from the kit it is sent by the GKnet board.
 * - Added Ethernet.maintain(). This allows for the renewal of DHCP leases. May improve stability.
 * - Reworked TEST_COUNTS - it now has a test posting interval.
 * - support for static IP (uses more memory)
 * - cleaned up use of client.stop() and client.flush
 * - v4.0b added flush to data.sparkfun.com
 * - v4.0c saved 90 bytes on Sparkfun posting
 * - v4.0c comments under sendToPhant()about using all lowercase field names for Sparkfun phant.
 * - v4.0c delay added between flush and close - reported to improve reliability
 *
 * SETUP: (only needed if GKnet board NOT used)
 * CONNECTIONS FROM ARDUINO to WIZNET BOARD: (Remember: WIZnet is a 3.3V device!)
 *  D10 to J2-4 (/SCS)          D11 to J1-1 (MOSI)         D12 to J1-2 (MISO)
 *  D13 to J2-3 (SCLK)          RESET to J2-2 (/RESET)
 *  3.3V pin to J2-1 (3.3V)     GND to J2-9 (GND)
 * CONNECTIONS FROM ARDUINO to GEIGER KIT:
 * D1 (Rx) to FTDI TXD          (D2 (Tx) to FTDI RXD is not required)
 * GND to FTDI - (or any GND)   5V pin to FTDI + (or any 5V)
 *
 * NOTES:
 * - Requires GK-B5 software version 10.1 or higher or GK-Plus
 * - Make sure you have USE RADLOGGER turned off.
 * - Note: Tx and Rx are reversed on FDTI socket on GKnet board. Making a crossover cable
 *   for programming is suggested if you want to do development. Then you can reprogram and
 *   even run the GKnet with TEST_COUNTS without having the Geiger Kit attached.
 * - LEDs: RED=  no network connection
 * - YEL= steady = 0 CPM (no GK connection?) / flashes when reveiving data from Geiger counter
 * - GRN= flashes when sending
 *
 * THIS PROGRAM AND IT'S MEASUREMENTS IS NOT INTENDED TO GUIDE ACTIONS TO TAKE, OR NOT
 * TO TAKE, REGARDING EXPOSURE TO RADIATION. THE GEIGER KIT AND IT'S SOFTWARE ARE FOR
 * EDUCATIONAL PURPOSES ONLY. DO NOT RELY ON THEM IN HAZARDOUS SITUATIONS!
 *
 * TODO:
 * - Don't need to convert to string for data.sparkfun - sending a float should work.
 */

//----------------------------------------------------------------------------------------------+
//                              User setup #defines
//----------------------------------------------------------------------------------------------+
// First select the lib for the ethernet module that you are using by commenting out one of
// the two libs below.

//#include <Ethernet.h>                 // Use this if WIZnet ethernet module
#include <UIPEthernet.h>                // Use this if ENC28J60 ethernet module

// Define who you want to send to. (If ENC28J60 used only 2 choices can be true)
#define SEND_TO_RADMON   true           // CPM will be sent to Radmon.org
#define SEND_TO_XIVELY   false          // CPM, Dose, Vcc and MAX CPM will be sent to Xively
#define SEND_TO_PHANT    false           // CPM, Dose, Vcc and MAX CPM will be sent to Sparkfun

// Add your credintials for the site(s) you will connect to.
#if (SEND_TO_RADMON)
// For Radmon.org: set the next 2 lines to your Radmon.org UserName and PassWord . . .
char UserName[] = "sandi";
char PassWord[] = "R4di4ti0n";
char radmonSite[] = "radmon.org";       // no need to change this
#endif

#if (SEND_TO_XIVELY)
// For Xively: set the next 2 lines to your Xively FEED ID and Device Key . . .
#define FEED_ID       ???????         // FEED ID FROM XIVELY - ABOUT 9-10 DIGITS 
#define KEY   "?????????????????????" // BIG STRING GOES HERE
const char xivelySite[] = "api.xively.com";   // URL of Xively
#endif

#if (SEND_TO_PHANT)
const String publicKey = "pwvmN7vmQYf62amLMAj6";
const String privateKey = "64AG1nAGj6Hz0AWNmJgz";
char server[] = "data.sparkfun.com";    // name address for data.sparkFun (using DNS)
#endif

// Other user settings . . .
#define STATIC_IP        false          // false to use DHCP (suggested) - true for static IP (uses more memory)
#define MAX_CPM_PERIOD   86400 //=24hr  // secs before new maxCPM period rolls over to begin again
#define TEST_COUNTS      false          // generates test counts 20-100 with each post
#define TEST_POST_PERIOD 30             // secs between posting if TEST_COUNTS is true
#define DEBUG            false          // serial output of status (works with TEST_COUNTS)

#if (STATIC_IP)
IPAddress ip(192, 168, 0, 16);          // Your static IP address to use if not relying on DHCP
#endif

//----------------------------------------------------------------------------------------------+
//                       End user setup #defines
//----------------------------------------------------------------------------------------------+

#define GRN_LED          5              // flashes with each send
#define YEL_LED          6              // no connection to Geiger Kit
#define RED_LED          7              // no connection to network

#define CPM              0              // index names for f_values
#define DOSE             1
#define VCC              2

#include <SPI.h>                        // needed if Ethernet.h but no cost if UIPEthernet.h 


// global variables . . .
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};  // MAC address for Ethernet shield - these numbers OK
float f_value[3];                       // floats for CPM, Dose, Vcc
boolean lastConnected = false;          // state of the connection last time through loop
boolean newData = false;                // true when new data arrives from Geiger Kit
unsigned long lastConnectionTime = 0;   // mS from last server connection
unsigned int maxCPM = 0;                // saves the maxium CPM from each 24 hours after startup
unsigned long lastmaxCpmTime = 0;       // mS from last maxCPM period
EthernetClient client;

// need to protype these - else compiler errors due to #define
void sendToRadmon();
void sendToXively();
void sendToPhant();
int AvailRam();
////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(9600);
  pinMode(RED_LED, OUTPUT);             // setup LED pin
  pinMode(YEL_LED, OUTPUT);             // setup LED pin
  pinMode(GRN_LED, OUTPUT);             // setup LED pin
  delay(1000);                          // give the ethernet module time to boot up

  digitalWrite(RED_LED, HIGH);          // red until Ethernet.begin = 1

#if (STATIC_IP)
  Ethernet.begin(mac, ip);
#else // USE DHCP
  while (Ethernet.begin(mac) != 1) {
#if (DEBUG)
    Serial.println("DHCP Error - retry in 15 sec");
#endif
    client.stop();
    delay(15000);                       // wait 15 seconds and try Ethernet.begin again
  }
#endif // STATIC_IP
#if (DEBUG)
  Serial.println("Start");
  Serial.print("RAM Avail: ");
  Serial.println(AvailRam());
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
  Serial.println();
#endif

  digitalWrite(YEL_LED, HIGH);          // lamp test . . .
  digitalWrite(GRN_LED, HIGH);
  delay (1000);
  digitalWrite(RED_LED, LOW);
  digitalWrite(YEL_LED, LOW);
  digitalWrite(GRN_LED, LOW);
#if (TEST_COUNTS)
  f_value[CPM] = 20;                    // DEBUG for simulating counts
#endif
}


void loop() {
#if (TEST_COUNTS)
  if (millis() - lastConnectionTime > (TEST_POST_PERIOD * 1000L)) {
    f_value[CPM]++;                            // DEBUG for simulating counts
    if (f_value[CPM] > 100) f_value[CPM] = 20; // DEBUG for simulating counts
    newData = true;                            // make it think data has arrived
    f_value[DOSE] = f_value[CPM] / 100;
    f_value[VCC] = 4.95;
    digitalWrite(YEL_LED, HIGH);
    delay(500);
  }
#endif

  Ethernet.maintain();                  // This allows for the renewal of DHCP leases.
  if (f_value[CPM] == 0) {
    digitalWrite(YEL_LED, HIGH);        // nothing from Geiger Kit (yet)
    delay(1000);                        // Fixes (startup) problem of sending 0 CPM counts
    return;                             // loop() will be called again in a moment...
  }
  else {
    digitalWrite(YEL_LED, LOW);
  }

  if (!client.connected() && lastConnected) {   // if no net but there was in the last loop, stop
    client.stop();
  }

  // Here we go! If not connected, and new data arrived, connect and send data:
  if (!client.connected() && newData) {
    newData = false;                           // reset for next transmission

#if (SEND_TO_RADMON)
    //Serial.println("sending to Radmon");
    sendToRadmon();                     // call the function that sends to Radmon.org
#endif

#if (SEND_TO_XIVELY)
    //Serial.println("sending to Xively");
    sendToXively();                     // call the function that sends to Xively
#endif

#if (SEND_TO_PHANT)
    //Serial.println("sending to Sparkfun");
    sendToPhant();                     // call the function that sends to Xively
#endif
  }

  lastConnected = client.connected();   // store connection state for next time through the loop:

  if (millis() - lastmaxCpmTime > MAX_CPM_PERIOD * 1000L) { // For MAX CPM stuff
    maxCPM = 0;                         // reset this
    lastmaxCpmTime = millis();          // reset this
  }
}

//----------------------------------------------------------------------------------------------+
//                              The three send functions . . .
//----------------------------------------------------------------------------------------------+

#if (SEND_TO_RADMON)
void sendToRadmon() {
   Serial.print("RAM Avail1: ");
  Serial.println(AvailRam());
  if (client.connect(radmonSite, 80)) {
     Serial.print("RAM Avail2: ");
  Serial.println(AvailRam());
    digitalWrite(GRN_LED, HIGH);        // signal you're transmitting and send this stuff
    client.print("GET /radmon.php?function=submit&user=");
    client.print(UserName);
    client.print("&password=");
    client.print(PassWord);
    client.print("&value=");
    client.print(int(f_value[CPM]));
    client.print("&unit=CPM");
    client.println(" HTTP/1.0");
    client.print("HOST: ");
    client.println(radmonSite);
    client.println();
#if (DEBUG)
    Serial.print("GET /radmon.php?function=submit&user=");
    Serial.print(UserName);
    Serial.print("&password=");
    Serial.print(PassWord);
    Serial.print("&value=");
    Serial.print(int(f_value[CPM]));
    Serial.print("&unit=CPM");
    Serial.println(" HTTP/1.0");
    Serial.print("HOST: ");
    Serial.println(radmonSite);
    Serial.println();
#endif

    lastConnectionTime = millis();      // note the time that the connection was made
    client.flush();                     // need to flush or read buffer else it only sends once
    delay(1000);                        // reported to improve reliability
    client.stop();
    delay(1000);                        // so you can see the flash
    digitalWrite(GRN_LED, LOW);         // it's just a flash
  }
  else {
#if (DEBUG)
    Serial.println("Radmon failed");
#endif
  }
}
#endif


#if (SEND_TO_XIVELY)
void sendToXively() {
  digitalWrite(GRN_LED, HIGH);          // signal you're transmitting
  float tmpFloat;
  char fString[6];                      // buffer used to hold float to string conversion

  // Build the data string first - multiple readings by appending to "dataString"
  String dataString = "CPM,";
  dtostrf(f_value[CPM], 1, 0, fString);    // convert float to string
  dataString += fString;

  dtostrf(f_value[DOSE], 1, 2, fString);   // convert float to string
  dataString += "\nDose,";
  dataString += fString;

  dtostrf(f_value[VCC], 1, 2, fString);    // convert float to string
  dataString += "\nVcc,";
  dataString += fString;

  if (f_value[CPM] > maxCPM) maxCPM = f_value[CPM]; // check for maxCPM
  dtostrf(maxCPM, 1, 0, fString);          // convert float to string
  dataString += "\nMaxCPM,";
  dataString += fString;

  // this method makes a HTTP connection to the server and sends a STRING with multiple readings
  if (client.connect(xivelySite, 80)) {
    client.print("PUT /v2/feeds/");
    client.print(FEED_ID);
    client.println(".csv HTTP/1.1");
    client.print("Host: ");
    client.println(xivelySite);
    client.print("X-ApiKey: ");
    client.println(KEY);
    client.print("Content-Length: ");
    client.println(dataString.length());   // use String func to calculate the length of the data sent

    // last pieces of the HTTP PUT request :
    client.println("Content-Type: text/csv");
    client.println("Connection: close");
    client.println();
    client.println(dataString);         // here's the actual content of the PUT request:

    lastConnectionTime = millis();      // note the time that the connection was made
    client.flush();                     // need to flush or read buffer else it only sends once
    delay(1000);                        // reported to improve reliability
    client.stop();
    delay(1000);                        // so you can see the flash
    digitalWrite(GRN_LED, LOW);         // it's just a flash
  }
  else {
#if (DEBUG)
    Serial.println("Xively failed");
#endif
  }
}
#endif


#if (SEND_TO_PHANT) // (data.sparkfun.com)
void sendToPhant() {
  // NOTE: v6.7 of phant converts field names to all lowercase when creating a new account.
  // When v7.0 comes out it will convert transmitted mixed case field names to lower case.
  // If you created an account with v6.7 you must change the field names to all lowercase.
  // The 4 lines to change are commented as follows "*USE "&[var]" IF v6.7*
  // v7.0+ of phant is expected to work with the field names given below.

  digitalWrite(GRN_LED, HIGH);          // signal you're transmitting
  char fString[6];                      // buffer used to hold float to string conversion

  if (client.connect(server, 80)) {     // Make a TCP connection to remote host
    client.print("GET /input/");
    client.print(publicKey);
    client.print("?private_key=");
    client.print(privateKey);

    client.print("&cpm=");                   // send CPM - *USE "&cpm" IF v6.7*
    dtostrf(f_value[CPM], 1, 0, fString);    // convert float to string
    client.print(fString);

    client.print("&dose=");                   // send Dose to 4 places - *USE "&dose" IF v6.7*
    dtostrf(f_value[DOSE], 1, 4, fString);    // convert float to string
    client.print(fString);

    client.print("&vcc=");                   // send Vcc to two places  - *USE "&vcc" IF v6.7*
    dtostrf(f_value[VCC], 1, 2, fString);    // convert float to string64AG1nAGj6Hz0AWNmJgz
    client.print(fString);

    if (f_value[CPM] > maxCPM) maxCPM = f_value[CPM]; // check for maxCPM
    client.print("&maxcpm=");                // send maxCPM  - *USE "maxcpm" IF v6.7*
    dtostrf(maxCPM, 1, 0, fString);          // convert float to string
    client.print(fString);

    // finish up
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(server);
    client.println("Connection: close"); // finished
    client.println();

    lastConnectionTime = millis();      // note the time that the connection was made
    client.flush();                     // need to flush or read buffer else it only sends once
    delay(1000);                        // reported to improve reliability
    client.stop();
    digitalWrite(GRN_LED, LOW);         // it's just a flash
    
    #if(DEBUG)
    Serial.print("GET /input/");
    Serial.print(publicKey);
    Serial.print("?private_key=");
    Serial.print(privateKey);

    Serial.print("&cpm=");                   // send CPM - *USE "&cpm" IF v6.7*
    dtostrf(f_value[CPM], 1, 0, fString);    // convert float to string
    Serial.print(fString);

    Serial.print("&dose=");                   // send Dose to 4 places - *USE "&dose" IF v6.7*
    dtostrf(f_value[DOSE], 1, 4, fString);    // convert float to string
    Serial.print(fString);

    Serial.print("&vcc=");                   // send Vcc to two places  - *USE "&vcc" IF v6.7*
    dtostrf(f_value[VCC], 1, 2, fString);    // convert float to string64AG1nAGj6Hz0AWNmJgz
    Serial.print(fString);

    if (f_value[CPM] > maxCPM) maxCPM = f_value[CPM]; // check for maxCPM
    Serial.print("&maxcpm=");                // send maxCPM  - *USE "maxcpm" IF v6.7*
    dtostrf(maxCPM, 1, 0, fString);          // convert float to string
    Serial.print(fString);

    // finish up
    Serial.println(" HTTP/1.1");
    Serial.print("Host: ");
    Serial.println(server);
    Serial.println("Connection: close"); // finished
    Serial.println();

    #endif
  }
  else {
#if (DEBUG)
    Serial.println(F("Sparkfun failed"));

    // Check for a response from the server, and route it
    // out the serial port.
    while (client.connected()) {
      if ( client.available()) {
        char c = client.read();
        Serial.print(c);
      }
    }
    Serial.println();
#endif
  }
}
#endif


//----------------------------------------------------------------------------------------------+
//                              Get serial data from Geiger kit
//----------------------------------------------------------------------------------------------+
void serialEvent() {
  // Called when serial received, (i.e. 1889,10.7678,4.96) it's parsed into 3 floats
  // Floats are used for compatibility with services that take them - Usv/hr for instance.
  char inStr[10];                       // buffer for serial input
  byte f_valIdx = 0;                    // index to the 3 floats
  byte inStrPos = 0;                    // position in inStr

  memset(inStr, 0, sizeof(inStr));      // clear the input string
  while (Serial.available()) {
    digitalWrite(YEL_LED, HIGH);        // signal buffer is being read
    delay(10);                          // makes you crazy if you don't allow input buffer to fill NEEDED!
    char inChar = Serial.read();
    if (inChar == ',' || inChar == '\r') {
      f_value[f_valIdx] = atof(inStr);  // covert to float (strcat(inStr,'\0') not needed)
      f_valIdx++;
      inStrPos = 0;
      memset(inStr, 0, sizeof(inStr));  // clear the input string again
    }
    else {
      inStr[inStrPos] = inChar;         // just read in the next char
      inStrPos++;
    }
    digitalWrite(YEL_LED, LOW);
    if (inChar == '\r') {
      newData = true;
      break;
    }
  }
}


//----------------------------------------------------------------------------------------------+
//                                         Utilities
//----------------------------------------------------------------------------------------------+

// variables created by the build process when compiling the sketch
extern int __bss_end;
extern void *__brkval;

int AvailRam() { // prints available RAM if in DEBUG
  int freeValue;
  if ((int)__brkval == 0)
    freeValue = ((int)&freeValue) - ((int)&__bss_end);
  else
    freeValue = ((int)&freeValue) - ((int)__brkval);
  return freeValue;
}


