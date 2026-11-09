#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC,LCD_CS,LCD_SCK,LCD_MOSI,LCD_MISO,VSPI);
Arduino_GFX *gfx = new Arduino_ILI9341(bus,GFX_NOT_DEFINED,1,false);
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touch(TP_CS,TP_IRQ);
constexpr int W=320,H=240;
constexpr uint16_t BG=0x0842,PANEL=0x18C6,WHITE=0xFFFF,MUTED=0xA514,CYAN=0x07FF,GREEN=0x07E0,YELLOW=0xFFE0,ORANGE=0xFD20,PURPLE=0xA19F,RED=0xF800;
enum Page {HOME,CLOCK,WEATHER,HUD,ASSISTANT,CONNECTION,SETTINGS};
Page page=HOME;
unsigned long lastDraw=0,lastWeather=0,lastTouch=0;
float tempC=NAN,windKmh=NAN;
bool weatherOk=false;
int hudSpeed=0,hudLimit=0,hudDistance=0;
String hudTurn="--",hudAlert="NO ALERT",serialLine;

void txt(int x,int y,const String &s,uint16_t c=WHITE,int size=2){gfx->setTextColor(c);gfx->setTextSize(size);gfx->setCursor(x,y);gfx->print(s);}
void center(int y,const String &s,uint16_t c=WHITE,int size=2){txt(max(0,(W-(int)s.length()*6*size)/2),y,s,c,size);}
void header(const char *s){gfx->fillScreen(BG);gfx->fillRoundRect(5,5,310,32,8,PANEL);txt(13,14,s,CYAN);gfx->fillRoundRect(267,8,45,25,7,0x2945);txt(279,13,"<");}
void icon(int x,int y,int k){
 switch(k){
 case 0:gfx->drawCircle(x,y,13,WHITE);gfx->drawLine(x,y,x,y-8,WHITE);gfx->drawLine(x,y,x+7,y+3,WHITE);break;
 case 1:gfx->fillCircle(x-6,y+3,8,WHITE);gfx->fillCircle(x+5,y,10,WHITE);gfx->fillRoundRect(x-16,y,32,12,5,WHITE);break;
 case 2:gfx->fillTriangle(x-12,y-10,x+12,y,x-12,y+10,WHITE);break;
 case 3:gfx->drawRoundRect(x-13,y-10,26,20,7,WHITE);gfx->fillCircle(x-5,y-2,2,WHITE);gfx->fillCircle(x+5,y-2,2,WHITE);gfx->drawLine(x-5,y+5,x+5,y+5,WHITE);break;
 case 4:gfx->drawCircle(x,y,12,WHITE);gfx->drawCircle(x,y,5,WHITE);break;
 default:gfx->drawCircle(x,y,12,WHITE);gfx->drawLine(x-8,y-8,x+8,y+8,WHITE);gfx->drawLine(x+8,y-8,x-8,y+8,WHITE);
 }
}
void tile(int x,int y,const char *name,int kind,uint16_t color){
 gfx->fillRoundRect(x,y,94,78,13,PANEL);gfx->fillRoundRect(x+29,y+7,36,36,10,color);icon(x+47,y+25,kind);
 txt(x+(94-(int)strlen(name)*6)/2,y+54,name,WHITE,1);
}
void home(){
 gfx->fillScreen(BG);txt(12,9,"T-OS",CYAN);txt(75,13,"PREMIUM",MUTED,1);
 struct tm t;char b[12];
 if(getLocalTime(&t,5)){strftime(b,sizeof(b),"%H:%M",&t);txt(239,9,b);}
 else txt(248,9,"--:--",WHITE,1);
 tile(6,35,"CLOCK",0,0x247F);tile(113,35,"WEATHER",1,0x05B9);tile(220,35,"HUD",2,ORANGE);
 tile(6,123,"AI",3,PURPLE);tile(113,123,"CONNECT",4,0x04F0);tile(220,123,"SETTINGS",5,0x8410);
 txt(11,214,WiFi.status()==WL_CONNECTED?"WIFI CONNECTED":"WIFI OFFLINE",WiFi.status()==WL_CONNECTED?GREEN:RED,1);
 txt(264,214,"v0.3",MUTED,1);
}
void clockPage(){
 header("CLOCK");struct tm t;char b[40];
 if(getLocalTime(&t,5)){
 strftime(b,sizeof(b),"%H:%M:%S",&t);center(74,b,CYAN,4);
 strftime(b,sizeof(b),"%d/%m/%Y",&t);center(142,b);
 center(180,"VIETNAM UTC+7",MUTED,1);
 }else{center(90,"WAITING FOR NTP",YELLOW);center(130,"CHECK WIFI",MUTED);}
}
void fetchWeather(){
 lastWeather=millis();weatherOk=false;if(WiFi.status()!=WL_CONNECTED)return;
 HTTPClient http;
 String url="https://api.open-meteo.com/v1/forecast?latitude="+String(TOS_WEATHER_LAT,4)+"&longitude="+String(TOS_WEATHER_LON,4)+"&current=temperature_2m,wind_speed_10m";
 http.setTimeout(7000);
 if(!http.begin(url))return;
 if(http.GET()==200){
 String body=http.getString();
 auto val=[&](const char *key)->float{
 String token=String('"')+key+"\":";int p=body.indexOf(token);
 if(p<0)return NAN;p+=token.length();return body.substring(p).toFloat();
 };
 tempC=val("temperature_2m");windKmh=val("wind_speed_10m");weatherOk=!isnan(tempC);
 }
 http.end();
}
void weatherPage(){
 header("WEATHER");txt(12,50,TOS_CITY,CYAN);
 if(weatherOk){center(86,String(tempC,1)+" C",WHITE,4);center(158,"WIND "+String(windKmh,0)+" km/h",MUTED);}
 else{center(92,"NO WEATHER DATA",YELLOW);center(137,"CHECK WIFI",MUTED);}
}
void hudPage(){
 header("SMART HUD");txt(9,48,"SPEED",MUTED);txt(12,82,String(hudSpeed),GREEN,5);txt(19,145,"km/h",MUTED);
 gfx->drawRoundRect(225,48,83,70,12,RED);txt(239,58,"LIMIT",RED,1);txt(240,78,hudLimit?String(hudLimit):"--",WHITE,3);
 txt(183,134,hudTurn,CYAN);txt(182,160,String(hudDistance)+" m");gfx->fillRoundRect(8,193,304,35,7,PANEL);txt(17,203,hudAlert,YELLOW);
}
void assistantPage(){header("AI ASSISTANT");center(75,"AI BRIDGE",PURPLE,3);center(131,"NOT CONNECTED",MUTED,1);center(164,"PHONE / CLOUD REQUIRED",MUTED,1);}
void connectionPage(){
 header("CONNECTION");txt(13,59,"WIFI STATUS",CYAN);
 txt(13,94,WiFi.status()==WL_CONNECTED?"CONNECTED":"DISCONNECTED",WiFi.status()==WL_CONNECTED?GREEN:RED);
 if(WiFi.status()==WL_CONNECTED)txt(13,130,WiFi.localIP().toString());
 txt(13,181,"HUD: USB SERIAL DEMO",YELLOW,1);
}
void settingsPage(){
 header("SETTINGS");txt(14,57,"T-OS PREMIUM v0.3",CYAN);txt(14,101,"BOARD: ESP32-2432S028R",WHITE,1);
 txt(14,130,"DISPLAY: 320 x 240",WHITE,1);txt(14,159,"TOUCH: XPT2046",WHITE,1);txt(14,191,"RESET: HARDWARE BUTTON",MUTED,1);
}
void render(){
 switch(page){case HOME:home();break;case CLOCK:clockPage();break;case WEATHER:weatherPage();break;case HUD:hudPage();break;case ASSISTANT:assistantPage();break;case CONNECTION:connectionPage();break;case SETTINGS:settingsPage();break;}
}
void processHud(const String &s){
 if(!s.startsWith("HUD,"))return;
 int a=s.indexOf(',',4),b=s.indexOf(',',a+1),c=s.indexOf(',',b+1),d=s.indexOf(',',c+1);
 if(a<0||b<0||c<0||d<0)return;
 hudSpeed=constrain(s.substring(4,a).toInt(),0,250);hudLimit=constrain(s.substring(a+1,b).toInt(),0,150);
 hudDistance=constrain(s.substring(b+1,c).toInt(),0,99999);hudTurn=s.substring(c+1,d);hudAlert=s.substring(d+1);
 hudTurn.toUpperCase();hudAlert.toUpperCase();if(hudTurn.length()>10)hudTurn.remove(10);if(hudAlert.length()>23)hudAlert.remove(23);
 if(page==HUD)render();
}
void touchRead(){
 if(!touch.touched()||millis()-lastTouch<250)return;
 TS_Point p=touch.getPoint();int rawX=p.x,rawY=p.y;
 #if TOUCH_SWAP_XY
 int tmp=rawX;rawX=rawY;rawY=tmp;
 #endif
 int x=map(rawX,TOUCH_MIN_X,TOUCH_MAX_X,0,W-1),y=map(rawY,TOUCH_MIN_Y,TOUCH_MAX_Y,0,H-1);
 #if TOUCH_INVERT_X
 x=W-1-x;
 #endif
 #if TOUCH_INVERT_Y
 y=H-1-y;
 #endif
 x=constrain(x,0,W-1);y=constrain(y,0,H-1);lastTouch=millis();
 if(page!=HOME){if(x>258&&y<40){page=HOME;render();}return;}
 if(y>=35&&y<113){page=x<107?CLOCK:x<214?WEATHER:HUD;}
 else if(y>=123&&y<201){page=x<107?ASSISTANT:x<214?CONNECTION:SETTINGS;}
 render();
}
void setup(){
 Serial.begin(115200);pinMode(LCD_BL,OUTPUT);digitalWrite(LCD_BL,HIGH);
 gfx->begin(40000000);gfx->setRotation(1);gfx->fillScreen(BG);txt(32,88,"T-OS PREMIUM",CYAN,3);
 touchSPI.begin(TP_SCK,TP_MISO,TP_MOSI,TP_CS);touch.begin(touchSPI);touch.setRotation(1);
 WiFi.mode(WIFI_STA);if(strlen(TOS_WIFI_SSID))WiFi.begin(TOS_WIFI_SSID,TOS_WIFI_PASSWORD);
 configTzTime("ICT-7","pool.ntp.org","time.google.com");render();
}
void loop(){
 touchRead();
 while(Serial.available()){char c=(char)Serial.read();if(c=='\n'){processHud(serialLine);serialLine="";}else if(c!='\r'&&serialLine.length()<140)serialLine+=c;}
 unsigned long now=millis();
 if((lastWeather==0&&WiFi.status()==WL_CONNECTED)||now-lastWeather>900000UL){fetchWeather();if(page==WEATHER)render();}
 if(now-lastDraw>1000){lastDraw=now;if(page==CLOCK||page==HOME)render();}
 delay(10);
}
