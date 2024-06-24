#include <dummy.h>

#define vers "Version: 11.05.2024 tibber MAKE "
const char *ssid = "Heise-Gaeste"; 
const char *password = "NurFuerHeiseGaeste!";  
const char *token = "5K4MVS-OjfWhK_4yrjOlFe1F6kJXPVf7eQYggo8ebAE"; // Demo Token durch eigenen Token ersetzen!!!
const char *tibberApi = "https://api.tibber.com/v1-beta/gql";
//############### ePaper "Gedoens" ###########################################################
#define EPD_CS SS
#define MAX_DISPLAY_BUFFER_SIZE (81920ul-34000ul-5000ul) // ~34000 base use, change 5000 to your application use
#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
#define GxEPD2_DRIVER_CLASS GxEPD2_750_T7  // GDEW075T7   800x480, EK79655 (GD7965), (WFT0583CZ61)
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2))
//#if defined uwe #else #endif
#define ENABLE_GxEPD2_GFX 0  //wenn ohne Grauwerte
#include <GxEPD2_BW.h> 
#define GxEPD_LIGHTGREY GxEPD_BLACK
//Pins ePapaer an ESP8266: VCC=gr=3,3V, GND=br=GND, DIN=bl=Gpio13=D7=MOSI, CLK=ge=D5=Gpio14=SCLK, CS=or=D8=Gpio15=CS, DC=gn=D3=Gpio0, RST=ws=D4=Gpio2, BUSY=vi=D6=Gpio12=MISO, PWR=rt=3,3V
GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=D8*/ EPD_CS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4));
#include <Fonts/FreeMonoBold9pt7b.h>
//############### ePaper "Gedoens" Ende ######################################################
#include <ESP8266WiFi.h> 
#include <ESP8266HTTPClient.h> 
#include <WiFiClientSecureBearSSL.h> 
#include <ArduinoJson.h>  
#include <time.h>
#include "ota.h" 
#include "Internetzeit.h"
#define ePaper_on 5 //D1 = GPIo5
#define LED 2 //D4
#define AN LOW
#define AUS HIGH
#define RTCMEMORYSTART 65 //1.freier Speicherplatz im RTC
ADC_MODE(ADC_VCC);
char buf[200]; // Buffer für sprintf
int preis[2][24] ; //24h = 24 Preise, jeweils für den aktuellen und den folgenden Tag
int min1[2]={0,0}, max1[2]={0,0}; //jeweils für den aktuellen und den folgenden Tag
long preismittel[2]={0, 0}; //Mittelwert, jeweils für den aktuellen und den folgenden Tag
long mittelwert[2]={0,0};   //hier jetzt der Mittelwert von 13-24 Uhr des aktuellen Tages und 0-23 Uhr des folgenden Tages fuer epaper-Darstellung
int mini=30000, maxi=0, min_abgerundet, max_aufgerundet, spreizung; //fuer ePaper Darstellung
WiFiClientSecure client;  
boolean wlan_ok=false;
uint32_t bootcount, deepsleeptime; //deepsleeptime knapp unter 2hoch32, daher 22 x booten, Bootcount wird in der rtc des ESP gespeichert
unsigned long zeit_bis13; //hier wird die Zahl der Sekunden bis zum naechsten 13 Uhr gespeichert
StaticJsonDocument<1501> doc; //Speicher für die json-Daten vor dem Deserialisieren
//************************************************************
void setup() {
  unsigned long t=micros();
  Serial.begin(115200);
  Serial.print(vers);Serial.print("\nWaking up..., ESP.getResetReason: "); Serial.println(ESP.getResetReason().c_str());
  // die deepsleep Zeit wir in Mikrosekunden angegeben und beträgt maximal 2hoch32, also etwas mehr als 4 Milliarden, was ca. 70 Minuten entspricht
  // Der ESP soll aber 24h schlafen, also sorgt der nachfolgende Algorythmus dafür das der ESP nach knapp 70 Minuten kurz aufwacht 
  // (dauert ca 1,5 millisek.), den bootcounter erhöht und im nichtfluechtigem rtc Speicher ablegt und wieder schlafen geht. 
  // Ob das System aus dem deepsleep kommt oderneu eingeschaltet wurde, wird über ESP.getReason ermittelt.
  strcpy(buf,ESP.getResetReason().c_str());
  if(buf[0]=='D') { //D steht für DeepSleeep, weitere Faelle waeren z.B. 'P'ower on oder 'S'oftware watchdog . . . 
    ESP.rtcUserMemoryRead(RTCMEMORYSTART, &bootcount, sizeof(bootcount)); //Counter aus dem ESP RTC (RealTimeClock) lesen
    if(bootcount<21) {
      bootcount++;
      ESP.rtcUserMemoryWrite(RTCMEMORYSTART, &bootcount, sizeof(bootcount)); //Serial.print("bootcount = "); Serial.println(bootcount); 
      ESP.rtcUserMemoryRead(RTCMEMORYSTART+1, &deepsleeptime, sizeof(deepsleeptime));
      Serial.print(bootcount); Serial.print(" =bootcount, deepsleeptime= ");Serial.println(deepsleeptime);
      if(bootcount==21) {
        ESP.deepSleep(deepsleeptime, WAKE_RFCAL); delay(100); // nach dem Aufwachen WLAN ein
      }
      ESP.deepSleep(deepsleeptime, WAKE_RF_DISABLED); delay(100); //nach dem Aufwachen WLAN aus
    }
  }
  bootcount=0; //kein Deep Sleep, sondern echter Neustart oder es ist kurz nach 13:oo:oo Uhr
  ESP.rtcUserMemoryWrite(RTCMEMORYSTART, &bootcount, sizeof(bootcount));
  pinMode(ePaper_on, OUTPUT);
  digitalWrite(ePaper_on, HIGH); 
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");  // Wait for WiFi connection
  for(int timeout=20; timeout >=0; timeout--) { //max 10 Sekunden auf WLAN Verbindung warten
    if (WiFi.status() == WL_CONNECTED) {
      wlan_ok=true;
      break;
    }
    delay(500);
    Serial.print(".");
  }  
  sprintf(buf,"\nconnected, address=%s; Hostname=%s, Version= %s\r\n",WiFi.localIP().toString().c_str(),WiFi.hostname().c_str(),vers);  Serial.print(buf);
  //############### Berechne deep sleep time ######################
  if(wlan_ok==true) {
    showTime(); showTime(); // Internetzeit holen, beim 1. Aufruf sind die Sekunden falsch, daher 2x
    if(tm.tm_hour>=13) zeit_bis13=(12+24-tm.tm_hour)*3600; // der Rest bis zur 13. Stunde wird mit Minuten und Sekunden aufgefüllt
    else              zeit_bis13=(12   -tm.tm_hour)*3600;
    zeit_bis13=zeit_bis13+(59-tm.tm_min)*60;
    zeit_bis13=zeit_bis13+(59-tm.tm_sec)+30*60;  //30*60 = 30 Minuten zusätzlich wegen ggf. Ungenauigkeit um sicher nach 13.oo Uhr zu "landen"
    zeit_bis13=(zeit_bis13*10000+21)/22; //1. Aufrunden und 2. durch 22 Zyklen teilen damit das spätere Ergebnis sicher in 32 Bit passt
    deepsleeptime = zeit_bis13*100; // deepsleep Zeit in Mikrosekunden
    //############### Berechne deep sleep time ENDE #################
    sprintf(buf,"Zeit: %s, %02d.%02d.%04d %02d:%02d:%02d, bootcount: %d deepsleeptime: ",wochentag[tm.tm_wday],tm.tm_mday,tm.tm_mon,tm.tm_year,tm.tm_hour,tm.tm_min,tm.tm_sec, bootcount);
    Serial.print(buf);Serial.println(deepsleeptime); //sprintf kann keine unsigned long ausgeben, daher Serial.print
    ESP.rtcUserMemoryWrite(RTCMEMORYSTART+1, &deepsleeptime, sizeof(deepsleeptime));
    // jetzt das eigentliche Programm
    hole_tibber_preise();
    WiFi.mode(WIFI_OFF); //WLAN aus um Strom zu sparen
    epaper_ermittele_minmax(); //fuer den ersten Tag erst ab 13.oo Uhr
  }
  else deepsleeptime=(3600*1000/22)*1000*10; //1h = 3600 Sek.*10 =10h in mikrosekunden neu versuchen;
  display.init(115200, true, 2, false); // USE THIS for epaper Waveshare boards with "clever" reset circuit, 2ms reset pulse
  sprintf(buf,"\nePaper Display mit WIDTH: %d, Hoehe: %d\n", display.epd2.WIDTH, display.epd2.HEIGHT); Serial.print(buf);
  epaper_ausgabe();
  for(int cnt=0;cnt<6;cnt++) {delay(1000); Serial.print(".");}
  //delay(20000);
  Serial.println(" gehe in deep Sleep");
  ESP.deepSleep(deepsleeptime, WAKE_RF_DISABLED); delay(100);
}   
//************************************************************
void loop() {}
//************************************************************
void epaper_ermittele_minmax() {
  for(int h=13;h<24;h++) { //min, max, mittelw Werte für den ersten Tag ermitteln, 13-23 Uhr
    if(preis[0][h]<mini) mini=preis[0][h];
    if(preis[0][h]>maxi) maxi=preis[0][h];
    mittelwert[0] = mittelwert[0] + preis[0][h]; 
  }
  for(int h=0;h<24;h++) { //min, max, mittelw Werte für den zweiten Tag ermitteln, 0-23 Uhr
    if(preis[1][h]<mini) mini=preis[1][h];
    if(preis[1][h]>maxi) maxi=preis[1][h];
    mittelwert[1] = mittelwert[1] + preis[1][h]; 
  }
  mittelwert[0]=mittelwert[0]/11; // Tag1 von 13-23 Uhr
  mittelwert[1]=mittelwert[1]/24; // Tag2 von 0-23 Uhr
  min_abgerundet =(mini/100-2)*100; //auf voll 100  abgerundet -200
  max_aufgerundet=(maxi/100+1)*100; //auf voll 100 aufgerundet +100
  spreizung=max_aufgerundet - min_abgerundet; // Schwankungsbreite
  sprintf(buf,"Mittelwert0: %4d, Mittelwert1: %4d, Minimum: %4d, Maximum: %4d, min_abgerundet: %4d, max_aufgerundet: %4d, spreizung: %4d\n", 
               mittelwert[0], mittelwert[1], mini, maxi, min_abgerundet, max_aufgerundet, spreizung); Serial.print(buf);
}
//*****************************************************************************
void epaper_ausgabe() {
  display.setRotation(2);
  display.setFont(&FreeMonoBold9pt7b); //alternativ: display.setFont(0);
  display.fillScreen(GxEPD_WHITE); 
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
//####### Balken zeichnen ############################################
    int d=0, p=0;
    for(int h=13;h<48;h++) { 
      if( h>23 && d==0) {
        d=1; 
        p=24;
      }
      int balkenhoehe= long (-380* ( (preis[d][h-p] - min_abgerundet)*100 / spreizung)/100) ; //max. 380 Pixel hoch
      int hh=(h-13)*20;
      display.fillRect(/*x0*/90+hh, /*y=*/display.height() - 50, 8, balkenhoehe, GxEPD_LIGHTGREY);
      //sprintf(buf,"h: %d, d: %d, p: %d, preis[d][h-p]: %d, Balkenhoehe in Pixel: %d, Balkenhoehe_Proz.: %d\n", h, d, p, preis[d][h-p], balkenhoehe, ((preis[d][h-p] - min_abgerundet)*100) / spreizung ); Serial.print(buf);
    }
//####### Balken zeichnen Ende #######################################
//####### Y-Achse schreiben  #########################################
    int unterteilung=6;
    int delta_spreizung=spreizung/(unterteilung)/100; // auf ganze Cent abgerundet 
    int y_achse=0;
    while(true) {
      int y_abstand = display.height() - (50+ (y_achse* 380 * delta_spreizung /(spreizung/100)));
      //int y_abstand = display.height() - 1*(50+ y_achse* 380/(unterteilung));
      display.drawLine(/*x=*/80, y_abstand,display.width()-20 , y_abstand,GxEPD_BLACK);
      display.setCursor(15 , y_abstand); 
      display.print(delta_spreizung * y_achse + min_abgerundet/100) ;
      //sprintf(buf, "delta_spreizung: %d, y_achse: %d, y_abstand absolut in Pixel: %d\n", delta_spreizung, y_achse, y_abstand); Serial.print(buf);
      if( y_abstand < (display.height()-(380+0)) ) break;
      y_achse++;
      if(y_achse >10) break; //Notbremse
    }
    display.setCursor(13 , display.height() -465); //Y-Achse beschriften  
    display.print("Cent"); 
    display.setCursor(13 , display.height() -451); //Y-Achse beschriften  
    display.print("/kWh"); 
//####### Y-Achse schreiben Ende######################################
//####### Version, Spannung, Feldstärke, letzte Aktualisierung #######
    display.setFont(0);
    display.setCursor(15,473);
    sprintf(buf,"%s, RSSI: %d, Spannung [mV]: %d, letzte Aktualisierung: %s, %02d.%02d.%04d %02d:%02d:%02d",
              vers, wifi_station_get_rssi(),ESP.getVcc(),wochentag[tm.tm_wday],tm.tm_mday,tm.tm_mon,tm.tm_year,tm.tm_hour,tm.tm_min,tm.tm_sec);
    display.print(buf);
    display.setFont(&FreeMonoBold9pt7b);
//####### Version, Spannung, Feldstärke, letzte Aktualisierung Ende ##
//####### X-Achse schreiben ##########################################
    int yyy=display.height() -35;
    for(int yt=0;yt<18;yt++) {  //X-Achse beschriften  
      int ytt=yt*40;
      int h=13+2*yt;
      if(h>23) h=h-24;
      int sp=0; if(h<10) sp=6;
      display.setCursor(82 + ytt+sp, yyy);
      display.print(h); 
    }
    sprintf(buf,"%s,%2d.%02d.%04d          Stunde          ",wochentag[tm.tm_wday],tm.tm_mday,tm.tm_mon,tm.tm_year);
    now = (now + 86400);localtime_r(&now, &tm);  tm.tm_year += 1900; tm.tm_mon += 1; //einen Tag (86400 Sekunden) vor für den naechsten Tag, wird mit Aufruf von showtime() in loop wieder richtig gesetzt
    sprintf(buf+strlen(buf),"%s,%2d.%02d.%04d",wochentag[tm.tm_wday],tm.tm_mday,tm.tm_mon,tm.tm_year);
    display.setCursor(135, display.height() -16);
    display.print(buf);
    display.setCursor(260, 20); //display.setCursor(300, 20);
    display.setTextSize(2);
    display.print("Preisrahmen");
    display.setTextSize(1);
    display.setCursor(520, 18);
    display.print("(Tibber)");
//####### X-Achse schreiben Ende #####################################
//####### Mittelwert Linien ##########################################
    //int mw0 = display.height() - 50 - (380 * (mittelwert[0] - min_abgerundet)) / spreizung ; //Mittelwert von 13-23Uhr
    int mw0 = display.height() - 50 - (380 * (preismittel[0] - min_abgerundet)) / spreizung ;  //Mittelwert von 0 - 23Uhr
    int mw1 = display.height() - 50 - (380 * (mittelwert[1] - min_abgerundet)) / spreizung ;
    for(p=0;p<11*20;p+=8) display.fillRect(/*x0*/80+p, /*y=*/mw0, /*Laenge*/2, /*Breite*/3, GxEPD_LIGHTGREY); //gestrichelte "dicke" Linie
    for(p=0;p<24*20;p+=8) display.fillRect(/*x0*/80+11*20+5+p, /*y=*/mw1, /*Laenge*/2, /*Breite*/3, GxEPD_LIGHTGREY);//gestrichelte "dicke" Linie
//####### Mittelwert Linien Ende #####################################
//####### Fehlerbehandlung ###########################################
    if(wlan_ok==false) {
      display.setCursor(260, 50); 
      display.setTextSize(2);
      display.print("K E I N   W L A N");
      display.setTextSize(1);
    }
    if(ESP.getVcc()<2700) {
      display.setCursor(260, 80); 
      display.print("Batterie unter 2,7 Volt, WECHSELN");
    }
//####### Fehlerbehandlung Ende ######################################
  }
  while (display.nextPage());
  Serial.println("ePaper schreiben Ende");
}
//*****************************************************************************************
void hole_tibber_preise() {
  WiFiClientSecure client; //HTTPS !!!
  client.setInsecure(); //the magic line, use with caution
  HTTPClient https;
  strcpy(buf, "Bearer ");
  strcat(buf, token);
  https.begin(client, tibberApi);
  https.addHeader("Content-Type", "application/json");  // add necessary headers
  https.addHeader("Authorization",  buf);           // add necessary headers
  String payload = "{\"query\": \"{viewer { homes { currentSubscription{ priceInfo{ today{ total  } tomorrow { total  }}}}}}\"} ";
  //char payload[100]; strcpy( payload, "{\"query\": \"{viewer { homes { currentSubscription{ priceInfo{ today{ total  } tomorrow { total  }}}}}}\"} " );
  Serial.print("STRING PAYLOAD: "); Serial.println(payload);
  int httpCode = https.POST(payload);
  if (httpCode == HTTP_CODE_OK) {
    String response = https.getString();
   // strcpy(response, https.getString() );
    Serial.println(response);
    DeserializationError error = deserializeJson(doc, response);
    if (error) Serial.println("\n\n################################# Error parsing JSON response ###########################\n");
    preise_aus_json();
  } else {
    Serial.println("something went wrong");
    Serial.println(httpCode);
  }
  https.end();
  client.stop();  // Disconnect from the server
}
//*****************************************************************************************
void preise_aus_json() { //Preise aus JSON STring extrahieren und in kompakter Tabelle ausgeben
  char buf[200]; 
  Serial.println("preise_aus_json    :");
  char tag[2][9]={"today", "tomorrow"};
  double preis_in_doublefloat;
  for(int to=0;to<2;to++) {
    for(int h=0;h<24;h++) {
      preis_in_doublefloat = doc["data"]["viewer"]["homes"][0]["currentSubscription"]["priceInfo"][tag[to]][h]["total"];
      preis[to][h] = int (10000*preis_in_doublefloat);
      sprintf(buf,"Tag= %9s, to=%2d, h=%2d, Preis= %4d\n", tag[to], to, h, preis[to][h]); Serial.print (buf);
      int h1; if(h==23) h1=0; else h1=h;
      if(h>0) {
        if(preis[to][ min1[to] ]>preis[to][h1])  min1[to]=h1;
        if(preis[to][ max1[to] ]<preis[to][h1])  max1[to]=h1;
      }
      preismittel[to] =preis[to][h] + preismittel[to]; //mittelwert berechnen Teil 1
    }
    preismittel[to] = preismittel[to] / 24;  //mittelwert berechnen Teil 2    
  }
  Serial.println("\n            Heute        ||||             Morgen"); // in kompakter Tabelle ausgeben
  for(int h1=0;h1<12;h1++) {
    sprintf(buf,"h=%2d: %4d || h=%2d: %4d |||| h=%2d: %4d || h=%2d: %4d ||\n", h1, preis[0][h1], h1+12, preis[0][h1+12], h1, preis[1][h1], h1+12, preis[1][h1+12]); Serial.print (buf);
  }
  sprintf(buf,            " Min h: %2d, %4d,        |||| Min h: %2d, %4d, \n", min1[0], preis[0][min1[0]],min1[1], preis[1][min1[1]]);
  sprintf(buf+strlen(buf)," Max h: %2d, %4d,        |||| Max h: %2d, %4d, \n",max1[0], preis[0][max1[0]],max1[1], preis[1][max1[1]]);
  sprintf(buf+strlen(buf),"Mittelwert: %4d         |||| Mittelwert: %4d\n\n",preismittel[0], preismittel[1]); Serial.print (buf);
  sprintf(buf,"   <h:%2d, >h:%2d, =:%4d  ||||   <h:%2d, >h:%2d, =:%4d \n", min1[0], max1[0], preismittel[0], min1[1], max1[1] ,preismittel[1]); Serial.print (buf);
}
//*****************************************************************************************
//                     0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15   16   17   18   19   20   21   22   23    Uhr
//int preis[2][24]={{2703,2685,2686,2674,2678,2687,2875,3020,2946,2774,2574,2508,2148,1939,2177,2368,2552,2745,2885,2840,2739,2661,2663,2601},//Preise zu Testzwecken vom 8.+9.3.2024
//                  {2562,2542,2528,2514,2467,2457,2489,2520,2520,2461,2281,1891,1875,1829,1888,2224,2515,2716,2817,2781,2689,2608,2609,2570}};//Preise zu Testzwecken vom 8.+9.3.2024
//   Min h: 13, 0.1939,          |||| Min h: 13, 0.1829,      
//   Max h:  7, 0.3020,          |||| Max h: 18, 0.2817,      
//  Mittelwert: 0.2630           |||| Mittelwert: 0.2431      
