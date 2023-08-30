//
// Beer brew monitoring of temperature
//
// ------------------------
// 128x64 OLED pinout:
// Data to I2C SDA (GPIO 4) purple(blue)
// Clk to I2C SCL (GPIO 5) grey(green)
// GND goes to ground
// Vin goes to 3.3V
// ------------------------
// buzz D5
// button D6 and TX
// ------------------------


#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <String.h>
#include <Wire.h>
#include <SSD1306.h>
#include <SSD1306Wire.h>
#include "IFTTTWebhook.h"

// Create a display object
//0x3d for the Adafruit 1.3" OLED, 0x3C being the usual address of the OLED
SSD1306  display(0x3c, 4, 5);
// Setup a oneWire instance to communicate with any OneWire devices
#define ONE_WIRE_BUS 2 // DS18B20 on NodeMCU pin D4
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature DS18B20(&oneWire);


//********************************
#define SERIAL_DEBUG // Uncomment this to dissable serial debugging
//********************************

// set delay period
int period = 30 * 1000;
unsigned long last_time = 0;

//------- WiFi Settings -------
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASS";

//------- deep sleep ----------
const int sleepTimeS = 100;

//------- ntp settings --------
unsigned int localPort = 2390;      // local port to listen for UDP packets
/* Don't hardwire the IP address or we won't get the benefits of the pool.
    Lookup the IP address for the host name instead */
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
#define GMT 3  //timezone
uint8_t hour, minute, second, day, month, year, weekday;
uint8_t monthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
String Time1;
unsigned long secsSince1900;

// ------- ifttt config --------
//https://maker.ifttt.com/trigger/MY_IFTTT_EVENT_NAME/with/key/MY_IFTTT_API_KEY
#define IFTTT_API_KEY "MY_IFTTT_API_KEY"
#define IFTTT_EVENT_NAME "MY_IFTTT_EVENT_NAME"
unsigned long last_time_message;
unsigned long current_time;
int period_message = 300; // delay beetwen sending message

//------- temp bounds --------
float t1;
float default_temp = 25;
boolean default_temp_state = false;
float t_first_pause = 55;
boolean t_first_pause_state = false;
float t_second_pause = 65;
boolean t_second_pause_state = false;
float t_third_pause = 72;
boolean t_third_pause_state = false;
float t_mash_out = 78;
boolean t_mash_out_state = false;
float t_boil = 98;
boolean t_boil_state = false;


//------- temp button --------
const int  p_button = 13;    // the pin that the pushbutton is attached D7
volatile int buttonPushCounter = 0;   // counter for the number of button presses
volatile int buttonState = 0;         // current state of the button
//------- buzzer ----------
const int p_sound = 14;

WiFiClient client;
WiFiUDP udp;

String ipAddress = "";

//converter string to char ??
char *append_str(char *here, char *s) {
  while (*here++ = *s++)
    ;
  return here - 1;
}

void setup () {
// config for debugging
#ifdef SERIAL_DEBUG
  delay(1000);
  Serial.begin(115200);
  delay(200);
  Serial.println();
#endif
  // init buzzer
  pinMode(p_sound, OUTPUT);
  beep(80);
  // init builtin LED light
  pinMode (LED_BUILTIN, OUTPUT);
  light(80);
  // init OLED
  Wire.pins(4, 5);  // Start the OLED with GPIO 4 and 5 on NodeMCU
  Wire.begin(4, 5); // D2 - sda, D1 - sck NodeMCU 1.0
  display.init();
  display.flipScreenVertically();
  // init DS18B20 temperature senson
  DS18B20.begin(); // Start temp sensor
  debugln("Testing Dual Sensor data");
  display.drawString(0, 0, "Testing Dual Sensor data");
  display.display();

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);


  debug("Connecting Wifi: ");
  debugln(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    debug(".");
    delay(500);
    display.drawString(0, 10, "Connecting to Wifi...");
    display.display();
  }
  debugln("");
  debugln("WiFi connected");
  debug("IP address: ");
  IPAddress ip = WiFi.localIP();
  ipAddress = ip.toString();
  debugln(ipAddress);
  display.drawString(0, 20, "Connected.");
  display.drawString(0, 30, ipAddress);
  display.display();
  debugln("Starting UDP");
  udp.begin(localPort);
  debug("Local port: ");
  debugln(String(udp.localPort()));


  // init button
  pinMode(p_button, INPUT_PULLUP);
  attachInterrupt ( digitalPinToInterrupt (p_button), buttonInterrupt, FALLING);

  // get time from NTP server
  gettime();

  // start messages
  float temp_0 = getTemperature();
  // test if ESP is working twice per day
  String msg_test;
  debugln("-- Online test ESP --");
  msg_test = String("BREW: ");
  msg_test += String(Time1);
  msg_test += " t:";
  msg_test += temp_0;
  debugln(msg_test);
  sendIFTTTrequest_http(msg_test);
  display.drawString(0, 40, msg_test);
  display.display();
  delay(5000);
  //  debugln("closing connections");
  //  debugln("Sleep Mode");
  //  ESP.deepSleep(sleepTimeS * 2000000);


}


void sendIFTTTrequest_http(String msg) {
  // create and convert to char messages (ssid, ip, temp)
  String message1 = "SSID: ";
  message1.concat(ssid);
  String message2 = "IP: ";
  message2.concat(ipAddress);
  String message3 = msg;
  char cmsg1[message1.length() + 1]; char cmsg2[message2.length() + 1]; char cmsg3[message3.length() + 1];
  message1.toCharArray(cmsg1, message1.length() + 1);
  message2.toCharArray(cmsg2, message2.length() + 1);
  message3.toCharArray(cmsg3, message3.length() + 1);

  debugln("IFTTT values:");
  debugln(cmsg1);
  debugln(cmsg2);
  debugln(cmsg3);
  debugln("-------------");
  // connect to the Maker event server
  client.connect("maker.ifttt.com", 80);

  // construct the POST request
  char post_rqst[257];    // hand-calculated to be big enough

  char *p = post_rqst;
  p = append_str(p, "POST /trigger/");
  p = append_str(p, IFTTT_EVENT_NAME);
  p = append_str(p, "/with/key/");
  p = append_str(p, IFTTT_API_KEY);
  p = append_str(p, " HTTP/1.1\r\n");
  p = append_str(p, "Host: maker.ifttt.com\r\n");
  p = append_str(p, "Content-Type: application/json\r\n");
  p = append_str(p, "Content-Length: ");

  // we need to remember where the content length will go, which is:
  char *content_length_here = p;

  // it's always two digits, so reserve space for them (the NN)
  p = append_str(p, "NN\r\n");

  // end of headers
  p = append_str(p, "\r\n");

  // construct the JSON; remember where we started so we will know len
  char *json_start = p;

  // As described - this example reports a pin, uptime, and "via#tuinbot"
  p = append_str(p, "{\"value1\":\"");
  p = append_str(p, cmsg1);
  p = append_str(p, "\",\"value2\":\"");
  p = append_str(p, cmsg2);
  p = append_str(p, "\",\"value3\":\"");
  p = append_str(p, cmsg3);
  p = append_str(p, "\"}");

  // go back and fill in the JSON length
  // we just know this is at most 2 digits (and need to fill in both)
  int i = strlen(json_start);
  content_length_here[0] = '0' + (i / 10);
  content_length_here[1] = '0' + (i % 10);
  last_time_message = secsSince1900;
  // finally we are ready to send the POST to the server!
  client.print(post_rqst);
  client.stop();
  debug (post_rqst);
  IFTTTWebhook wh(IFTTT_API_KEY, IFTTT_EVENT_NAME);
  //wh.trigger();
  //wh.trigger("1");
  //wh.trigger("1", "2");
  //wh.trigger("1", "2", "3");
}

float getTemperature() {
  float result;
  DS18B20.requestTemperatures();
  result = DS18B20.getTempCByIndex(0);
  // debug ("result: ");
  // debugln(String(result));
  result = round(result * 10) / 10.0;
  return result;
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

String Time_to_String() {
  String Tm, wd;
  int k, n;

  k = day / 10;
  n = day % 10;
  Tm = String(k) + String(n) + ".";
  k = month / 10;
  n = month % 10;
  Tm = Tm + String(k) + String(n) + ".";
  // short version of  yyear - 2000
  k = 1900 + year - 2000;
  // Tm = Tm + String(k) + " " + wd + " ";
  Tm = Tm + String(k) + " ";
  k = hour / 10;
  n = hour % 10;
  Tm = Tm + String(k) + String(n) + ":";
  k = minute / 10;
  n = minute % 10;
  Tm = Tm + String(k) + String(n);
  //Uncommment if you need seconds
  k = second / 10;
  n = second % 10;
  Tm = Tm + ":" + String(k) + String(n);
  //
  //Tm = "10 05 2018 th";
  return (Tm);
}

void Unix_to_GMT(unsigned long epoch) {
  // correct timezone
  epoch = epoch + GMT * 3600;
  second = epoch % 60;
  epoch /= 60; // now it is minutes
  minute = epoch % 60;
  epoch /= 60; // now it is hours
  hour = epoch % 24;
  epoch /= 24; // now it is days
  weekday = (epoch + 4) % 7; // day week, 0-sunday
  year = 70;
  int days = 0;
  while (days + ((year % 4) ? 365 : 366) <= epoch) {
    days += (year % 4) ? 365 : 366;
    year++;
  }
  epoch -= days; // now it is days in this year, starting at 0
  days = 0;
  month = 0;
  byte monthLength = 0;
  for (month = 0; month < 12; month++) {
    if (month == 1) { // february
      if (year % 4) monthLength = 28;
      else monthLength = 29;
    }
    else monthLength = monthDays[month];
    if (epoch >= monthLength) epoch -= monthLength;
    else break;
  }
  month++;       // jan is month 1
  day = epoch + 1; // day of month

}
void gettime() {
  debug("Get time: ");
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  int cb = 0;
  int trys = 0;
  debug("sending NTP packet");
  while (!cb) {
    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
    // wait to see if a reply is available
    delay(2500);
    cb = udp.parsePacket();
    trys++;
    debug(".");
    if (trys >= 7) {
      break;
    }
  }
  debugln();
  //debugln(String(trys));

  if (!cb) {
    debugln("no packet yet");
    debugln(String(cb));
  }
  else {
    debug("packet received, length=");
    debugln(String(cb));
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    //unsigned long secsSince1900 = highWord << 16 | lowWord;
    secsSince1900 = highWord << 16 | lowWord;
    debug("Seconds since Jan 1 1900 = " );
    debugln(String(secsSince1900));

    // now convert NTP time into everyday time:
    debug("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    debugln(String(epoch));
    Unix_to_GMT(epoch);
    debug("The UTC time is ");// UTC is the time at Greenwich Meridian (GMT)
    Time1 = Time_to_String();
    debugln(Time1);
    // Serial.println(Time_to_String());


  }
  // wait ten seconds before asking for the time again
  //delay(10000);
}
void debug(String message) {
#ifdef SERIAL_DEBUG
  Serial.print(message);
#endif
}

void debugln(String message) {
#ifdef SERIAL_DEBUG
  Serial.println(message);
#endif
}
void debugln() {
#ifdef SERIAL_DEBUG
  Serial.println();
#endif
}

void buttonInterrupt() {
  buttonPushCounter++;
  buttonState = 1;
}

void serial_log() {
  debug("btn_state: ");
  debugln(String(buttonState));
  debug("default_temp_state: ");
  debugln(String(default_temp_state));
  debug("t_first_pause_state: ");
  debugln(String(t_first_pause_state));
  debug("t_second_pause_state: ");
  debugln(String(t_second_pause_state));
  debug("t_third_pause_state: ");
  debugln(String(t_third_pause_state));
  debug("t_mash_out_state: ");
  debugln(String(t_mash_out_state));
  debug("t_boil_state: ");
  debugln(String(t_boil_state));
  debug("button_counter: ");
  debugln(String(buttonPushCounter));
}

void beep(unsigned char delayms) {
  analogWrite(p_sound, 40);
  delay(delayms);
  analogWrite(p_sound, 20);
  delay(delayms);
  analogWrite(p_sound, 0);
  delay(delayms);
}
void light (unsigned char delayms) {
  digitalWrite (LED_BUILTIN, LOW);
  delay(delayms);

  digitalWrite (LED_BUILTIN, HIGH);
  delay(delayms);
}




// main function
void loop() {
  display.clear();

  if (millis() - last_time > period) {
    last_time = millis();
    gettime();      //wait approx. [period] ms
  }

  String msg_test;
  String disp_msg = "t:";
  float temp_0 = getTemperature();
  if (Time1 != NULL) {
    debugln("-- Running test ESP --");
    msg_test = String("BREW: ");
    msg_test += String(Time1);
    msg_test += " t:";
    msg_test += temp_0;
    //  sendIFTTTrequest_http(msg_test);
    disp_msg += temp_0;
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawStringMaxWidth(64, 5, 128, disp_msg);
    display.setFont(ArialMT_Plain_10);
    display.drawStringMaxWidth(64, 30, 128, Time1);
    display.display();
  }
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  //  sensors.requestTemperatures();
  display.drawLine (5, 44, 123, 43);
  // Read temperature as Celsius
  //  t1 = sensors.getTempCByIndex(0);
  t1 = temp_0;

  //  lcd.setCursor(0, 0); //  Set cursor 1 line 0 symbol
  //  lcd.print("Current"); // Output text
  //  lcd.setCursor(0, 1); // Set cursor 2 line 0 symbol
  //  lcd.print(sensors.getTempCByIndex(0), 1);

  // First pause
  if ((t1 >= t_first_pause - 1) && (t1 <= t_first_pause + 1)) {
    disp_msg = "First Pause : " + String(t_first_pause).substring(0, 2);
    if ((buttonState) || (t_first_pause_state)) {
      //  lcd.setCursor(8, 0); //  Set cursor 1 line 8 symbol
      //  lcd.print("Frst off"); // Output text
      disp_msg += " OFF";
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      t_first_pause_state = true;
    }
    if (!t_first_pause_state) {
      beep(80);
      //lcd.setCursor(8, 0); // Set cursor 1 line 8 symbol
      //lcd.print("First P "); // Output text
      //lcd.setCursor(9, 1); // // Set cursor 2 line 9 symbol
      //lcd.print(t_first_pause, 1); // Output temperature
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      if (secsSince1900 - last_time_message > period_message) {
        msg_test += " First Pause";
        debug("last_time_message:");
        debugln(String(last_time_message));
        debugln("Time to send message:");
        sendIFTTTrequest_http(msg_test);
        debugln(msg_test);
      }
      light(80);
    }
  }

  // Second pause
  if ((t1 >= t_second_pause - 1) && (t1 <= t_second_pause + 1)) {
    disp_msg = "Second Pause : " + String(t_second_pause).substring(0, 2);
    if ((buttonState) || (t_second_pause_state)) {
      disp_msg += " OFF";
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      t_second_pause_state = true;
    }
    if (!t_second_pause_state) {
      beep(80);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      if (secsSince1900 - last_time_message > period_message) {
        msg_test += " Second Pause";
        debug("last_time_message:");
        debugln(String(last_time_message));
        debugln("Time to send message:");
        sendIFTTTrequest_http(msg_test);
        debugln(msg_test);
      }
      light(80);

    }
  }
  // Third pause
  if ((t1 >= t_third_pause - 1) && (t1 <= t_third_pause + 1)) {
    disp_msg = "Third Pause : " + String(t_third_pause).substring(0, 2);
    if ((buttonState) || (t_third_pause_state)) {
      disp_msg += " OFF";
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      t_third_pause_state = true;
    }
    if (!t_third_pause_state) {
      beep(80);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      if (secsSince1900 - last_time_message > period_message) {
        msg_test += " Third Pause";
        debug("last_time_message:");
        debugln(String(last_time_message));
        debugln("Time to send message:");
        sendIFTTTrequest_http(msg_test);
        debugln(msg_test);
      }
      light(80);

    }
  }
  //  Mash out
  if ((t1 >= t_mash_out - 1) && (t1 <= t_mash_out + 1)) {
    disp_msg = "Mashout : " + String(t_mash_out).substring(0, 2);
    if ((buttonState) || (t_mash_out_state)) {
      disp_msg += " OFF";
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      t_mash_out_state = true;
    }

    if (!t_mash_out_state) {
      beep(80);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      if (secsSince1900 - last_time_message > period_message) {
        msg_test += " Mashout";
        debug("last_time_message:");
        debugln(String(last_time_message));
        debugln("Time to send message:");
        sendIFTTTrequest_http(msg_test);
        debugln(msg_test);
      }
      light(80);

    }
  }
  ////////  Boil
  if (t1 >= t_boil) {
    disp_msg = "Boiling : " + String(t_boil).substring(0, 2);
    if ((buttonState) || (t_boil_state)) {
      disp_msg += " OFF";
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      t_boil_state = true;
    }

    if (!t_boil_state) {
      beep(80);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      if (secsSince1900 - last_time_message > period_message) {
        msg_test += " Boil";
        debug("last_time_message:");
        debugln(String(last_time_message));
        debugln("Time to send message:");
        sendIFTTTrequest_http(msg_test);
        debugln(msg_test);
      }
      light(80);
    }
  }

  //////////////////////////////////////////
  if ((t1 >= default_temp - 1) && (t1 <= default_temp + 1)) {
    disp_msg = "Default : " + String(default_temp).substring(0, 2);
    if ((buttonState) || (default_temp_state)) {
      disp_msg += " OFF ";
      disp_msg += String(hour);
      disp_msg += ":";
      disp_msg += String(minute);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      default_temp_state = true;
    }
    if (!default_temp_state) {
      beep(80);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawStringMaxWidth(64, 47, 128, disp_msg);
      display.display();
      //debugln("def state");
      //current_time=secsSince1900
      if (secsSince1900 - last_time_message > period_message) {
        msg_test += " Default";
        debug("last_time_message:");
        debugln(String(last_time_message));
        debugln("Time to send message:");
        sendIFTTTrequest_http(msg_test);
        debugln(msg_test);
      }
      /*debug("secsSince1900:");
        debugln(String(secsSince1900));
        debug("last_time_message:");
        debugln(String(last_time_message));*/
      light(80);
    }
  }
  serial_log();
  // delay(500);
  /*  if (digitalRead(p_button)) {
      debugln("-- button 1 --");   //LED ON
    } else {
      debugln("-- button 2 --");;  //LED OFF
    }
  */
  buttonState = 0;

  delay(1000);    //Send a request to update every 10 sec (= 10,000 ms)
}