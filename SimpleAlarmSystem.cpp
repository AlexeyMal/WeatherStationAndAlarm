/*******************************************************************
    A Telegram App bot for ESP8266 that controls the 
    433 MHz home alarm system.
    AMA 12.09.2020
 *******************************************************************/

#include "SimpleAlarmSystem.h"

// The version of ESP8266 core needs to be 2.5 or higher
// or else your bot will not connect.

// ----------------------------
// Standard ESP8266 Libraries
// ----------------------------

///#define USE_CLIENTSSL true  
#include <AsyncTelegram2.h>

#include <ESP8266WiFi.h> //already included in the host .ino script. Needed here for BearSSL::
BearSSL::WiFiClientSecure client;
BearSSL::Session   session;
BearSSL::X509List  certificate(telegram_cert);
////#include <WiFiClientSecure.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------
#include <RCSwitch.h>// https://github.com/sui77/rc-switch
///#include <UniversalTelegramBot.h> //https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot

// Initialize Wifi connection to the router
///char ssid[] = "XXX";     // your network SSID (name)
///char password[] = "XXX"; // your network key

// Initialize Telegram BOT
const char* token =  "XXX";  // Telegram token
int64_t userid = 000;  // your telegram user id (to chat with)

const char* firmware_version_date = __DATE__;
const char* firmware_version_time = __TIME__;

////WiFiClientSecure client;
///UniversalTelegramBot bot(BOTtoken, client);
AsyncTelegram2 myBot(client);

//Checks for new messages every botRequestDelay:
///int botRequestDelay = 5 * 60 * 1000; //5 min ///10000; //5000; //1000; //[ms]
///unsigned long lastTimeBotRan;

RCSwitch rfReceiver = RCSwitch();

//LED will light on when alarming
const int ledPin = LED_BUILTIN; //== D4
bool ledOn = false;

const int rfReceiverPin = D2; //4; //pin D2, works. Connect RFReceiver module to this pin. Power it from 3V3 (not from 5V because of risk of killing GPIO pin).
//const int rfReceiverPin = D5; //14; //14=GPIO14 //pin D5, works. Connect RFReceiver module to this pin. Power it from 3V3 (not from 5V because of risk of killing GPIO pin).
///const int rfReceiverPin = D8; // Connect RFReceiver module to this pin. Power it from 3V3 (not from 5V because of risk of killing GPIO pin).

ADC_MODE(ADC_VCC); //to use ESP.getVcc()

typedef struct {
  int remoteID;
  String remoteName;
  int group; // 0 - disarming; 1 - arming; 2 - reported 24h, 3 - reported when armed
  long lastTime;
} device_struct;

#define NUM_DEVICES 33 //<<<
device_struct devices[NUM_DEVICES] = { // add your device RF codes and names here
  
  {000,  "RC1 disarm", 0},
  {000,  "RC1 arm", 1},
  {000,  "RC1 flash", 2},
  {000,  "RC1 SOS", 2},
  {000,  "RC2 disarm", 0},
  {000,  "RC2 arm", 1},
  {000,  "RC2 flash", 2},
  {000,  "RC2 SOS", 2},
  {000,  "RC3 disarm", 0},
  {000,  "RC3 arm", 1},
  {000,  "RC3 flash", 2},
  {000,  "RC3 SOS", 2},
  {000, "RC4 disarm", 0},
  {000, "RC4 arm", 1},
  {000, "RC4 flash", 2},
  {000, "RC4 SOS", 2},
  
  {000,   "26 water spare", 2},
  {000,   "25 water EG", 2},
  {000, "24 water UG", 2},
  
  {000,  "23 smoke UG", 2},
  {000,  "22 smoke EG", 2},
  {000,  "21 smoke UG", 2},
  {000,  "20 smoke EG", 2},
  {000,  "19 smoke OG", 2},
  {000, "18 smoke OG", 2},
  {000,  "17 smoke OG", 2},
  {000, "16 smoke DG", 2},
  {000,  "15 smoke DG", 2},
  {000, "14 smoke DG", 2},
  
  {000, "13 motion spare", 3},
  {000,  "12 motion spare", 3}, 
  {000,  "11 motion OG", 3},
  {000,  "10 motion EG", 3}
  
}; //make the last entry above without a column

bool enable_system = true; //start armed
bool enable_sniffing = false;
bool force_reset = false; // SW reset by Telegram command
bool enable_display = true; // display on/off, toggled by command (also stopping weather updates for better RF listening)
long timeOfLastCheck = 0;

int idSearch(int remoteID) {
  for (int i = 0; i < NUM_DEVICES; i++) {
    if (devices[i].remoteID == remoteID)
      return i;
  }
  return -1;
}

void parseRemote(int remoteID) {
  int currentDevice = idSearch(remoteID);
  if (currentDevice >= 0 && currentDevice < NUM_DEVICES ) {
    if (millis() - devices[currentDevice].lastTime > 1500) { //<<<<<<< [ms] time blanking here
      devices[currentDevice].lastTime = millis();
      //Serial.println(String(remoteID) + ": " + devices[currentDevice].remoteName);
      if ((devices[currentDevice].group <= 2) || enable_system) {
        ledOn = true; //turn LED on
        myBot.sendTo(userid, String(remoteID) + ": " + devices[currentDevice].remoteName);
      }
      if (devices[currentDevice].group == 1)
      {
        //arm
        enable_system = true;
        enable_display = false; //turn display and weather updates off (to save oled lifetime and improve RF listening)
        ledOn = false; //turn LED off
      }
      if (devices[currentDevice].group == 0)
      {
        //disarm
        enable_system = false;  
        enable_display = true;
        ledOn = false; //turn LED off
      }
    }
  } else {
    //Serial.println(String(remoteID) + ": " + "unknown code");
    if (enable_sniffing)
      myBot.sendTo(userid, String(remoteID) + ": " + "unknown code"); //use this to get the sensor id codes 
  }
}

///void handleNewMessages(int numNewMessages) {
void handleNewMessage(TBMessage newMessage) {
  //Serial.println("handleNewMessages");
  //Serial.println(String(numNewMessages));

  ///for (int i=0; i<numNewMessages; i++) {
    //String chat_id = String(bot.messages[i].chat_id);

    //String from_name = bot.messages[i].from_name;
    //if (from_name == "") from_name = "Guest";

    ///if (bot.messages[i].chat_id != chat_id) { // process commands only from me
    if (newMessage.chatId != userid) { // process commands only from me
      //bot.sendMessage(bot.messages[i].chat_id, "You are not authorized", "");
      return;
    }
    
    //String text = bot.messages[i].text;
    String text = newMessage.text;
    
    if (text == "/arm") {
      enable_system = true;
      ///bot.sendMessage(chat_id, "armed", "");
      myBot.sendTo(userid, "armed");
    }

    else if (text == "/disarm") {
      enable_system = false;
      ///bot.sendMessage(chat_id, "disarmed", "");
      myBot.sendTo(userid, "disarmed");
    }
    
    else if (text == "/status") {
      String str = "";
      str += "Name " + WiFi.hostname() +"\n";
      str += "Firmware " + String(firmware_version_date) + " " + String(firmware_version_time) +"\n";
      if(enable_system){
        str += "Alarm system armed\n";
      } else {
        str += "Alarm system disarmed\n";
      }
      if(ledOn){
        str += "LED on\n";
      } else {
        str += "LED off\n";
      }
      if(enable_display){
        str += "Display on\n";
      } else {
        str += "Display off\n";
      }      
      str += "IP " + WiFi.localIP().toString() +"\n";
      str += "WiFi " + String(WiFi.RSSI()) + " dB\n";
      char time_str[15];
      const uint32_t millis_in_day = 1000 * 60 * 60 * 24;
      const uint32_t millis_in_hour = 1000 * 60 * 60;
      const uint32_t millis_in_minute = 1000 * 60;
      uint8_t days = millis() / (millis_in_day);
      uint8_t hours = (millis() - (days * millis_in_day)) / millis_in_hour;
      uint8_t minutes = (millis() - (days * millis_in_day) - (hours * millis_in_hour)) / millis_in_minute;
      sprintf(time_str, "%2d d %2d h %2d m", days, hours, minutes);
      str += "Uptime " + String(time_str) +"\n";
      //str += "Heap " + String(ESP.getFreeHeap() / 1024) + " KB\n";
      str += "Heap " + String(ESP.getFreeHeap()) + " B\n";
      str += "CPU " + String(ESP.getCpuFreqMHz()) + " MHz\n";
      str += "Vcc " + String(ESP.getVcc() / 912.0) + " V\n"; // 1024 // https://www.mikrocontroller.net/topic/438271 => 912 divider is better
      str += "Boot " + ESP.getResetInfo() +"\n";

      myBot.sendTo(userid, str);
    }
    
    else if (text == "/list") {
      String list = "Device codes, names and groups:\n";
      for (int currentDevice = 0; currentDevice < NUM_DEVICES; currentDevice++ ) {
        list += String(devices[currentDevice].remoteID) + ": " + devices[currentDevice].remoteName + " (" + String(devices[currentDevice].group) + ")\n";
      }
      myBot.sendTo(userid, list);
    }
    
    else if (text == "/sniff") {
      enable_sniffing = !enable_sniffing;
      if(enable_sniffing){
        myBot.sendTo(userid, "sniffing turned on");
      } else {
        myBot.sendTo(userid, "sniffing turned off");
      }
    }

    else if (text == "/display") {
      enable_display = !enable_display;
      if(enable_display){
        myBot.sendTo(userid, "display turned on");
      } else {
        myBot.sendTo(userid, "display turned off");
      }
    }

    else if (text == "/heap") {
      uint32_t free;
      uint32_t max;
      ESP.getHeapStats(&free, &max, nullptr);
      char heap_msg[64];
      //Serial.printf("Total free: %5u - Max block: %5u\n", free, max);
      snprintf(heap_msg, 64, "Total free: %5u - Max block: %5u", free, max);
      myBot.sendTo(userid, heap_msg);
    } 	
	
    else if (text == "/reset") {
      //myBot.sendTo(userid, "reset is not implemented beacuse it causes reset loop"); //beacuse it causes reset loop
      myBot.sendTo(userid, "reset command received");
      force_reset = true; //set the flag
      //myBot.getNewMessage(newMessage); // flush the message que
      //yield();
      //delay(60000); //give it time to mark the Telegram message as read //give CPU time to the Wi-Fi/TCP stacks, https://tttapa.github.io/ESP8266/Chap04%20-%20Microcontroller.html
      //ESP.restart();
    }    
        
    else if (text == "/led") {
      ledOn = !ledOn;
      if(ledOn){
        myBot.sendTo(userid, "LED turned on");
      } else {
        myBot.sendTo(userid, "LED turned off");
      }      
    }

    else if ((text == "/start") || (text == "/help")) {
      //String welcome = "Welcome, " + from_name + ".\n";
      String welcome = "Commands:\n";
      welcome += "/arm : to arm\n";
      welcome += "/disarm : to disarm\n";
      welcome += "/status : returns current status\n";
      welcome += "/list : lists all known devices \n";
      welcome += "/sniff : toggles RF codes sniffing mode\n";
      welcome += "/display : toggles display on-off\n";
      welcome += "/heap : returns heap usage\n";
      welcome += "/reset : to reset\n";
      welcome += "/led : toggles LED\n";
      welcome += "/start or /help : to show commands\n";
      myBot.sendTo(userid, welcome ); //, "Markdown"); TODO
    }

}

/*
void setup() {
  Serial.begin(115200);

  // This is the simplest way of getting this working
  // if you are passing sensitive information, or controlling
  // something important, please either use certStore or at
  // least client.setFingerPrint
  client.setInsecure();

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // attempt to connect to Wifi network:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  bot.sendMessage(chat_id, "booted", "");

  rfReceiver.enableReceive(rfReceiverPin);

  pinMode(ledPin, OUTPUT); // initialize digital ledPin as an output.
  delay(10);
  digitalWrite(ledPin, HIGH); // initialize pin as off (active high)
}
*/

// global variable to store telegram message data
//TBMessage msg;

void SimpleAlarmSystemSetup() {
  
  // This is the simplest way of getting this working
  // if you are passing sensitive information, or controlling
  // something important, please either use certStore or at
  // least client.setFingerPrint
 //// client.setInsecure();
 
  ///client.setCACert(telegram_cert); 
  client.setSession(&session);
  client.setTrustAnchors(&certificate);
  client.setBufferSizes(1024, 1024);
  
  // Set the Telegram bot properies
  //myBot.setUpdateTime(60000); //60000 [ms] <<<<<<<< 
  myBot.setUpdateTime(60000); //60000 [ms] <<<<<<<< 
  myBot.setTelegramToken(token);
  myBot.setJsonBufferSize(3072); //<<<< AMA increased the buffer. 2k caused "[ERROR] out of memory" twice a day.
  myBot.begin();
  timeOfLastCheck = millis();
      
  //bot.sendMessage(chat_id, "booted", "");
  // Send a message to specific user who has started your bot
  //myBot.sendTo(userid, "booted"); // <<<<<<<<<
 
  // Send a welcome message to user when ready
  //char welcome_msg[96];
  //snprintf(welcome_msg, 96, "%s booted.\nFirmware: %s %s\n/help for command list.", myBot.getBotName(), firmware_version_date, firmware_version_time);
  String str = "";
  str += "Name " + WiFi.hostname() +" booted\n";
  str += "Firmware " + String(firmware_version_date) + " " + String(firmware_version_time) +"\n";  
  str += "Boot " + ESP.getResetInfo() +"\n";
  str += "/help\n";
  // Check the userid with the help of bot @JsonDumpBot or @getidsbot (work also with groups)
  // https://t.me/JsonDumpBot  or  https://t.me/getidsbot
  myBot.sendTo(userid, str);
  delay(0);
 
  rfReceiver.enableReceive(rfReceiverPin);

  pinMode(ledPin, OUTPUT); // initialize digital ledPin as an output.
  //delay(10);
  //digitalWrite(ledPin, HIGH); // initialize pin as off (active high)  
}

//void loop() {
void SimpleAlarmSystemLoop() {

  // check rfReceiver
  if (rfReceiver.available()) {
    parseRemote(rfReceiver.getReceivedValue());
    rfReceiver.resetAvailable();
    delay(0); //give CPU time to the Wi-Fi/TCP stacks, https://tttapa.github.io/ESP8266/Chap04%20-%20Microcontroller.html
  }
      
  // check for new Telegram messages
  // This seems to take a second, the rfReception seems to be not listerning for that busy time, it is missing RF codes.
  // I do not have a solution for this problem. A radical solution is not to check messages at all, as in "SimleAlarmSystem" project.
  /*if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    // doing this here inbetween again in order not to miss RFCodes (communication with Telegram takes a second)
    // check rfReceiver
    if (rfReceiver.available()) {
      parseRemote(rfReceiver.getReceivedValue());
      rfReceiver.resetAvailable();
    }
  
    //while(numNewMessages) { //AMA commented this out, check only one message per loop cycle
      //Serial.println("got response");
      handleNewMessages(numNewMessages);
      //numNewMessages = bot.getUpdates(bot.last_message_received + 1); //AMA commented this out, process only one message per turn
    //}
    lastTimeBotRan = millis();
  }*/

  // local variable to store telegram message data
  TBMessage msg;

  // if there is an incoming message...
  if (myBot.getNewMessage(msg)) {    
    // echo the received message
    //myBot.sendMessage(msg, msg.text);
    handleNewMessage(msg);
  }
  delay(0); //give CPU time to the Wi-Fi/TCP stacks, https://tttapa.github.io/ESP8266/Chap04%20-%20Microcontroller.html

  //reset watchdog for lost Telegram connection
  if (millis() - timeOfLastCheck > 300000L) //<<<<<<<<<  watchdog interval //60000 //5min
  {
    if(myBot.begin() != true) //test Telegram connection
    {
      //Serial.println("Connection NOK");
      //force_reset = true;
      delay(0);
      //delay(5000);
      myBot.reset(); //reset the Telegram connection
      delay(0);
      if(myBot.begin() != true) //try to restore the Telegram connection, test Telegram connection, 2nd chance
      {
        ESP.restart();      
      }
    }
    timeOfLastCheck = millis();
  }

  // forced reset triggered by a Telegram command
  if(force_reset)
  {
    force_reset = false;
    // Wait until bot synced with telegram to prevent cyclic reboot
    while (! myBot.noNewMessage()) 
    {
    //Serial.print(".");
    delay(100);
    }    
    //Serial.println(F("Restart in 5 seconds..."));
    delay(5000);
    ESP.restart();
  }  
  
  if(ledOn){
    digitalWrite(ledPin, LOW);   // turn the LED on (HIGH is the voltage level)
  } else {
    digitalWrite(ledPin, HIGH);    // turn the LED off (LOW is the voltage level)
  }  
}
