# Clock-JWS
1. Hanya mendukung ESP8266
2. Terdapat fitur ubah
   - WiFi AP
   - WiFi Router
   - NTP Server atau Manual jam (belum ada DS3231)
   - Setting Jadwal Shalat menggunakan API
   - Reset pengaturan tetapi tidak akan reset jadwal shalat
   - Menggunakan library DMD2 untuk Led Panel P10

![FireShot Capture 017 - Clock Settings - ](https://github.com/user-attachments/assets/2734b417-cee4-48ce-92a8-4ceb51c417c0)

Library Install </br>
#include <ESP8266WiFi.h> </br>
#include <ESPAsyncTCP.h> </br>
#include <FS.h> </br>
#include <ESPAsyncWebServer.h> </br>
#include <SPI.h> </br>
#include <DMD2.h> </br>
#include <fonts/SystemFont5x7.h> </br>
#include <TimeLib.h> </br>
#include <NTPClient.h> </br>
#include <WiFiUdp.h> </br>
#include <AsyncHTTPRequest_Generic.h> </br>
#include <ArduinoJson.h> </br>

Note: </br>
Jika ingin ubah library menjadi LED P5 cari tulisan ini **dmd**
