/*
MIT License

Copyright (c) [2017] [SiniTronics]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>
#include "fonts/VeraSe24pt7b.h"
#include "fonts/VeraSe18pt7b.h"
#include "fonts/VeraSe30pt7b.h"
#include "fonts/VeraSe7pt7b.h"
#include <dragon.h>
#include <WiFi.h>
#include <SPI.h>
#include <OneButton.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "wifion.h"
#include "wifioff.h"
#include <NTPClient.h>
#include <PubSubClient.h>

//define network client 
static WiFiClient network;
//define TFT pins for use with ESP32
#define TFT_DC 16   
#define TFT_CS 15
#define TFT_RST 5

//define button pins
#define SWTU 34// switch up is at pin 3
#define SWTD 35 // switch down is at pin 4

//WiFi AP details - fixed for now
const char* ssid = "TheCity";
const char* password = "P1xelcat";

//define temp sensor pin
#define ONE_WIRE_BUS 32

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
//register WiFiUDP
WiFiUDP ntpUDP;
//register NTPClient
NTPClient timeClient(ntpUDP, "uk.pool.ntp.org", 3600);

#if defined(__SAM3X8E__)
#undef __FlashStringHelper::F(string_literal)
#define F(string_literal) string_literal
#endif

#define heatActive 33 //define led status light

//set millis and data collection interval variables.
unsigned long currentMillis = millis();
unsigned long previousMillis = 0;
unsigned long prevMills = 0;
uint32_t colData; 

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

//declare buttons
OneButton buttonUp(SWTU, true);
OneButton buttonDown(SWTD, true);

int prevTemp;
float getLocTemp = 0;
float localTemp = 0;
int setPoint = 20;
int setPointOld = 0;
int currentStatus = 54321;

//Sketch version
char sVersion[] = "v0.02";

void setup() {

  Serial.begin(115200);

  tft.begin();

  // read diagnostics (optional but can help debug problems)
/*  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX);
*/
  // begin Dallas sesnsor
  sensors.begin();

  tft.setRotation(3);  // 0 - Portrait, 1 - Landscape
  tft.setTextWrap(true);

  bootSplash();
  delay(500);
  
  if(ssid != ""){
    initialiseWifi();
    delay(3000);
    timeClient.begin();
  }else{
    noWiFi();
  }
  
  getTemp();
  Serial.print("Temp: ");
  Serial.println(localTemp);
  displayLayout();
  heatingStatus(currentStatus);
  setPointTxt(setPoint, NULL);
  localTempTxt(localTemp, NULL);

  pinMode(heatActive, OUTPUT);  // set active LED pin as output

  buttonUp.attachClick(setPointUp);
  buttonDown.attachClick(setPointDown);

  Serial.print("Firmware version: ");
  Serial.println(sVersion);
  //Interval to collect data.
  colData = 1000;
}

void loop() {
  
  buttonUp.tick();
  buttonDown.tick();

  currentMillis = millis();
 
  if (currentMillis - previousMillis >= colData) {
    previousMillis = currentMillis;
    timeClient.update();
    getTemp();
    displayTime();
    updateDispTemp(localTemp, prevTemp);
    checkTempStatus(setPoint, localTemp);
    checkWifiStatus();
  }

  //check set point value anf if change update display set point temp.
  if (setPoint != setPointOld) {
    int trigger = 1;
    setPointTxt(setPoint, trigger);
    
    setPointOld = setPoint;
  }

}

//function to read temp, update display and set localTemp
void getTemp(){
  sensors.requestTemperatures();
  float t1 = sensors.getTempCByIndex(0);  
  if(t1 != -127.00 && t1 != 85.00){
    localTemp = t1;
  } else{
    localTemp = prevTemp; 
  }
}
//function to update local temp on the display 
//$i = localTemp, $g = prevTemp
void updateDispTemp(int $i, int $g){
  if ($i != $g) {
    localTempTxt(localTemp, 1);
    prevTemp = localTemp;
  }
}
//function to check set pont temp against local temp and update heating status accordingly. 
//$i = setPoint, $g = localTemp
void checkTempStatus(int $i, int $g){
  if ($g <= $i - 1) // if the temperature exceeds your chosen setting set font colour to RED and turn LED ON
    {
      localTempTxt($g, NULL);
      setPointTxt($i, NULL);
      heatingStatus(1);
    }
    else if ($g >= $i + 1) // if not then set the font colour to WHITE and turn LED OFF
    {
      localTempTxt($g, NULL);
      setPointTxt($i, NULL);
      heatingStatus(0);
    }
}

void heatingStatus(int $i){
  if(currentStatus != $i){
    tft.fillRect(205, 25, 57, 31, ILI9341_DARKGREY);
    if($i != 1){
      tft.setTextColor(ILI9341_BLUE);
      tft.setCursor(50, 53);
      tft.setFont(&VeraSe18pt7b);
      tft.print("Heating Off");
      digitalWrite (heatActive, 0); //turn LED OFF.
      currentStatus = $i;
    } else {
      tft.setTextColor(ILI9341_GREEN);
      tft.setCursor(50, 53);
      tft.setFont(&VeraSe18pt7b);
      tft.print("Heating On");
      digitalWrite (heatActive, 1); // turn on the led
      currentStatus = $i;
    }
  }
  //Check is startup set status to Off
  if($i == 54321){
    tft.setTextColor(ILI9341_BLUE);
    tft.setCursor(50, 53);
    tft.setFont(&VeraSe18pt7b);
    tft.print("Heating Off");
    digitalWrite (heatActive, 0); //turn LED OFF.
    currentStatus = 0;
  }
}

//function for display layout
unsigned long displayLayout() {
  //set background to black
  tft.fillScreen(ILI9341_BLACK);
  //draw background boxes
  
  tft.fillRoundRect(5, 5, 310, 60, 5, 0x7BCF);      //status
  tft.fillRoundRect(5, 70, 152, 80, 5, ILI9341_DARKGREY);     //Set point
  tft.fillRoundRect(163, 70, 152, 80, 5, ILI9341_DARKGREY);   //temperature
  tft.fillRoundRect(5, 155, 152, 80, 5, ILI9341_DARKGREY);    //time
  tft.fillRoundRect(163, 155, 152, 80, 5, ILI9341_DARKGREY);  //others
  
  tft.setFont(&VeraSe7pt7b);
  tft.setTextSize(0);
  tft.setTextWrap(true);
  tft.setTextColor(ILI9341_WHITE);
  
  tft.setCursor(10, 20);
  tft.print("Status:");

  tft.setCursor(10, 85);
  tft.print("Set Point: ");

  tft.setCursor(169, 85);
  tft.print("Temp: ");

  tft.setCursor(10, 170);
  tft.print("Time:");  

  tft.drawCircle(114, 100, 4, ILI9341_CYAN);
  tft.setCursor(120,120);
  tft.setFont(&VeraSe18pt7b);
  tft.setTextSize(1);
  tft.setTextWrap(true);
  tft.setTextColor(ILI9341_CYAN);
  tft.print("C");

  tft.drawCircle(275, 100, 4, ILI9341_CYAN);
  tft.setCursor(282, 120);
  tft.setFont(&VeraSe18pt7b);
  tft.setTextSize(1);
  tft.setTextWrap(true);
  tft.setTextColor(ILI9341_CYAN);
  tft.print("C");
}
//function to update the set point test on the display
//$i = setPoint, $h = trigger
unsigned long setPointTxt(int $i, int $h){
  if($h == 1){
  tft.fillRect(37, 95, 72, 48, ILI9341_DARKGREY);
  }

  if($i <= localTemp){
    tft.setTextColor(ILI9341_BLUE);
    //tft.drawRGBBitmap(170, 190, radCold, 50, 50);
    }else{
      tft.setTextColor(ILI9341_RED);
      //tft.drawRGBBitmap(170, 190, radHot, 50, 50);
      }
  
  tft.setFont(&VeraSe30pt7b);
  tft.setCursor(35, 140);
  tft.println($i);
  }
//function to update local temp text
//$i = localTemp, $h = either 1 or NULL, 1 if temperature has changed or NULL of no change
unsigned long localTempTxt(int $i, int $h){
  if ($h == 1){
    tft.fillRect(191, 95, 72, 48, ILI9341_DARKGREY);
  }
  //if heating is in off state circle is set to orange and text to orange
  //if heating is in on state circle is set to green and text to blue
  if($i < setPoint){
    tft.setTextColor(ILI9341_BLUE);
    }else{
      tft.setTextColor(0xFB40);
      }
  
  tft.setFont(&VeraSe30pt7b);
  tft.setCursor(189, 140);
  tft.println($i);  
  }
//initial boot screen 
unsigned long bootSplash() {
  tft.fillScreen(ILI9341_BLACK);
  unsigned long start = micros();
  tft.setCursor(60,50);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.printf("ThermoStat: ");
  tft.printf(sVersion);

  tft.drawRGBBitmap(105, 85, dragonBitmap, DRAGON_WIDTH, DRAGON_HEIGHT);
  
  tft.setCursor(94, 180);
  tft.println("SiniTronics");
  return micros() - start;
}
//function to start wifi and connect to router
void initialiseWifi() {

  tft.fillScreen(ILI9341_BLACK);
  WiFi.begin(ssid, password);
  Serial.println("WiFi initialised");
  tft.setCursor(10, 10);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.println("Connecting to WiFi : ");
  tft.setCursor(10, 35);
  tft.setTextColor(ILI9341_BLUE);
  tft.println(ssid);
  tft.setCursor(10, 10);
  tft.println("");
  tft.println("");
  tft.println("");
  tft.println("");
  tft.setTextColor(ILI9341_WHITE);
  tft.printf("Connecting.");

  while (WiFi.status() != WL_CONNECTED) {
    tft.drawRGBBitmap(170, 35, wifiOff, WIFIOFF_WIDTH, WIFIOFF_HEIGHT);
    delay(500);
    Serial.print(".");
    tft.printf(".");
    }
    tft.drawRGBBitmap(170, 35, wifiOn, WIFION_WIDTH, WIFION_HEIGHT);
  Serial.println("");
  Serial.println("WiFi connected");
  tft.println("");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  tft.println("");
  tft.println("WiFi connected");
  delay(500);
  tft.println("");
  tft.println("IP address: ");
  delay(500);
  tft.println("");
  tft.setTextColor(ILI9341_GREEN);
  tft.println(WiFi.localIP());  
  }
//No WiFi print text to screen
void noWiFi(){
  tft.drawRGBBitmap(170, 190, wifiOff, WIFIOFF_WIDTH, WIFIOFF_HEIGHT);
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(10, 10);
  tft.setTextColor(ILI9341_WHITE);
  tft.println("WiFi not configured...");
  tft.setCursor(10, 35);
  tft.println("Skipping!");
  delay(500);
}
void checkWifiStatus(){
  if(WiFi.status() != WL_CONNECTED){
    tft.drawRGBBitmap(250, 180, wifiOff, WIFIOFF_WIDTH, WIFIOFF_HEIGHT);
  } else {
    tft.drawRGBBitmap(250, 180, wifiOn, WIFION_WIDTH, WIFION_HEIGHT);
  }
}
//Diaplay na update time from NTPClient
void displayTime(){
  if(millis() - prevMills >= 20000){
    tft.fillRect(16, 184, 134, 40, ILI9341_DARKGREY);
    tft.setCursor(15, 220);
    tft.setTextColor(ILI9341_WHITE);
    tft.setFont(&VeraSe24pt7b);
    tft.print(timeClient.getFormattedTime());
    prevMills = millis();
  }
}
//Functions for button presses
void setPointUp() {
      setPoint ++ ;  // add one to the setPoint, the setPoint is the ideal temperature for you
}

void setPointDown() {
      setPoint -- ;  // subtract one to the setPoint, the setPoint is the ideal temperature for you
}
