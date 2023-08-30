//
// Beer fermentation monitoring of temperature
//
// ------------------------

/*******************************************************************
    ds18 with deepsleep
    NTP
    http post to maker.ifttt.com
 *******************************************************************/

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 5 // DS18B20 on NodeMCU pin D4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

#define SERIAL_DEBUG // Uncomment this to dissable serial debugging

//------- temp bounds --------
const int temp_crit_high = 21;
const int temp_crit_low = 16;

//------- WiFi Settings -------
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASS";

//------- deep sleep ----------
const int sleepTimeS = 180; // def 600
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

// ------- ifttt config --------
//https://maker.ifttt.com/trigger/MY_IFTTT_EVENT_NAME/with/key/MY_IFTTT_API_KEY
#define IFTTT_API_KEY "MY_IFTTT_API_KEY"
#define IFTTT_EVENT_NAME "MY_IFTTT_EVENT_NAME"
const char *host = "maker.ifttt.com";
const int httpPort = 80;

WiFiClient client;
WiFiUDP udp;

String ipAddress = "";

//converter string to char ??
char *append_str(char *here, char *s) {
  while (*here++ = *s++)
    ;
  return here - 1;
}

void setup() {
#ifdef SERIAL_DEBUG
  delay(1000);
  Serial.begin(115200);
  delay(200);
  Serial.println();
#endif

  DS18B20.begin();
  debugln("Testing Dual Sensor data");

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Attempt to connect to Wifi network:
  debug("Connecting Wifi: ");
  debugln(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    debug(".");
    delay(500);
  }
  debugln("");
  debugln("WiFi connected");
  debug("IP address: ");
  IPAddress ip = WiFi.localIP();
  ipAddress = ip.toString();
  debugln(ipAddress);
  debugln("Starting UDP");
  udp.begin(localPort);
  debug("Local port: ");
  debugln(String(udp.localPort()));
  gettime();
  float temp_0 = getTemperature();
  debug("Temperature: ");
  debugln(String(temp_0));

  // TEST DEVICE
  String msg_test;  // this is for test device twice a day (8 and 20)
  // test if ESP is working twice per day
  if (Time1 != NULL) {
    // Send message at 8 and 20 hour during the day with any temperature
    if ((hour == 8) || (hour == 20))  {
      debugln("Hour equal 8 or 20");
      if ((minute >= 0) && (minute <= 20)) {
        debugln("-- Online test ESP --");
        msg_test = String("ESP: ");
        msg_test += String(Time1);
        msg_test += " t:";
        msg_test += temp_0;
        sendIFTTTrequest_http(msg_test);
        debugln (msg_test);
      }
    }
  }

  // TEST TEMPERATURE
  if (msg_test == NULL) {
    // check if was test message  send
    debugln("msg_test is NULL test");
    String msg_temp = String("Start ESP with Temp: ");  // this is for send critical temperature
    // Send message only if temp is critical and at working time (exclude night time)
    if ((hour > 7) || (hour < 1))  {
      if ((temp_0 >= temp_crit_high) || (temp_0 <= temp_crit_low)) {
        debugln("-- Temperature test ESP --");
        msg_temp += temp_0;
        sendIFTTTrequest_http(msg_temp);
        debugln (msg_temp);
      }
    }
  }

  delay(1000);
  debugln("closing connections");
  debugln("Sleep Mode");
//  ESP.deepSleep(sleepTimeS * 1000000);

}

void sendIFTTTrequest_http(String msg) {

  // connect to the Maker event server
  if (!client.connect(host, httpPort)) {
    debugln("!!! Connection failed !!!");
    return;
  }
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

  // finally we are ready to send the POST to the server!
  client.print(post_rqst);
  // Read all the lines of the reply from server and print them to Serial,
  // the connection will close when the server has sent all the data.
  while (client.connected()) {
    if (client.available()) {
      String line = client.readStringUntil('\r');
      debug("Reply: ");
      debugln(line);
    } else {
      // No data yet, wait a bit
      delay(50);
    };
  }
  client.stop();
  debugln (post_rqst);
}

float getTemperature() {
  float result;
  DS18B20.requestTemperatures();
  result = DS18B20.getTempCByIndex(0);
  return result;
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
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

String Time_to_String()
{
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
  /* //Uncommment if you need seconds
    k = second / 10;
    n = second % 10;
    Tm = Tm + String(k) + String(n);
    Tm = "10 05 2018 th"; */
  return (Tm);
}

void Unix_to_GMT(unsigned long epoch)
{

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
void gettime()
{
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
    unsigned long secsSince1900 = highWord << 16 | lowWord;
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
void loop() {
}
