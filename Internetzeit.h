
#define MY_NTP_SERVER "de.pool.ntp.org"         // Configuration of NTP     
#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"  //Definition /Sommerzeit / Winterzeit 

//* Necessary Includes */
//#include <ESP8266WiFi.h>            // we need wifi to get internet access
#include <time.h>                   // time() ctime()
const char *wochentag[]={"So.", "Mo.", "Di.", "Mi.", "Do.", "Fr.", "Sa."};
/* Globals */
time_t now;                         // this is the epoch
tm tm;                              // the structure tm holds time information in a more convient way
char start_datum_zeit[20];
//*********************************************************************************
void showTime() {
  static boolean ersterDurchlauf=true;
  if(ersterDurchlauf==true) { //nur beim 1, Aufruf zum Initialisieren durchlaufen . . . 
    //ersterDurchlauf=false;
    configTime(MY_TZ, MY_NTP_SERVER); // --> Here is the IMPORTANT ONE LINER needed in your sketch!
  }
  /*unsigned long zeit=*/ time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time
  tm.tm_year += 1900;
  tm.tm_mon += 1;
  if(ersterDurchlauf==true) { //nur beim 1, Aufruf 
    ersterDurchlauf=false;
    sprintf(start_datum_zeit,"%s,%02d.%02d.%04d %02d:%02d:%02d",wochentag[tm.tm_wday],tm.tm_mday,tm.tm_mon,tm.tm_year,tm.tm_hour,tm.tm_min,tm.tm_sec);
  }
//  Serial.print("year:");  Serial.print(tm.tm_year + 1900);  // years since 1900
//  Serial.print(".");      Serial.print(tm.tm_mon + 1);      // January = 0 (!)
//  Serial.print(".");      Serial.print(tm.tm_mday);         // day of month
//  Serial.print(", ");     Serial.print(tm.tm_hour);         // hours since midnight  0-23
//  Serial.print(".");      Serial.print(tm.tm_min);          // minutes after the hour  0-59
//  Serial.print("\.");     Serial.print(tm.tm_sec);          // seconds after the minute  0-61*
//  Serial.print("\twday"); Serial.print(tm.tm_wday);         // days since Sunday 0-6
//  if (tm.tm_isdst == 1) Serial.print("\tSommerzeit");       // Daylight Saving Time flag
//  else Serial.print("\tStandardzeit");
//  Serial.println();
}
//*********************************************************************************
//void loop() {
//  showTime();
//  delay(1000); // dirty delay
//}

uint32_t sntp_update_delay_MS_rfc_not_less_than_15000 () {
  return 12 * 60 * 60 * 1000UL; // 12 hours
}
/*
Internetquelle: https://werner.rothschopf.net/201802_arduino_esp8266_ntp.htm

Die eigentliche Konfiguration erfolgt über zwei Pre-Compiler defines:

Zunächst definierst du den NTP Server an den du die Anfrage stellen willst. Den Server wählst du am besten entsprechend deinem Standort aus. In meinem Fall ist es ein NTP Server Pool in Österreich:

#define MY_NTP_SERVER "at.pool.ntp.org" // set the best fitting NTP server (pool) for your location
Zum Umrechnen der UTC auf die konkrete Ortszeit verwendest du eine bestehende Zeitzonedefinition:

#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03" // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
Den Eintrag wählst du aus der Liste aus (am Ende der Seite verlinkt). Der Eintrag enthält alle Informationen die der ESP8266 zur Berücksichtigen der Zeitzone sowie zur Umstellung von Sommerzeit auf Winterzeit (Normalzeit). Die Sommerzeitumstellung erfolgt im dritten Monat (M3), der fünften Woche (5), am Sonntag (0) um zwei Uhr (/02). Die Winterzeitumstellung erfolgt im 10 Monat (M10) wieder in der fünften Woche (5) am Sonntag(0) um drei Uhr (/03).

Mit dem ESP Core kommt eine time Library, die du inkludieren musst:

#include <time.h>                   // time() ctime()
Der Beispiel-Sketch verwendet zwei globale Variablen:

now entspricht dem UNIX Timestamp, der Epoche, also angelaufenen Sekunden seit 1. Jänner 1970.

time_t now;                         // this is the epoch
Da Menschen gewohnt sind Datum/Uhrzeit in Teilen (Tag, Monat Jahr...) zu verwenden, gibt es die Struktur tm:

tm tm;                              // the structure tm holds time information in a more convient way
Die Struktur tm ist in der time.h definiert. Durch tm tm wird daher eine Instanz tm vom typ tm definiert. Die wichtigsten Membervariablen dieser Instanz tm besprechen wir noch etwas später bei der Ausgabe.

Im Setup kommt die wichtigste Zeile vom ganzen Sketch:

 configTime(MY_TZ, MY_NTP_SERVER); // --> Here is the IMPORTANT ONE LINER needed in your sketch!
Mit dieser einen Codezeile gibst du dem ESP deine Zeitzone bekannt und legst fest welcher NTP Server verwendet werden soll.

MEHR BRAUCHT ES NICHT FÜR EINE NTP ABFRAGE AM ESP8266! (!!!)

Im Sketch findet sich noch eine Funktion zur Ausgabe der Zeit auf der seriellen Schnittstelle. Die Ausgabe erfolgt in 3 Schritten: Zunächst machst du ein Update der Variable now und übernimmst den aktuellen Zeitstempel. Mit der localtime_r wandelst du den Zeitstempel in die Struktur tm.

Die Struktur enthält folgende wichtige Member-Variablen:

  Member    Type  Meaning Range
  tm_sec    int   seconds after the minute  0-61*
  tm_min    int   minutes after the hour  0-59
  tm_hour   int   hours since midnight  0-23
  tm_mday   int   day of the month  1-31
  tm_mon    int   months since January  0-11
  tm_year   int   years since 1900
  tm_wday   int   days since Sunday 0-6
  tm_yday   int   days since January 1  0-365
  tm_isdst  int   Daylight Saving Time flag

* tm_sec ist für gewöhnlich 0..59. Die zusätzlichen Werte können bei Schaltsekunden vorkommen.

Mit beispielsweise tm.tm_min kannst du auf die Zeitkomponente Minute zugreifen. Eine Stolperfalle lauert beim Monat: Die Monate werden von 0 (Jänner) bis 11 (Dezember) angegeben. Auch beim Jahr musst du achtgeben, aber hier reicht die Addition von 1900 um auf das 4stellige Jahr zu kommen.

Ja und das war es wirklich - du hast einen lauffähigen NTP Sketch für den ESP8266!

Die NTP Standard-Konfiguration am ESP8266
Wenn du den Sketch auf deinen ESP hochladest wirst du feststellen, dass zunächst das Datum 1.1.1970 ausgegeben wird. Defaultmäßig beginnt die erste NTP Anfrage nach 60 Sekunden. Das lässt sich ändern. Ein IDE Beispiel gibt es dazu im originalen NTP Sketch mit der Funktion sntp_startup_delay_MS_rfc_not_less_than_60000. In der (obsoleten) RFC 4330 Seite 22 wird der Boot Delay von 1 - 5 Minuten empfohlen. In der aktuellen RFC5905 habe ich zwar diesen Hinweis nicht mehr gefunden, aber ich finde die Verzögerung ohnehin besser, da man sieht, wie die Uhrzeit aktualisiert wird.

Ein weiterer Standardwert ist der NTP Abfrageintervall. Standardmäßig ist dieser auf 60 Minuten. Auch dieser Wert lässt sich verändern. Imho reicht eine Zeitsyncronisierung alle 12h. Ein Beispiel findest du dazu im originalen Sketch (sntp_update_delay_MS_rfc_not_less_than_15000). Dazu aktivierst du diese Funktion und passt sie entsprechend an:

uint32_t sntp_update_delay_MS_rfc_not_less_than_15000 ()
{
  return 12 * 60 * 60 * 1000UL; // 12 hours
}
Beide Funktionen sind in der Library speziell gekennzeichnet ("weak"). Du brauchst die Funktionen nur definieren, aber du brauchst sie weder im setup noch im loop() aufrufen. Der ESP Core ruft sie als callback aus deinem Sketch auf, wenn sie vorhanden sind.

Hinweise und Änderungen

Ab dem ESP Core 2.6.0 ist das NTP Handling enthalten, hat sich aber offenbar in Details geändert
Mit dem Versionssprung auf 2.7.4 gab es eine Änderung im API. Daher funktionieren möglicherweise ältere Beispiele nicht mehr mit dem neueren Core. Es kann vorkommen, dass der erste Aufruf von NTP noch ordnungsgemäß in die jeweilige Zeitzone konvertiert werden kann, jeder weitere Sync (nach einer Stunde) bringt dann aber nur mehr die UTC Zeit.
Und dann noch in eigener Sache
Der Sketch kommt so wie er ist und basiert überwiegend auf dem originalen Arduino IDE Beispiel.
Kommentare gibt es dann, wenn ich glaube dass sie notwendig sind. Andere machen sicher mehr.
"Der Sketch funktioniert nicht" ... doch er funktioniert ganz bestimmt. Man benötigt dazu eine aktuelle Arduino IDE und einen aktuellen ESP Core. Zum Zeitpunkt der Sketch Erstellung war das Arduino IDE 1.8.13 und ESP Core 2.7.4.
Meine Entwicklung ist für einen NodeMCU V2 entstanden sollte aber auf jedem ESP8266 basierenden Board funktionieren.
Für den ESP32 gibt es eine eigene Variante!
Wenn es nützlich war, freue ich mich über ein Kommentar/email.
*/
