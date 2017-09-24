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
#include <Fonts/VeraSe60pt7b.h>
#include <Fonts/VeraSe42pt7b.h>
#include <Fonts/VeraSe18pt7b.h>
#include <Fonts/VeraSe10pt7b.h>
#include <dragon.h>
#include <WiFi.h>
#include <SPI.h>
#include <OneButton.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//define TFT pins for use with ESP32
#define TFT_DC 16   
#define TFT_CS 15
#define TFT_RST 5

//define button pins
#define SWTU 35// switch up is at pin 3
#define SWTD 34 // switch down is at pin 4

//WiFi AP details - fixed for now
const char* ssid = "YourSID";
const char* password = "YourPassword";

//define DHT temp sensor pin
#define ONE_WIRE_BUS 32

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//DeviceAddress dallasAddress;

#if defined(__SAM3X8E__)
#undef __FlashStringHelper::F(string_literal)
#define F(string_literal) string_literal
#endif

int heatActive = 33; //define relay pin - initially LED for testing.



//set millis and data collection interval variables.
unsigned long previousMillis = 0;
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

//Sketch version

char sVersion[] = "v0.01";

void setup() {

  Serial.begin(115200);

  tft.begin();

  // read diagnostics (optional but can help debug problems)
  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX);

  // begin Dallas sesnsor
  sensors.begin();
  //sensors.setResolution(dallasAddress, 9);

  tft.setRotation(3);  // 0 - Portrait, 1 - Lanscape
  tft.setTextWrap(true);

  bootSplash();
  delay(3000);
  
  initialiseWifi();
  delay(3000);
  
  displayLayout();
  setPointTxt(setPoint, NULL);
  localTempTxt(localTemp, NULL);

  pinMode(heatActive, OUTPUT);  // make led or pin3 an output

  buttonUp.attachClick(setPointUp);
  buttonDown.attachClick(setPointDown);

  Serial.print("Firmware version: ");
  Serial.println(sVersion);

  colData = 1000;

}

void loop() {

  buttonUp.tick();
  buttonDown.tick();
 
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= colData) {
    previousMillis = currentMillis;
    getTemp();
    updateDispTemp(localTemp, prevTemp);
    checkTempStatus(setPoint, localTemp);
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
  if ($g < $i) // if the temperature exceeds your chosen setting set font colour to RED and turn LED ON
    {
      digitalWrite (heatActive, 1); // turn on the led
      localTempTxt($g, NULL);
    }
    else if ($g >= $i) // if not then set the font colour to WHITE and turn LED OFF
    {
      digitalWrite (heatActive, 0); //turn LED OFF.
      localTempTxt($g, NULL);
    }
}

//function for display layout
unsigned long displayLayout() {
  //set background to black
  tft.fillScreen(ILI9341_BLACK);
  //draw big green circle left.
//  tft.fillCircle(70, 120, 120, ILI9341_GREEN);
//  tft.fillCircle(70, 120, 111, ILI9341_BLACK);
  //draw green circle top right
//  tft.fillCircle(300, 30, 120, ILI9341_GREEN);
//  tft.fillCircle(300, 30, 111, ILI9341_BLACK);

  tft.setCursor(5, 55);
  tft.setFont(&VeraSe10pt7b);
  tft.setTextSize(1);
  tft.setTextWrap(true);
  tft.setTextColor(ILI9341_MAGENTA);
  tft.print("Set Point: ");

  tft.drawCircle(133, 85, 4, ILI9341_CYAN);
  tft.setCursor(140,105);
  tft.setFont(&VeraSe18pt7b);
  tft.setTextSize(1);
  tft.setTextWrap(true);
  tft.setTextColor(ILI9341_CYAN);
  tft.print("C");

  tft.setCursor(200, 20);
  tft.setFont(&VeraSe10pt7b);
  tft.setTextSize(1);
  tft.setTextWrap(true);
  tft.setTextColor(ILI9341_MAGENTA);
  tft.print("Temp: ");

  tft.drawCircle(275, 105, 4, ILI9341_CYAN);
  tft.setCursor(282, 125);
  tft.setFont(&VeraSe18pt7b);
  tft.setTextSize(1);
  tft.setTextWrap(true);
  tft.setTextColor(ILI9341_CYAN);
  tft.print("C");
 
  }
//function to updat the set point test on the display
//$i = setPoint, $h = trigger
unsigned long setPointTxt(int $i, int $h){
  if($h == 1){
  //tft.fillRect(8, 101, 140, 95, ILI9341_BLACK);
  tft.fillRoundRect(6, 101, 142, 95, 10, ILI9341_BLACK);
  }

  if($i <= localTemp){
    tft.setTextColor(ILI9341_BLUE);
    //draw big green circle left.
    tft.drawCircle(70, 120, 120, ILI9341_GREEN);
    tft.drawCircle(70, 120, 119, ILI9341_GREEN);
    tft.drawCircle(70, 120, 118, ILI9341_GREEN);
    tft.drawCircle(70, 120, 117, ILI9341_GREEN);
    tft.drawCircle(70, 120, 116, ILI9341_GREEN);
    tft.drawCircle(70, 120, 115, ILI9341_GREEN);
    tft.drawCircle(70, 120, 114, ILI9341_GREEN);
    tft.drawCircle(70, 120, 113, ILI9341_GREEN);
    tft.drawCircle(70, 120, 112, ILI9341_GREEN);
    tft.drawCircle(70, 120, 111, ILI9341_GREEN);
    }else{
      tft.setTextColor(ILI9341_RED);
      //draw big green circle left.
    tft.drawCircle(70, 120, 120, ILI9341_RED);
    tft.drawCircle(70, 120, 119, ILI9341_RED);
    tft.drawCircle(70, 120, 118, ILI9341_RED);
    tft.drawCircle(70, 120, 117, ILI9341_RED);
    tft.drawCircle(70, 120, 116, ILI9341_RED);
    tft.drawCircle(70, 120, 115, ILI9341_RED);
    tft.drawCircle(70, 120, 114, ILI9341_RED);
    tft.drawCircle(70, 120, 113, ILI9341_RED);
    tft.drawCircle(70, 120, 112, ILI9341_RED);
    tft.drawCircle(70, 120, 111, ILI9341_RED);
      }
  
  tft.setFont(&VeraSe60pt7b);
  tft.setTextSize(1);
  
  tft.setCursor(3, 190);
  tft.println($i);
  }
//function to update local temp text
//$i = localTemp, $h = either 1 or NULL, 1 if temperature has changed orr NULL of no change
unsigned long localTempTxt(int $i, int $h){
  if ($h == 1){
  tft.fillRect(212, 28, 100, 67, ILI9341_BLACK);
  }
  //if heating is in off state circle is set to orange and text to orange
  //if heating is in on state circle is set to green and text to blue
  if($i < setPoint){
    tft.setTextColor(ILI9341_BLUE);
    tft.drawCircle(300, 30, 120, ILI9341_GREEN);
    tft.drawCircle(300, 30, 119, ILI9341_GREEN);
    tft.drawCircle(300, 30, 118, ILI9341_GREEN);
    tft.drawCircle(300, 30, 117, ILI9341_GREEN);
    tft.drawCircle(300, 30, 116, ILI9341_GREEN);
    tft.drawCircle(300, 30, 115, ILI9341_GREEN);
    tft.drawCircle(300, 30, 114, ILI9341_GREEN);
    tft.drawCircle(300, 30, 113, ILI9341_GREEN);
    tft.drawCircle(300, 30, 112, ILI9341_GREEN);
    tft.drawCircle(300, 30, 111, ILI9341_GREEN);
    }else{
      tft.setTextColor(0xFB40);
      tft.drawCircle(300, 30, 120, 0xFB40);
      tft.drawCircle(300, 30, 119, 0xFB40);
      tft.drawCircle(300, 30, 118, 0xFB40);
      tft.drawCircle(300, 30, 117, 0xFB40);
      tft.drawCircle(300, 30, 116, 0xFB40);
      tft.drawCircle(300, 30, 115, 0xFB40);
      tft.drawCircle(300, 30, 114, 0xFB40);
      tft.drawCircle(300, 30, 113, 0xFB40);
      tft.drawCircle(300, 30, 112, 0xFB40);
      tft.drawCircle(300, 30, 111, 0xFB40);
      }
  
  tft.setFont(&VeraSe42pt7b);
  tft.setTextSize(1);
  
  tft.setCursor(210, 90);
  tft.println($i);  
  }

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
    delay(500);
    Serial.print(".");
    tft.printf(".");
    }

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

//Functions for button presses

void setPointUp() {
      setPoint ++ ;  // add one to the setPoint, the setPoint is the ideal temperature for you
}

void setPointDown() {
      setPoint -- ;  // subtract one to the setPoint, the setPoint is the ideal temperature for you
}
