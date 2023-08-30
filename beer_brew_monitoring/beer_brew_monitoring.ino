//
// Beer brew monitoring of temperature
//
// work ds18b20 + sound + temper pause
// added button reaction
//
//
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 2
/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
/********************************************************************/
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
// set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x3f, 16, 2);

const int  p_button = 4;    // the pin that the pushbutton is attached
const int p_sound = 11; //init pin for sound
const int p_light = 13; //init pin for light

float t1;
float default_temp = 27;
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

// Variables will change:
int buttonPushCounter = 0;   // counter for the number of button presses
int buttonState = 0;         // current state of the button
int lastButtonState = 0;     // previous state of the button

void setup()
{
  lcd.init();                      // initialize the lcd
  lcd.backlight();
  Serial.begin(9600);
  sensors.begin();
  //dht1.begin();
  //dht2.begin();
  pinMode(p_sound, OUTPUT);
  pinMode(p_light, OUTPUT);
  pinMode(p_button, INPUT);
  // init system
  beep(50);
  light(1000);
  beep(50);
  light(1000);
  beep(50);
  light(1000);
  delay(1000);

}

void loop()
{
  //delay(1000);
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures();

  // Read temperature as Celsius
  t1 = sensors.getTempCByIndex(0);

  lcd.setCursor(0, 0); //  Set cursor 1 line 0 symbol
  lcd.print("Current"); // Output text
  lcd.setCursor(0, 1); // Set cursor 2 line 0 symbol
  lcd.print(sensors.getTempCByIndex(0), 1);

  buttonState = digitalRead(p_button);  // read the pushbutton input pin

  // First pause
  if ((t1 >= t_first_pause - 1) && (t1 <= t_first_pause + 1)) {
    if (buttonState != lastButtonState) {
      if (buttonState == HIGH) {
        lcd.setCursor(8, 0); //  Set cursor 1 line 8 symbol
        lcd.print("Frst off"); // Output text
        t_first_pause_state = true;
      }
    } else {
      if (!t_first_pause_state) {
        beep(1000);
        lcd.setCursor(8, 0); // Set cursor 1 line 8 symbol
        lcd.print("First P "); // Output text
        lcd.setCursor(9, 1); // // Set cursor 2 line 9 symbol
        lcd.print(t_first_pause, 1); // Output temperature
        light(1000);
      }
    }
  }
  // Second pause
  if ((t1 >= t_second_pause - 1) && (t1 <= t_second_pause + 1)) {
    if (buttonState != lastButtonState) {
      if (buttonState == HIGH) {
        lcd.setCursor(8, 0); //  Set cursor 1 line 8 symbol
        lcd.print("Scnd off"); // Output text
        t_second_pause_state = true;
      }
    } else {
      if (!t_second_pause_state) {
        beep(1000);
        lcd.setCursor(8, 0); // Set cursor 1 line 8 symbol
        lcd.print("Second P"); // Output text
        lcd.setCursor(9, 1); // // Set cursor 2 line 9 symbol
        lcd.print(t_second_pause, 1); // Output temperature
        light(1000);
      }
    }
  }
  // Third pause
  if ((t1 >= t_third_pause - 1) && (t1 <= t_third_pause + 1)) {
    if (buttonState != lastButtonState) {
      if (buttonState == HIGH) {
        lcd.setCursor(8, 0); //  Set cursor 1 line 8 symbol
        lcd.print("Thrd off"); // Output text
        t_third_pause_state = true;
      }
    } else {
      if (!t_third_pause_state) {
        beep(1000);
        lcd.setCursor(8, 0); // Set cursor 1 line 8 symbol
        lcd.print("Third P "); // Output text
        lcd.setCursor(9, 1); // // Set cursor 2 line 9 symbol
        lcd.print(t_third_pause, 1); // Output temperature
        light(1000);
      }
    }
  }
  //  Mash out
  if ((t1 >= t_mash_out - 1) && (t1 <= t_mash_out + 1)) {
    if (buttonState != lastButtonState) {
      if (buttonState == HIGH) {
        lcd.setCursor(8, 0); //  Set cursor 1 line 8 symbol
        lcd.print("Mash off"); // Output text
        t_mash_out_state = true;
      }
    } else {
      if (!t_mash_out_state) {
        beep(1000);
        lcd.setCursor(8, 0); // Set cursor 1 line 8 symbol
        lcd.print("Mash Out"); // Output text
        lcd.setCursor(9, 1); // // Set cursor 2 line 9 symbol
        lcd.print(t_mash_out, 1); // Output temperature
        light(1000);
      }
    }
  }
  ////////  Boil
  if (t1 >= t_boil) {
    if (buttonState != lastButtonState) {
      if (buttonState == HIGH) {
        lcd.setCursor(8, 0); //  Set cursor 1 line 8 symbol
        lcd.print("Boil off"); // Output text
        t_boil_state = true;
      }
    } else {
      if (!t_boil_state) {
        beep(1000);
        lcd.setCursor(8, 0); // Set cursor 1 line 8 symbol
        lcd.print("Boiling"); // Output text
        lcd.setCursor(9, 1); // // Set cursor 2 line 9 symbol
        lcd.print(t_boil, 1); // Output temperature
        light(1000);
      }
    }
  }
  //////////////////////////////////////////
  if ((t1 >= default_temp - 1) && (t1 <= default_temp + 1)) {
    if (buttonState != lastButtonState) {
      if (buttonState == HIGH) {
        lcd.setCursor(8, 0); //  Set cursor 1 line 8 symbol
        lcd.print("Def: off"); // Output text
        default_temp_state = true;
      }
    } else {
      if (!default_temp_state) {
        beep(1000);
        lcd.setCursor(8, 0); //  Set cursor 1 line 8 symbol
        lcd.print("Def: on"); // Output text
        lcd.setCursor(12, 0); // Set cursor 2 line 9 symbol
        lcd.print(default_temp, 1); // Output temperature
        //light(1000);
      }
    }
  }
  lastButtonState = buttonState;
  //serial_log();


}
void serial_log() {
  Serial.print("btn_state: ");
  Serial.println(buttonState);
  Serial.print("last_state: ");
  Serial.println(lastButtonState);
  Serial.print("default_temp_state: ");
  Serial.println(default_temp_state);
  Serial.print("t_first_pause_state: ");
  Serial.println(t_first_pause_state);
  Serial.print("t_second_pause_state: ");
  Serial.println(t_second_pause_state);
  Serial.print("t_third_pause_state: ");
  Serial.println(t_third_pause_state);
  Serial.print("t_mash_out_state: ");
  Serial.println(t_mash_out_state);
  Serial.print("t_boil_state: ");
  Serial.println(t_boil_state);
}

void beep(unsigned char delayms) {
  analogWrite(p_sound, 20);
  delay(60);
  analogWrite(p_sound, 0);
  delay(60);
}
void light (unsigned char delayms) {
  digitalWrite(p_light, HIGH);
  delay(delayms);
  digitalWrite(p_light, LOW);
  delay(delayms);
}
