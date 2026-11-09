#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
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
constexpr uint16_t BG=0x0022,PANEL=0x10A4,WHITE=0xFFFF,MUTED=0x8C71,CYAN=0x05FF,GREEN=0x05E8,YELLOW=0xFE60,ORANGE=0xFBE0,PURPLE=0x91BF,RED=0xF9C5;
enum Page {HOME,CLOCK,WEATHER,HUD,ASSISTANT,CONNECTION,SETTINGS};
Page page=HOME;
unsigned long lastDraw=0,lastWeather=0,lastTouch=0;
String lastClockText="",lastHomeTime="";
float tempC=NAN,windKmh=NAN;
bool weatherOk=false;
int hudSpeed=0,hudLimit=0,hudDistance=0;
String hudTurn="--",hudAlert="NO ALERT",serialLine;
WebServer webServer(80);
Preferences prefs;
String wifiName,wifiPassword;
bool setupPortal=false;
const char* AP_NAME="TOS-SETUP";

void txt(int x,int y,const String &s,uint16_t c=WHITE,int size=2){gfx->setTextColor(c);gfx->setTextSize(size);gfx->setCursor(x,y);gfx->print(s);}
void center(int y,const String &s,uint16_t c=WHITE,int size=2){txt(max(0,(W-(int)s.length()*6*size)/2),y,s,c,size);}
void header(const char *s){
 gfx->fillScreen(BG);
 gfx->fillRect(0,0,320,3,ORANGE);
 txt(13,13,"T-OS",ORANGE,2);
 txt(82,17,s,WHITE,1);
 gfx->fillRoundRect(265,9,46,26,8,PANEL);
 txt(280,14,"<",WHITE,2);
 gfx->drawFastHLine(10,42,300,0x2945);
}
void icon(int x,int y,int k){
 switch(k){
 case 0:
 gfx->drawCircle(x,y,13,WHITE);gfx->drawCircle(x,y,11,WHITE);
 gfx->drawLine(x,y,x,y-8,WHITE);gfx->drawLine(x,y,x+7,y+4,WHITE);break;
 case 1:
 gfx->fillCircle(x-8,y-5,6,YELLOW);
 gfx->fillCircle(x-4,y+5,8,WHITE);gfx->fillCircle(x+7,y+2,10,WHITE);
 gfx->fillRoundRect(x-14,y+2,30,11,5,WHITE);break;
 case 2:
 gfx->fillTriangle(x-14,y+12,x+1,y-14,x+4,y+2,WHITE);
 gfx->fillTriangle(x+1,y-14,x+14,y+12,x+4,y+2,WHITE);break;
 case 3:
 gfx->drawRoundRect(x-14,y-11,28,22,7,WHITE);
 gfx->fillCircle(x-6,y-2,3,WHITE);gfx->fillCircle(x+6,y-2,3,WHITE);
 gfx->drawFastHLine(x-5,y+6,10,WHITE);break;
 case 4:
 gfx->drawCircle(x,y,12,WHITE);gfx->drawCircle(x,y,6,WHITE);
 gfx->fillCircle(x,y,3,WHITE);break;
 default:
 gfx->drawCircle(x,y,12,WHITE);
 gfx->drawLine(x-9,y-9,x+9,y+9,WHITE);
 gfx->drawLine(x+9,y-9,x-9,y+9,WHITE);
 }
}
void tile(int x,int y,const char *name,int kind,uint16_t color){
 gfx->fillRoundRect(x,y,96,77,12,PANEL);
 gfx->drawRoundRect(x,y,96,77,12,0x2945);
 gfx->fillRoundRect(x+28,y+7,40,40,11,color);
 icon(x+48,y+27,kind);
 txt(x+(96-(int)strlen(name)*6)/2,y+57,name,WHITE,1);
}
void updateHomeTime(){
 struct tm t;char b[12];String v="--:--";
 if(getLocalTime(&t,5)){strftime(b,sizeof(b),"%H:%M",&t);v=b;}
 if(v==lastHomeTime)return;
 gfx->fillRect(238,7,79,24,BG);txt(240,10,v,WHITE,2);lastHomeTime=v;
}
void home(){
 gfx->fillScreen(BG);
 gfx->fillRect(0,0,320,3,ORANGE);
 txt(12,10,"T-OS",ORANGE,2);
 txt(83,15,"CAR EDITION",MUTED,1);
 lastHomeTime="";updateHomeTime();
 tile(6,38,"CLOCK",0,0x24BF);
 tile(112,38,"WEATHER",1,0x05B9);
 tile(218,38,"SMART HUD",2,ORANGE);
 tile(6,124,"ASSISTANT",3,PURPLE);
 tile(112,124,"CONNECT",4,0x04F0);
 tile(218,124,"SETTINGS",5,0x8410);
 gfx->drawFastHLine(8,210,304,0x2945);
 txt(12,219,WiFi.status()==WL_CONNECTED?"WIFI ONLINE":"WIFI OFFLINE",WiFi.status()==WL_CONNECTED?GREEN:ORANGE,1);
 txt(263,219,"v0.5",MUTED,1);
}
void updateClockTime(){
 struct tm t;char b[40];String v="--:--:--";
 if(getLocalTime(&t,5)){strftime(b,sizeof(b),"%H:%M:%S",&t);v=b;}
 if(v==lastClockText)return;
 gfx->fillRect(0,68,320,55,BG);center(74,v,CYAN,4);lastClockText=v;
}
void clockPage(){
 header("CLOCK");struct tm t;char b[40];
 if(getLocalTime(&t,5)){
 lastClockText="";updateClockTime();
 strftime(b,sizeof(b),"%d/%m/%Y",&t);center(142,b);
 center(180,"VIETNAM UTC+7",MUTED,1);
 }else{lastClockText="";updateClockTime();center(145,"CHECK WIFI / NTP",MUTED,1);}
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
 header("DRIVE DASHBOARD");
 txt(14,55,"CURRENT SPEED",MUTED,1);
 gfx->fillRoundRect(9,72,175,111,14,PANEL);
 gfx->drawRoundRect(9,72,175,111,14,0x2B06);
 String sp=String(hudSpeed);
 gfx->fillRect(18,85,155,69,PANEL);
 txt(21,88,sp,GREEN,6);
 txt(115,158,"KM/H",MUTED,1);
 gfx->fillRoundRect(206,72,104,82,13,WHITE);
 gfx->drawRoundRect(209,75,98,76,13,RED);
 txt(230,82,"LIMIT",RED,1);
 String lim=hudLimit?String(hudLimit):"--";
 txt(231,101,lim,0x0000,4);
 txt(204,162,hudTurn,CYAN,2);
 txt(206,183,String(hudDistance)+" m",WHITE,2);
 gfx->fillRoundRect(8,211,304,26,7,PANEL);
 txt(16,218,hudAlert,YELLOW,1);
}
void assistantPage(){header("AI ASSISTANT");center(75,"AI BRIDGE",PURPLE,3);center(131,"NOT CONNECTED",MUTED,1);center(164,"PHONE / CLOUD REQUIRED",MUTED,1);}
void connectionPage(){
 header("CONNECTION");txt(13,59,"WIFI STATUS",CYAN);
 txt(13,94,WiFi.status()==WL_CONNECTED?"CONNECTED":"DISCONNECTED",WiFi.status()==WL_CONNECTED?GREEN:RED);
 if(WiFi.status()==WL_CONNECTED)txt(13,130,WiFi.localIP().toString());
 txt(13,170,setupPortal?"SETUP AP: TOS-SETUP":"HUD: SERIAL DEMO",YELLOW,1);
 if(setupPortal)txt(13,192,"OPEN 192.168.4.1",WHITE,1);
}
void settingsPage(){
 header("SETTINGS");txt(14,57,"T-OS CAR EDITION v0.5",CYAN);txt(14,101,"BOARD: ESP32-2432S028R",WHITE,1);
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
void beginNetwork(){
 prefs.begin("tos",false);
 wifiName=prefs.getString("ssid",TOS_WIFI_SSID);
 wifiPassword=prefs.getString("pass",TOS_WIFI_PASSWORD);
 WiFi.mode(WIFI_STA);
 if(wifiName.length()){
  WiFi.begin(wifiName.c_str(),wifiPassword.c_str());
  unsigned long start=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<8000)delay(50);
 }
 if(WiFi.status()!=WL_CONNECTED){
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_NAME);
  setupPortal=true;
 }
 webServer.on("/",HTTP_GET,[](){
  String page="<html><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{background:#08141e;color:white;font:18px sans-serif;padding:20px}input,button{padding:14px;margin:8px 0;width:95%;font-size:18px}</style><h2>T-OS Wi-Fi Setup</h2><form action='/save' method='POST'><input name='ssid' placeholder='Wi-Fi name' required maxlength='32'><input name='pass' placeholder='Wi-Fi password' type='password' maxlength='64'><button>Save and restart</button></form><p>Connect phone to TOS-SETUP, then open 192.168.4.1</p>";
  webServer.send(200,"text/html",page);
 });
 webServer.on("/save",HTTP_POST,[](){
  String ssid=webServer.arg("ssid"),pass=webServer.arg("pass");
  if(ssid.length()<1||ssid.length()>32||pass.length()>64){webServer.send(400,"text/plain","Invalid Wi-Fi credentials");return;}
  prefs.putString("ssid",ssid);prefs.putString("pass",pass);
  webServer.send(200,"text/plain","Saved. Restart ESP32 to connect.");
 });
 webServer.on("/hud",HTTP_POST,[](){
  if(webServer.arg("plain").length()>140){webServer.send(413,"text/plain","Too long");return;}
  processHud(webServer.arg("plain"));webServer.send(200,"text/plain","OK");
 });
 webServer.begin();
}
void setup(){
 Serial.begin(115200);pinMode(LCD_BL,OUTPUT);digitalWrite(LCD_BL,HIGH);
 gfx->begin(40000000);gfx->setRotation(1);gfx->fillScreen(BG);txt(32,88,"T-OS PREMIUM",CYAN,3);
 touchSPI.begin(TP_SCK,TP_MISO,TP_MOSI,TP_CS);touch.begin(touchSPI);touch.setRotation(1);
 beginNetwork();
 configTzTime("ICT-7","pool.ntp.org","time.google.com");render();
}
void loop(){
 webServer.handleClient();
 touchRead();
 while(Serial.available()){char c=(char)Serial.read();if(c=='\n'){processHud(serialLine);serialLine="";}else if(c!='\r'&&serialLine.length()<140)serialLine+=c;}
 unsigned long now=millis();
 if((lastWeather==0&&WiFi.status()==WL_CONNECTED)||now-lastWeather>900000UL){fetchWeather();if(page==WEATHER)render();}
 if(now-lastDraw>1000){lastDraw=now;if(page==CLOCK)updateClockTime();else if(page==HOME)updateHomeTime();}
 delay(10);
}
