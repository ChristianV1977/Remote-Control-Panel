/*
 * Version 1
 * 2016-03-15
*/

#include <WiFi101.h>
#include <Firmata.h>
#include <RTCZero.h>
#include "utility/SerialFirmata.h"
#include <ArduinoJson.h>
#include "utility/WiFi101Stream.h"

#define WIFI_MAX_CONN_ATTEMPTS      3
#define SERVER_PORT                 3030                //port for firmata

/*/custom sysex commands used for 
//test connection is up by switching led on/off
//Do a refresh of data outside the normal cycle
//Set the inverter mode - Utility,Solar or SBU */

#define TEST_MODE                   0x40
#define RELOAD_NOW                  0x41      
//#define SET_MODE		                0x42     

///*** WiFi Network Config ***///
char ssid[] = "<ssid here>";                              //  your network SSID (name)
char pass[] = "<password here>";                             // your network password (use for WPA, or use as key for WEP)

///*** Azure IoT Hub Config ***///
char hostname[] = "<enter here>";    // host name address for your Azure IoT Hub
char authSAS[] = "<enter here>";
String azureReceive = "/devices/<enter here>/messages/devicebound?api-version=2016-02-03";
char feeduri[] = "/devices/<enter here>/messages/devicebound?api-version=2016-02-03"; //feed URI 
char azurePOST_Uri[]= "/devices/<enter here>/messages/events?api-version=2016-02-03";    // feed POST Uri
unsigned long lastConnectionTime = 0;            

WiFi101Stream stream;                                   
RTCZero rtc;
//SerialFirmata serialFeature;                            //enable
StaticJsonBuffer<300> jsonBuffer;
WiFiSSLClient client;


/*********************************************************  
 *   
*********************************************************/
JsonObject& root = jsonBuffer.createObject();    


int status = WL_IDLE_STATUS;
int wifiConnectionAttemptCounter = 0;
int wifiStatus = WL_IDLE_STATUS;
int last_request;
int serial_time_out = 10;

//String inData;                                          // global string to hold the inverter answer   
const int MKR_LED = 6;
const int MKR_WIFI = 0;
const int MKR_INFO = 1;

int wait_cycle = 60;                                    // set the cycle time in seconds autmatic update will happen every wait_cycle seconds
boolean isResetting = false;
signed char queryIndex = -1;

/* timer variables */
unsigned long currentMillis;        // store the current value from millis()
unsigned long previousMillis;       // for comparison with currentMillis

//Device commands with 2 CRC bytes
String QPIGS = "\x51\x50\x49\x47\x53\xB7\xA9\x0D";
String QPIWS = "\x51\x50\x49\x57\x53\xB4\xDA\x0D";  
String QDI = "\x51\x44\x49\x71\x1B\x0D";
String QMOD = "\x51\x4D\x4F\x44\x49\xC1\x0D"; 
String QVFW =  "\x51\x56\x46\x57\x62\x99\x0D"; 
String QVFW2 = "\x51\x56\x46\x57\x32\xC3\xF5\x0D"; 

String POP02 = "\x50\x4F\x50\x30\x32\xE2\x0B\x0D";  //SBU priority
String POP01 = "\x50\x4F\x50\x30\x32\xE2\xD2\x69";  //solar first
String POP00 = "\x50\x4F\x50\x30\x30\xC2\x48\x0D";  //utility first

/*********************************************************  
 *   setup
*********************************************************/
void systemResetCallback()
{
  isResetting = true;

  // initialize a defalt state
  // TODO: option to load config from EEPROM instead of default


  for (byte i = 0; i < TOTAL_PINS; i++) {
    // pins with analog capability default to analog input
    // otherwise, pins default to digital output
    if (IS_PIN_ANALOG(i)) {
      // turns off pullup, configures everything
      //tPinModeCallback(i, PIN_MODE_ANALOG);
    } else if (IS_PIN_DIGITAL(i)) {
      // sets the output to 0, configures portConfigInputs
     //etPinModeCallback(i, OUTPUT);
    }

  }
  isResetting = false;
}

void setup() {

  pinMode(MKR_LED, OUTPUT);
  pinMode(MKR_INFO, OUTPUT);
  pinMode(MKR_WIFI, OUTPUT);
  /*
   * FIRMATA SETUP
   */
  Firmata.setFirmwareVersion(FIRMATA_FIRMWARE_MAJOR_VERSION, FIRMATA_FIRMWARE_MINOR_VERSION);
  Firmata.attach(STRING_DATA, stringCallback);
  Firmata.attach(SYSTEM_RESET, systemResetCallback);
  Firmata.attach(START_SYSEX, sysexCallback);
   // start up Network Firmata:  

   
  connect_wifi();
  printWifiStatus();

  rtc.begin(); // initialize RTC
  Serial.begin(2400);
  Serial1.begin(2400);
  Serial1.setTimeout(100);
  last_request=rtc.getEpoch()-wait_cycle; //set last update request time to the past so updates run directly after reset.
  //testblink(MKR_LED,MKR_WIFI,MKR_INFO,200,5);
     //read_QVFW();
    // read_QVFW2();
}
 
/*********************************************************  
 *   Main loop
*********************************************************/
void loop() 
{
 
  /* STREAMREAD - processing incoming messagse as soon as possible, while still  checking digital inputs.  */
  if ( WiFi.status() == 3 )

  {

   while (Firmata.available()) 
  {
    digitalWrite(MKR_INFO,HIGH);
    Firmata.processInput();
  }
  stream.maintain();
  }
  else
  {
     ledblink(MKR_LED, 500, 4);
     connect_wifi();
  }
   //Serial.println(WiFi.status());
   printWifiStatus();
   delay(500);

  if (last_request + wait_cycle < rtc.getEpoch())               //Run every wait_cycle seconds
  {
     last_request = rtc.getEpoch();
     read_QMOD();
     read_QPIGS();
     //read_QPIWS();     



     String x;
     root.printTo(x);
     Firmata.sendString(x.c_str());                       
     //digitalWrite(MKR_INFO,LOW);
     Serial.println(x.c_str());  
     //if (connect_wifi()) {
       //  ledblink(MKR_LED, 500, 4);
     azureHttpPOST(x);
     JsonObject& root = jsonBuffer.createObject();     
     //} 
          
  }
};



//******************* Open WiFi connection ****************/

int connect_wifi() 
{
   
   WiFi.disconnect(); 
   //ledblink(MKR_WIFI, 100, 10);
   while ( WiFi.status() != 3) 
       {
          wifiStatus = stream.begin(ssid, pass, SERVER_PORT);
          ledblink(MKR_WIFI, 500, 4);
          delay(3000); // TODO - determine minimum delay
          if (++wifiConnectionAttemptCounter > WIFI_MAX_CONN_ATTEMPTS) break;
       }
    Firmata.begin(stream);
    systemResetCallback();  // reset to default config

   return 1;
};


//******************* get the current time ****************/
uint32_t getNow()
{
  return rtc.getEpoch();
};



//******************* send a QPIGS command to inverter and store values ****************/
//'(227.0 50.1 240.0 50.2 0490 0451 011 438 53.70 002 100 0044 0002 097.5 53.94 000 00 00010110 00 00 00129 110]I',

void read_QPIGS(){
  int end_time;
  end_time = serial_time_out + rtc.getEpoch();
 
  Serial1.print(QPIGS);
  
  
  while (Serial1.available() == 0 && rtc.getEpoch()< end_time)   ///wait for answer or timeout
  {
  }
  if (Serial1.find("(")) {
    root["INP_VOLT"] = Serial1.parseFloat();        //QPIGS.1 Grid voltage      
    root["INP_FREQ"] = Serial1.parseFloat();        //QPIGS.2 Grid frequency
    root["OUT_VOLT"] = Serial1.parseFloat();       //QPIGS.3 Out voltage  
    root["OUT_FREQ"] = Serial1.parseFloat();       //QPIGS.4 Out frequency
    root["OUT_LOAD_VA"] = Serial1.parseInt();     //QPIGS.5 AC output apparent power
    root["OUT_LOAD_W"] = Serial1.parseInt();      //QPIGS.6 AC output active power
    root["OUT_LOAD_PERC"] = Serial1.parseInt();   //QPIGS.7 Output load percent 
    Serial1.parseInt();                             //skip
    root["INP_BAT_VOLT"] = Serial1.parseFloat();    //QPIGS.9 Battery voltage 
    root["INP_CHG_AMP"] = Serial1.parseInt();     //QPIGS.10 Battery charging current
    root["BATT_LEVEL_PERC"] = Serial1.parseInt(); //QPIGS.11 Battery capacity 
    Serial1.parseInt();                             //skip
    Serial1.parseInt();                             //skip
    root["INP_PV_VOLT"] = Serial1.parseFloat();     //QPIGS.14 PV Input voltage 
    Serial1.parseInt();                             //skip
    root["OUT_DISC_AMP"] = Serial1.parseInt();    //QPIGS.16 Battery discharge current
    
    }


};

//
////******************* send a QDI command to inverter and store values ****************/
//void read_QDI(){
//  int end_time;
//  end_time = serial_time_out + rtc.getEpoch();
//
// Serial1.print(QDI);
//  while (Serial1.available() == 0 && rtc.getEpoch()< end_time )   ///wait for answer or timeout
//  {
//  }
// //Serial.setTimeout(10);
// //while (Serial.available() == 0  && rtc.getEpoch()< end_time )   ///wait for answer
// //while (Serial1.available() == 0  && rtc.getEpoch()< end_time )   ///wait for answer
// //{
// //    ledblink(MKR_LED, 200, 4);
// //}      
//    //if (Serial.find("(")) {
//    if (Serial1.find("(")) {
//      Serial.println('qdi');
//      //root["OUT_VOLT"] = Serial.parseFloat();        //QDI.1 AC output voltage       
//      //root["OUT_FREQ"] = Serial.parseFloat();        //QDI.2 AC output frequency
//      root["OUT_VOLT"] = Serial1.parseFloat();        //QDI.1 AC output voltage       
//      root["OUT_FREQ"] = Serial1.parseFloat();        //QDI.2 AC output frequency
//   }
//  
//};

//******************* send a QPIWS command to inverter and store values ****************/
void read_QPIWS(){
  String inData;
  int end_time;
  end_time = serial_time_out + rtc.getEpoch();
  //ledblink(MKR_LED, 500, 3);
  Serial1.print(QPIWS);
  while (Serial1.available() == 0 && rtc.getEpoch()< end_time)   ///wait for answer or timeout
  {
  }
  inData= Serial1.readStringUntil('\r');
  //while (Serial1.available() > 0)
    // {   
    //    char recieved = Serial1.read();
    //
    //      inData += recieved; 
    //
    //  }
    root["I_SETTING"] = inData.substring(1,30);
};

//******************* send a QMOD command to inverter and store values ****************/
void read_QMOD(){
  String inData;
  int end_time;
  end_time = serial_time_out + rtc.getEpoch();  
  //ledblink(MKR_LED, 500, 3);
  Serial1.print(QMOD);
  while (Serial1.available() == 0 && rtc.getEpoch()< end_time)   ///wait for answer or timeout
  {
  }
  inData= Serial1.readStringUntil('\r');
  //while (Serial1.available() > 0)
    // {   
    //    char recieved = Serial1.read();
    //
    //      inData += recieved; 
    //
    //  }
    root["MODE_MAINS"] = inData.substring(1,2);
};

//******************* send a QVFW command to inverter and store CPU software version ****************/
void read_QVFW(){
  String inData;
  int end_time;
  end_time = serial_time_out + rtc.getEpoch();
  //ledblink(MKR_LED, 500, 3);
  Serial1.print(QVFW);
  while (Serial1.available() == 0  && rtc.getEpoch()< end_time)   ///wait for answer or timeout
  {
  }
  //inData= Serial1.readStringUntil('\r');
  while (Serial1.available() > 0)
     {   
        char recieved = Serial1.read();
    
          inData += recieved; 
    
      }
    if (inData.substring(1,3) != "NAK"  && inData.substring(0,1) != ""  ){
    root["CPU_VERSION"] = inData.substring(1,13); 
    }
    else
    {
    root["CPU_VERSION"] = "VERFW:?????.??";       
      }
}  


//******************* send a QVFW2 command to inverter and store CPU2 software version ****************/
void read_QVFW2(){
  String inData;
  int end_time;
  end_time = serial_time_out + rtc.getEpoch();
  Serial.println("1a");
  Serial1.print(QVFW2);
  while (Serial1.available() == 0  && rtc.getEpoch()< end_time)   ///wait for answer or timeout
  {
  }
  Serial.println("2a");
  //inData= Serial1.readStringUntil('\r');
  while (Serial1.available() > 0)
     {   
        char recieved = Serial1.read();
    
          inData += recieved; 
    
      }
    Serial.print(".");
    Serial.print(inData);
    Serial.println(".");
    if (inData.substring(1,3) != "NAK"  && inData.substring(0,1) != ""  ){
    root["CPU2_VERSION"] = inData.substring(1,13); 
    }
    else
    {
    root["CPU2_VERSION"] = "VERFW:?????.??";       
      }
    
};



//******************* send data to Azure ****************/
void azureHttpPOST(String content) {

// close any connection before send a new request.
// This will free the socket on the WiFi shield
client.stop();

// if there’s a successful connection:
if (client.connect(hostname, 443)) {
//make the GET request to the Azure IOT device feed uri
client.print("POST "); //Do a GET
client.print(azurePOST_Uri); // On the feedURI
client.println(" HTTP/1.1");
client.print("Host: ");
client.println(hostname); //with hostname header
client.print("Authorization: ");
client.println(authSAS); //Authorization SAS token obtained from Azure IoT device explorer
client.println("Connection: close");
client.println("Content-Type: text/plain");
client.print("Content-Length: ");
client.println(content.length());
client.println();
client.println(content);
client.println();

// note the time that the connection was made:
lastConnectionTime = millis();
}
else {
// if you couldn’t make a connection:

}
};


//******************* blink all three leds testmode  ****************/
void testblink(int portnum, int portnum1, int portnum2,int timedelay, int flashnum) {
  for(int i = 0 ; i < flashnum ; i++){
  digitalWrite(portnum, LOW);
  digitalWrite(portnum1, LOW);
  digitalWrite(portnum2, LOW);
  delay(timedelay);
  digitalWrite(portnum, HIGH);
  digitalWrite(portnum1, HIGH);
  digitalWrite(portnum2, HIGH);
  delay(timedelay);
  }
};

//******************* blink a led  ****************/
void ledblink(int portnum, int timedelay, int flashnum) {
  for(int i = 0 ; i < flashnum ; i++){
  digitalWrite(portnum, LOW);
  delay(timedelay);
  digitalWrite(portnum, HIGH);
  delay(timedelay);
  }
};

//******************* stringCallback  ****************/
void stringCallback(char *myString)

{
   Serial.println(myString);
   //Firmata.sendString(myString);
  
};
//******************* setPinValueCallback  ****************/
void setPinValueCallback(byte pin, int value)
{
  if (pin < TOTAL_PINS && IS_PIN_DIGITAL(pin)) {
    if (Firmata.getPinMode(pin) == OUTPUT) {
      Firmata.setPinState(pin, value);
      digitalWrite(PIN_TO_DIGITAL(pin), value);
    }
  }
};

//******************* printWifiStatus  ****************/
void printWifiStatus() {
  if ( WiFi.status() != WL_CONNECTED )
  {
    digitalWrite(MKR_WIFI, LOW);

  }
  else
  {
     digitalWrite(MKR_WIFI, HIGH);
  }
};

// -----------------------------------------------------------------------------
/* process sysex commands
 * 
 */
void sysexCallback(byte command, byte argc, byte *argv)
{
  byte mode;
  byte stopTX;
  byte slaveAddress;
  byte data;
  int slaveRegister;
  unsigned int delayTime;

  switch (command) {
    case TEST_MODE:
         if( argc == 1 ) //verify we received the whole message
            {
                //the MSB is always sent in the LSB position 2nd byte
                byte val;
                if( argv[0] == 1 )
                {
                    val = HIGH;
                }
                else
                {
                    val = LOW;
                }
        
                digitalWrite( MKR_LED, val );
				Firmata.write(END_SYSEX);
            }

		 break;
    case RELOAD_NOW:
             Serial.println("RELOAD_NOW");
             read_QMOD();
             read_QPIGS();
             read_QPIWS();     
        
             String x;
             root.printTo(x);
             Firmata.sendString(x.c_str()); 
             Firmata.write(END_SYSEX);
      break;
//	case SET_MODE:
//		Serial.println("SET_MODE");
//		if (argc == 1) //verify we received the whole message
//		{
//			
//			byte val;
//			switch (argv[0]) {
//			case 0:
//				Serial1.print(POP00);
//				break;
//			case 1:
//				Serial1.print(POP00);
//				break;
//			case 2:
//				Serial1.print(POP00);
//				break;
//			}
//		}
//
//		Firmata.write(END_SYSEX);
//		break;
  }
}



