#include <Arduino.h>
#include <Wire.h>
#include <stdio.h>
#include <stdbool.h>
#include <EasyButton.h>
#include <Arduino-MAX17055_Driver.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>


#define I2C_SDA 40  // Custom SDA pin
#define I2C_SCL 41
#define MCP4706 0x60
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define PWR_EN 4
#define BACK_BUT 3//3
#define UP_BUT 15
#define OKAY_BUT 16//16
#define DOWN_BUT 8   
#define BOOST_EN 9
//---------------------------------------------------------MENU--------------------------------------------------------------
const int NUM_MENU = 2;
const int NUM_MAN = 3;

char menu_1[NUM_MENU][12]={"Manual mode","Wi-fi mode"};

int sel_y_menu_1[2] = {26,46};
int sel_y_menu_2[3] = {23,36,49};
int sel_pos = 1;

enum MenuPage{
  MAIN_MENU,
  WIFI_MENU,
  MODE_MENU,
  STIM_MENU
};

MenuPage currentPage = MAIN_MENU;
bool nextPage = false;
bool lastPage = false;
int last_pos = 1;
int drawStatus = 0;

//--------------------------------------------------------STIMULATION--------------------------------------------------------
uint intensity = 0;
char intensity_print[20];
bool start_stop = false;

bool last_start_stop = start_stop;
int last_intensity = intensity;

bool positive_pulse = false;
bool negative_pulse = false;

//------------------------------------------------------BUTTONS----------------------------------------------------------
EasyButton UP_BUTTON(UP_BUT, 40, false, false);
EasyButton DOWN_BUTTON(DOWN_BUT, 40, false, false);
EasyButton OKAY_BUTTON(OKAY_BUT, 40, false, false);
EasyButton BACK_BUTTON(BACK_BUT, 40, false, false);
bool UP_held = false;
bool DOWN_held = false;
unsigned long lastRepeatTime = 0;
const unsigned long repeatInterval = 200;

//------------------------------------------------------BATTERY----------------------------------------------------------
unsigned long batteryTime = 0;
unsigned long checkInterval = 2000;
char batteryCharge[50];
MAX17055 BAT;
float SOC;

//-------------------------------------------------------WIFI------------------------------------------------------------
const char* ssid = "BIOMEDICINA_2";
const char* password = "biomedicina2020";

bool start_wifi_stim = 0;
int frequency_slider;
int intensity_slider;
int pulse_width_slider;
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object

AsyncWebSocket ws("/ws");

String message = "";
String sliderValue1 = "0";
String sliderValue2 = "0";
String sliderValue3 = "0";

JSONVar sliderValues;

//Get Slider Values
String getSliderValues(){
  sliderValues["sliderValue1"] = String(sliderValue1);
  sliderValues["sliderValue2"] = String(sliderValue2);
  sliderValues["sliderValue3"] = String(sliderValue3);

  String jsonString = JSON.stringify(sliderValues);
  return jsonString;
}
//----------------------------------------------------------------------------------------------------------------------------
void setup(void) {

  Serial.begin(115200);
  Wire.begin(40,41, 400000);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();

  UP_BUTTON.begin();
  DOWN_BUTTON.begin();
  OKAY_BUTTON.begin();
  BACK_BUTTON.begin();

  UP_BUTTON.onPressed(UPPressed);
  DOWN_BUTTON.onPressed(DOWNPressed);
  OKAY_BUTTON.onPressed(OKPressed);
  BACK_BUTTON.onPressed(BACKPressed);


  DOWN_BUTTON.onPressedFor(200, DOWNHeld);
  UP_BUTTON.onPressedFor(200, UPHeld);


  BACK_BUTTON.onPressedFor(2000, BACKHeld);

  setV(128);
  pinMode(PWR_EN, OUTPUT);
  pinMode(BOOST_EN, OUTPUT);
  digitalWrite(PWR_EN, HIGH);
  digitalWrite(BOOST_EN, LOW);

  BAT.setResistSensor(0.01);
  BAT.setCapacity(750);
  delay(10);
  SOC = BAT.getSOC();
  sprintf(batteryCharge,"BAT: %.0f%%",SOC);
  
}
void loop(void) {
  //int drawStatus = 0;
  unsigned long currentTime = millis();
  if ((currentTime-batteryTime)>checkInterval){  
    SOC = BAT.getSOC();
    Serial.println(SOC);
    sprintf(batteryCharge,"BAT: %.0f%%",SOC);
    batteryTime = currentTime;
    float TTE = BAT.getTimeToEmpty();
    Serial.print("Time to empty is: ");
    Serial.print(TTE, 4);
    Serial.println(" Hours \n\n");
    float current = BAT.getInstantaneousCurrent();
    Serial.print("Instantaneous Current is: ");
    Serial.print(current, 4);
    Serial.println(" mA \n\n");
  }

  readButtons();

  switch(currentPage){
//----------------------------------------------------------------------------------------MAIN MENU--------------------------------------------------------------
    case MAIN_MENU:
      readButtons();
      if (sel_pos > NUM_MENU) sel_pos = 1;
      if (sel_pos == 0) sel_pos = NUM_MENU;
      //display.clearDisplay();
      if (drawStatus == 0){
        display.clearDisplay();
        displayMessage(1, 1, batteryCharge);
        displayMessage(10, 30, menu_1[0]);
        displayMessage(10, 50, menu_1[1]);
        display.drawRect(0, sel_y_menu_1[sel_pos-1], 128, 15, SSD1306_WHITE);
        drawStatus = 1;
        display.display();
      }
      readButtons();
      if (last_pos != sel_pos){
        display.clearDisplay();
        displayMessage(1, 1, batteryCharge);
        displayMessage(10, 30, menu_1[0]);
        displayMessage(10, 50, menu_1[1]);
        display.drawRect(0, sel_y_menu_1[sel_pos-1], 128, 15, SSD1306_WHITE);
        last_pos = sel_pos;
        display.display();

      }
      if (nextPage == true && sel_pos-1 == 0)
      {
        currentPage = MODE_MENU;
        nextPage = false;
        lastPage = false;
        drawStatus = 0;
      }
      if (nextPage == true && sel_pos-1 == 1)
      {
        currentPage = WIFI_MENU;
        nextPage = false;
        lastPage = false;
        drawStatus = 0;
      }
    readButtons();
    break;
//----------------------------------------------------------------------------------------------WIFI MENU-----------------------------------------------------------------------------------
    case WIFI_MENU:

       if (drawStatus == 0){

        display.clearDisplay();
        displayMessage(1, 1, batteryCharge);
        displayMessage(10, 30, "Wi-fi loading");
        drawStatus = 1;
        display.display();
        initFS();
        initWiFi();
        initWebSocket();
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html"); ///EDITED
        });
        server.serveStatic("/", LittleFS, "/");

        // Start server
        server.begin();

        display.clearDisplay();
        displayMessage(1, 1, batteryCharge);
        displayMessage(10, 30, "Wi-fi working");
        drawStatus = 1;
        display.display();
        digitalWrite(BOOST_EN, HIGH);
      }
      if(start_wifi_stim)
      {
        digitalWrite(BOOST_EN, HIGH);
        stim_mode_wifi(frequency_slider, pulse_width_slider, intensity_slider);

      }
      readButtons();
      
      //ws.cleanupClients();
      if (last_pos != sel_pos){
        display.clearDisplay();
        displayMessage(1, 1, batteryCharge);
        displayMessage(10, 30, "Wi-fi engaged");
        last_pos = sel_pos;
        display.display();

      }
      if (lastPage == true)
      {
        shutdownWifi();
        digitalWrite(BOOST_EN, LOW);
        currentPage = MAIN_MENU;
        lastPage = false;
        nextPage = false;
        drawStatus = 0;

      }
      break;
//----------------------------------------------------------------------------------------------MODE MENU----------------------------------------------------------------
    case MODE_MENU:
      if (sel_pos > NUM_MAN) sel_pos = 1;
      if (sel_pos == 0) sel_pos = NUM_MAN;
      if (drawStatus == 0){
        display.clearDisplay();
        displayMessage(1, 1, batteryCharge);
        displayMessage(5, 12, "Select mode:");
        displayMessage(10, 25, "Mode 1");
        displayMessage(10, 38, "Mode 2");
        displayMessage(10, 51, "Mode 3");
        display.drawRect(0, sel_y_menu_2[sel_pos-1], 128, 13, SSD1306_WHITE);
        drawStatus = 1;
        display.display();
      }
      readButtons();
      if (last_pos != sel_pos){
        display.clearDisplay();
        displayMessage(1, 1, batteryCharge);
        displayMessage(5, 12, "Select mode:");
        displayMessage(10, 25, "Mode 1");
        displayMessage(10, 38, "Mode 2");
        displayMessage(10, 51, "Mode 3");
        display.drawRect(0, sel_y_menu_2[sel_pos-1], 128, 13, SSD1306_WHITE);
        last_pos = sel_pos;
        display.display();

      }
      if (lastPage == true)
      {
        currentPage = MAIN_MENU;
        lastPage = false;
        nextPage = false;
        drawStatus = 0;
        
      }
      if (nextPage == true && sel_pos-1 == 0)
      {
        currentPage = STIM_MENU;
        nextPage = false;
        lastPage = false;
        int mode_sel = sel_pos;
        intensity = 50;
        drawStatus = 0;
      }
    break;
//-------------------------------------------------------------------------------------------STIM MENU-------------------------------------------------------------------
  case STIM_MENU:
    if (intensity > 100) intensity = 100;
    if (intensity == 0) intensity = 1;
    //unsigned long currentTime = millis();
    if(UP_BUTTON.wasReleased()) UP_held = false;
    if(DOWN_BUTTON.wasReleased()) DOWN_held = false;
    if(UP_BUTTON.isReleased()) UP_held = false;
    if(DOWN_BUTTON.isReleased()) DOWN_held = false;
    
    if (UP_held && millis() - lastRepeatTime >= repeatInterval) {
      intensity++;
      lastRepeatTime = millis();
    }
    if (DOWN_held && millis() - lastRepeatTime >= repeatInterval) {
      intensity--;
      lastRepeatTime = millis();
    }
    sprintf(intensity_print,"Intensity:%d", intensity);
    if (drawStatus == 0){
        display.clearDisplay();
        displayMessage(1, 1, batteryCharge);
        displayMessage(5, 20, "STATUS: ");
        displayMessage(47, 20, "PAUSED");
        displayMessage(5, 33, intensity_print);
        drawStatus = 1;
        display.display();
      }
      readButtons();
      if (last_start_stop != start_stop || last_intensity != intensity){
        display.clearDisplay();
        displayMessage(1, 1, batteryCharge);
        displayMessage(5, 20, "STATUS:");
        if (start_stop == false) displayMessage(47, 20, "PAUSED");
        if (start_stop == true) displayMessage(47, 20, "STIMULATING");
        displayMessage(5, 33, intensity_print);
        display.display();
        last_start_stop = start_stop;
        last_intensity = intensity;

      } 
      
      if (start_stop==true && currentPage== STIM_MENU){
        stim_mode_1(intensity);
        digitalWrite(BOOST_EN,HIGH);
      }
      if (lastPage == true) {
          currentPage = MODE_MENU;
          lastPage = false;
          nextPage = false;
          start_stop = false;
          drawStatus = 0;
        }
      if (nextPage == true) {
        start_stop = !start_stop;
        Serial.println(start_stop);
        nextPage = false;
      }

  break;

  }
  if (start_stop == false) digitalWrite(BOOST_EN,LOW);

}

void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  else{
   Serial.println("LittleFS mounted successfully");
  }
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String sliderValues) {
  ws.textAll(sliderValues);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    if (message == "start") {
      start_wifi_stim = !start_wifi_stim;
      // TODO: Insert your stimulation-starting logic here
    }

    if (message.indexOf("1s") >= 0) {
      sliderValue1 = message.substring(2);
      frequency_slider = map(sliderValue1.toInt(), 0, 100, 1, 100);
      //Serial.println(dutyCycle1);
      Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }
    if (message.indexOf("2s") >= 0) {
      sliderValue2 = message.substring(2);
      pulse_width_slider = map(sliderValue2.toInt(), 0, 100, 10, 1000);
      //Serial.println(dutyCycle2);
      Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }    
    if (message.indexOf("3s") >= 0) {
      sliderValue3 = message.substring(2);
      intensity_slider = map(sliderValue3.toInt(), 0, 100, 1, 100);
      //Serial.println(dutyCycle3);
      Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }
    if (strcmp((char*)data, "getValues") == 0) {
      notifyClients(getSliderValues());
    }
  }
}
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void shutdownWifi() {
  // Disconnect WiFi
  WiFi.disconnect(true);  // Disconnect and erase WiFi credentials (optional)
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi turned off");

  // Close all WebSocket connections and end server
  ws.cleanupClients();
  Serial.println("WebSocket server closed");

  server.end();  // Stop the HTTP server
  Serial.println("HTTP server stopped");

  // Unmount LittleFS
  LittleFS.end();  // Cleanly unmount filesystem
  Serial.println("LittleFS unmounted");
}


void displayMessage(uint set_x, uint set_y, char message[50])
{
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(set_x, set_y);
    display.print(message);
}
void readButtons(){
  
    UP_BUTTON.read();
    DOWN_BUTTON.read();
    OKAY_BUTTON.read();
    BACK_BUTTON.read();

}
void setV(uint8_t Vout){
  Wire.beginTransmission(MCP4706);
  Wire.write(0);            // First Byte 0
  Wire.write(Vout);       // Second byte: Data bits          (D7.D6.D5.D4.D3.D2.D1.D0)
  Wire.endTransmission();
}
void UPPressed(){
  //display.clearDisplay();
  sel_pos++;
  intensity++;
  Serial.println("UP");
}
void DOWNPressed(){
  //display.clearDisplay();
  sel_pos--;
  intensity--;
  Serial.println("DOWN");
}

void OKPressed(){
  nextPage = true;
  Serial.println("OK");
}
void BACKHeld(){

  display.clearDisplay();
  displayMessage(5, 25, "SHUT DOWN");
  display.display();
  delay(200);
  digitalWrite(PWR_EN,LOW);
  
}

void BACKPressed(){
  lastPage = true;
  Serial.println("BACK");
}
void DOWNHeld(){
  DOWN_held = true;
  lastRepeatTime = millis();
  
}
void UPHeld(){
  UP_held = true;
  lastRepeatTime = millis();
    
}
void stim_mode_1(uint intensity){
  unsigned long periodMicros = 20000;
  unsigned long microsTime = micros();
  unsigned long pulseMicros = 600;
  static unsigned long startTime = 0;
  int pos_pulse = 128 + intensity;
  int neg_pulse = 128 - intensity;

  if (microsTime - startTime < pulseMicros) setV(pos_pulse); 
  else if (pulseMicros < microsTime - startTime && microsTime - startTime < 2 * pulseMicros) setV(neg_pulse);
  else  setV(128);

  if (microsTime - startTime >= periodMicros) startTime = microsTime;

}

void stim_mode_wifi(uint frequency, uint pulse_width, uint intensity){
  float period = 1000000.0f / frequency;
  unsigned long periodMicros = (unsigned long) round(period);
  unsigned long microsTime = micros();
  unsigned long pulseMicros = pulse_width;
  static unsigned long startTime = 0;
  //255 max +
  //0 max -
  int pos_pulse = 128 + intensity;
  int neg_pulse = 128 - intensity;



  if (microsTime - startTime < pulseMicros /*positive_pulse == false*/) {

    setV(pos_pulse);
    //positive_pulse = true;
    Serial.println("POS");

  } else if (pulseMicros < microsTime - startTime && microsTime - startTime < 2 * pulseMicros /*negative_pulse == false */) {
    
    setV(neg_pulse);
    //negative_pulse = true;
    Serial.println("NEG");

  } else {

    setV(128);
  }
  if (microsTime - startTime >= periodMicros){
    Serial.println("AG");
    startTime = microsTime;
    //positive_pulse = false;
    //negative_pulse = false;
    Serial.print("Frequency:");
    Serial.println(periodMicros);
    Serial.print("Pulse width:");
    Serial.println(pulse_width);
    Serial.print("Intensity");
    Serial.println(intensity);
  } 

}