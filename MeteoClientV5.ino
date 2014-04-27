
/*
 
Library LCD.
https://bitbucket.org/fmalpartida/new-liquidcrystal/downloads
1602 with PCF8574T  LCM1602 IIC V1
 
 
K_Series.h -Library for interfacing with a K-series sensor
Created by Jason Berger
for CO2METER.com
OCT-12-2012
http://www.co2meters.com/Documentation/AppNotes/AN126-K3x-sensor-arduino-uart.zip
 
FILE: dht.h
VERSION: 0.1.05
PURPOSE: DHT Temperature & Humidity Sensor library for Arduino
URL: http://playground.arduino.cc/Main/DHTLib
HISTORY:
see dht.cpp file
 
 
MemoryFree library based on code posted here:
http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1213583720/15
Extended by Matthew Murdoch to include walking of the free list.
 
 */



#include <SPI.h>
#include <Ethernet.h>
#include "kSeries.h"
#include <dht.h>
#include <MemoryFree.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

dht DHT;
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);



EthernetClient client; //

char CurTemp[5]={
}
,CurHum[5]={
}
,ppm[5]={
};

kSeries K_30(6,8);

// CHANGE MAC ADDRESS TO ANY OTHER!!! (use http://www.miniwebtool.com/mac-address-generator/ for example)
byte mac[] = { 
  0x90, 0xA2, 0x7C, 0x5E, 0xA7, 0xB9       }; // default mac address of ethernet shield

boolean DHCP = true,NMService=true,DHT22=true,DL=false,K30=true;

IPAddress IP=(192,168,1,121),GW=(192,168,1,1),DNS=(192,168,1,1),NMIP=(94,19,113,221);
IPAddress server(94,19,113,221);
int DHT_P=(7),DL_P=int (5);


const unsigned long postingInterval = 400000;  // интервал между отправками данных в миллисекундах (5 минут)
unsigned long lastConnectionTime = postingInterval;           // время последней передачи данных
boolean lastConnected = false;                  // состояние подключения
int HighByte, LowByte, TReading, SignBit, Tc_100;
char replyBuffer[106],macch[14];


void setup() {
  delay(200);
  Serial.begin (115200);
  if (DHCP) {
    if (Ethernet.begin(mac) == 0) {
      Serial.println("Failed  DHCP");
      // doing nothing
      for(;;)
        ;
    }
  } 
  else {
    Ethernet.begin(mac, IP);

  };
  delay(500);  // Couple of seconds just to warm up Ethernet.
  Serial.println();
  Serial.print("IP ");
  Serial.println(Ethernet.localIP());
  genmacchar();

  lcd.backlight ();

  lcd.begin(16,2);               // initialize the lcd 

  lcd.home ();                   // go home
  lcd.print ("IP "); 
  lcd.print(Ethernet.localIP());  
  lcd.setCursor ( 0, 1 );        // go to the next line
  lcd.print ("MAC ");      
  lcd.print (macch); 

  Serial.println();
  Serial.print("IP Address is ");
  Serial.println(Ethernet.localIP());


  Serial.print ("MAC ");
  Serial.println (macch);
  delay (2500);
  lcd.clear ();
  lcd.home ();
  lcd.print ("   ");
  lcd.print((char)223);
  lcd.print ("C");
  lcd.setCursor ( 0, 1 ); 
  lcd.print ("   ");

  lcd.print ("%     ppm");




}

void loop() {
  delay (500);

  lcd.home ();
  if (DHT22) DHTRead();
  if (K30)  co2();

  delay (3000);
  Serial.print("frmem()=");
  Serial.println(freeMemory());
  //debug
  if (client.connected()) Serial.println ("client connected");


  Serial.println ("client stopped");

  Serial.print ("lastConnectionTime= ");
  Serial.println (lastConnectionTime);
  Serial.print ("postingInterval= ");
  Serial.println (postingInterval);
  Serial.print ("millis= ");
  Serial.println (millis());
  Serial.print ("CurTemp=");
  Serial.println (CurTemp);
  Serial.print ("CurHum=");
  Serial.println (CurHum);
  Serial.print("frmem()=");
  Serial.println(freeMemory());

  //debug
  if (client.connected() && lastConnected) {
    client.stop();
  }
  if(!client.connected() && ((millis() - lastConnectionTime )> postingInterval))   {
    formingPOST ();
  
  }
  lastConnected = client.connected();

  delay (1000);



}



void co2 (){ //reading data from CO2 sensor
  itoa (K_30.getCO2('p'),ppm,10);
  //Print the value on Serial
  Serial.print("Co2 =");
  Serial.println(ppm);
  lcd.setCursor (9,0);
  lcd.print ("     ");
  lcd.setCursor (9,0);
  lcd.print (ppm);

}


void formingPOST (){

  Serial.println ("forming POST");

  //формирование HTTP-запроса. Preparing http request.
  memset(replyBuffer, 0, sizeof(replyBuffer));
  strcpy(replyBuffer,"ID=");
  strcat(replyBuffer,macch);
  //Конвертируем MAC-адрес. Converting mac address.

  //конвертируем адрес термодатчика. Converting first sensor address.
  Serial.print ("Here is our replyBuffer");
  Serial.println (replyBuffer);
  strcat(replyBuffer,"&");
  strcat(replyBuffer,macch);
  strcat(replyBuffer,"01");
  Serial.print ("Here is our replyBuffer");
  Serial.println (replyBuffer);
  //конвертируем адрес датчика влажности. Converting secong sensor address.
  strcat(replyBuffer,"=");
  if (SignBit) 
  {
    strcat(replyBuffer,"-");
    Serial.println (" Beware of Minus !!! ");
  }

  strcat(replyBuffer,CurTemp);
  strcat(replyBuffer,"&");
  strcat(replyBuffer,macch);
  strcat(replyBuffer,"02");
  strcat(replyBuffer,"=");
  strcat(replyBuffer,CurHum);
  strcat(replyBuffer,"&");
  strcat(replyBuffer,macch);
  strcat(replyBuffer,"03");
  strcat(replyBuffer,"=");
  Serial.print ("Here is our ppm");
  Serial.println (ppm);
  strcat(replyBuffer,ppm);
  Serial.print ("Here is our replyBuffer");
  Serial.println (replyBuffer);
  httpRequest();



}

void httpRequest() { 
  Serial.print ("staring to POST request");

  if (client.connect(server, 80)) {

    // send the HTTP POST request:
    client.println("POST http://narodmon.ru/post.php HTTP/1.0");
    Serial.println("POST http://narodmon.ru/post.php HTTP/1.0");
    client.println("Host: narodmon.ru");
    Serial.println("Host: narodmon.ru");
    //client.println("User-Agent: arduino-ethernet");
    //client.println("Connection: close");
    client.println("Content-Type: application/x-www-form-urlencoded");
    Serial.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(strlen(replyBuffer));
    Serial.print("Content-Length: ");
    Serial.println(strlen(replyBuffer));
    client.println();
    Serial.println();
    client.println(replyBuffer);
    Serial.println(replyBuffer);
    client.println();

    delay(1000);
  } 
  else {
    Serial.print("http request failed - client not connected");
    client.stop();
  }
  lastConnectionTime = millis();
}

void DHTRead (){

  int chk = DHT.read22(DHT_P);
  Serial.print(chk);
  switch (chk)
  {
  case DHTLIB_OK:  
    Serial.println("DHT OK,\t"); 
    delay (1000);
    dtostrf(DHT.temperature, 3, 0, CurTemp);
    Serial.print("temp and hum");
    Serial.print (CurTemp);

    Serial.print(",\t");
    delay (1000);
    dtostrf(DHT.humidity, 3, 0, CurHum);
    Serial.println (CurHum);
    lcd.setCursor(0,0);
    lcd.print ("   ");
    lcd.setCursor (0,1);
    lcd.print ("   ");
    lcd.setCursor(0,0);
    lcd.print (CurTemp);
    lcd.setCursor (0,1);
    lcd.print (CurHum);

    break;
  case DHTLIB_ERROR_CHECKSUM: 
    Serial.print("Checksum er,\t"); 
    break;
  case DHTLIB_ERROR_TIMEOUT: 
    Serial.print("Time out er,\t"); 
    break;
  default: 
    Serial.print("Unknown er,\t"); 
    break;

  }


}

void genmacchar() {
  for (int k=0; k<6; k++)
  {
    int b1=mac[k]/16;
    int b2=mac[k]%16;
    char c1[2],c2[2];

    if (b1>9) c1[0]=(char)(b1-10)+'A';
    else c1[0] = (char)(b1) + '0';
    if (b2>9) c2[0]=(char)(b2-10)+'A';
    else c2[0] = (char)(b2) + '0';

    c1[1]='\0';
    c2[1]='\0';

    strcat(macch,c1);
    strcat(macch,c2);
  }
}







