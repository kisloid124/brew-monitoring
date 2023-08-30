//
// Beer fermentation monitoring of temperature
//
// ------------------------

/*******************************************************************
    ds18 with deepsleep
    worked 
    http post to maker.ifttt.com
 *******************************************************************/

#include <ArduinoJson.h>

#include <ESP8266WiFi.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 5 // DS18B20 on NodeMCU pin D4  
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

//------- temp bounds --------
const int temp_crit_high = 21;
const int temp_crit_low = 16;

//------- WiFi Settings -------
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASS";
//------- deep sleep in seconds ----------
const int sleepTimeS = 10;

// ------- ifttt config --------
//https://maker.ifttt.com/trigger/MY_IFTTT_EVENT_NAME/with/key/MY_IFTTT_API_KEY
#define IFTTT_API_KEY "MY_IFTTT_API_KEY"
#define IFTTT_EVENT_NAME "MY_IFTTT_EVENT_NAME"

WiFiClient client;

String ipAddress = "";

//converter string to char ??
char *append_str(char *here, char *s) {
  while (*here++ = *s++)
    ;
  return here - 1;
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  DS18B20.begin();
  Serial.println("Testing Dual Sensor data");

  // NOTE:
  // It is important to use interupts when making network calls in your sketch
  // if you just checked the status of te button in the loop you might
  // miss the button press.
  //  attachInterrupt(TELEGRAM_BUTTON_PIN, telegramButtonPressed, RISING);

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Attempt to connect to Wifi network:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  ipAddress = ip.toString();

  float temp_0 = getTemperature();
  String msg_temp = String("Start ESP with Temp: ");
  if ((temp_0 >= temp_crit_high) || (temp_0 <= temp_crit_low)) {
    msg_temp += temp_0;
    sendIFTTTrequest_http(msg_temp);
    // debug Serial.println(msg_temp);
  }

  delay(1000);
  Serial.println("closing connections");
  Serial.println("Sleep Mode");
  ESP.deepSleep(sleepTimeS * 1000000); 

}

void sendIFTTTrequest_http(String msg) {
  // create and convert to char messages (ssid, ip, temp)
  String message1 = "SSID: ";
  message1.concat(ssid);
  String message2 = "IP: ";
  message2.concat(ipAddress);
  String message3 = msg;
  char cmsg1[message1.length()]; char cmsg2[message2.length()]; char cmsg3[message3.length()];
  message1.toCharArray(cmsg1, message1.length());
  message2.toCharArray(cmsg2, message2.length());
  message3.toCharArray(cmsg3, message3.length());
  Serial.println(cmsg1);
  Serial.println(cmsg2);
  Serial.println(cmsg3);
  // connect to the Maker event server
  client.connect("maker.ifttt.com", 80);

  // construct the POST request
  char post_rqst[256];    // hand-calculated to be big enough

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
  client.stop();
  // debug Serial.println(post_rqst);
}



float getTemperature() {
  float result;
  DS18B20.requestTemperatures();
  result = DS18B20.getTempCByIndex(0);
  return result;
}

void loop() {
}
