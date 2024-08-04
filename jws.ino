#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <FS.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <SPIFFS.h>
#endif

#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <DMD2.h>
#include <fonts/SystemFont5x7.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define PANELS_WIDTH 1
#define PANELS_HEIGHT 1

const uint8_t *FONT = SystemFont5x7;

SPIDMD dmd(PANELS_WIDTH, PANELS_HEIGHT);

char apSSID[33] = "Clock Setting";
char apPassword[65] = "12345678";

String routerSSID = "";
String routerPassword = "";

unsigned long previousMillisclock = 0;
const long intervalclock = 1000;
unsigned long lastWiFiCheckMillis = 0;
const long wifiCheckInterval = 30000;
unsigned long lastNTPUpdate = 0;
unsigned long lastSuccessfulNTPUpdate = 0;
const unsigned long ntpUpdateInterval = 3600000;

bool colonOn = true;
time_t currentTime = 0;

AsyncWebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

String ntpServer = "pool.ntp.org";

String subuhTime = "04:30";
String zuhurTime = "12:00";
String asarTime = "15:30";
String maghribTime = "18:00";
String isyaTime = "19:30";

bool displayNeedsUpdate = false;
int failedNTPUpdates = 0;
int failedWiFiConnections = 0;

void resetSettings() {
  routerSSID = "";
  routerPassword = "";
  saveWiFiCredentials();
 
  ntpServer = "pool.ntp.org";
  saveNTPServer(ntpServer);
 
  strncpy(apSSID, "Clock Setting", sizeof(apSSID) - 1);
  strncpy(apPassword, "12345678", sizeof(apPassword) - 1);
  apSSID[sizeof(apSSID) - 1] = '\0';
  apPassword[sizeof(apPassword) - 1] = '\0';
  saveAPSettings();
 
  Serial.println("Semua pengaturan direset ke nilai default");
}
 
void loadAPSettings() {
  if (SPIFFS.exists("/ap_settings.txt")) {
    File file = SPIFFS.open("/ap_settings.txt", "r");
    if (file) {
      String ssid = file.readStringUntil('\n');
      String password = file.readStringUntil('\n');
      ssid.trim();
      password.trim();
      strncpy(apSSID, ssid.c_str(), sizeof(apSSID) - 1);
      strncpy(apPassword, password.c_str(), sizeof(apPassword) - 1);
      apSSID[sizeof(apSSID) - 1] = '\0';
      apPassword[sizeof(apPassword) - 1] = '\0';
      Serial.println("Pengaturan AP dimuat: " + String(apSSID));
      file.close();
    }
  }
}
 
void saveAPSettings() {
  File file = SPIFFS.open("/ap_settings.txt", "w");
  if (file) {
    file.println(apSSID);
    file.println(apPassword);
    file.close();
    Serial.println("Pengaturan AP disimpan");
  }
}
 
void loadNTPServer() {
  if (SPIFFS.exists("/ntp_server.txt")) {
    File file = SPIFFS.open("/ntp_server.txt", "r");
    if (file) {
      ntpServer = file.readStringUntil('\n');
      ntpServer.trim();
      Serial.println("Server NTP dimuat: " + ntpServer);
      file.close();
    }
  }
}
 
void saveNTPServer(String server) {
  File file = SPIFFS.open("/ntp_server.txt", "w");
  if (file) {
    file.println(server);
    file.close();
    Serial.println("Server NTP disimpan: " + server);
  }
}

void loadPrayerTimes() {
  if (SPIFFS.exists("/prayer_times.txt")) {
    File file = SPIFFS.open("/prayer_times.txt", "r");
    if (file) {
      subuhTime = file.readStringUntil('\n'); subuhTime.trim();
      zuhurTime = file.readStringUntil('\n'); zuhurTime.trim();
      asarTime = file.readStringUntil('\n'); asarTime.trim();
      maghribTime = file.readStringUntil('\n'); maghribTime.trim();
      isyaTime = file.readStringUntil('\n'); isyaTime.trim();
      file.close();
      Serial.println("Waktu sholat dimuat");
    }
  }
}

void savePrayerTimes() {
  File file = SPIFFS.open("/prayer_times.txt", "w");
  if (file) {
    file.println(subuhTime);
    file.println(zuhurTime);
    file.println(asarTime);
    file.println(maghribTime);
    file.println(isyaTime);
    file.close();
    Serial.println("Waktu sholat disimpan");
  }
}
 
void loadWiFiCredentials() {
  if (SPIFFS.exists("/wifi_creds.txt")) {
    File file = SPIFFS.open("/wifi_creds.txt", "r");
    if (file) {
      routerSSID = file.readStringUntil('\n');
      routerPassword = file.readStringUntil('\n');
      routerSSID.trim();
      routerPassword.trim();
      Serial.println("Kredensial WiFi dimuat: " + routerSSID);
      file.close();
    }
  }
}
 
void saveWiFiCredentials() {
  File file = SPIFFS.open("/wifi_creds.txt", "w");
  if (file) {
    file.println(routerSSID);
    file.println(routerPassword);
    file.close();
    Serial.println("Kredensial WiFi disimpan");
  }
}
 
void saveWiFiMode(WiFiMode_t mode) {
  File file = SPIFFS.open("/wifi_mode.txt", "w");
  if (file) {
    file.println(static_cast<int>(mode));
    file.close();
    Serial.println("Mode WiFi disimpan");
  }
}
 
WiFiMode_t loadWiFiMode() {
  WiFiMode_t mode = WIFI_AP_STA;  // Default mode
  if (SPIFFS.exists("/wifi_mode.txt")) {
    File file = SPIFFS.open("/wifi_mode.txt", "r");
    if (file) {
      String modeStr = file.readStringUntil('\n');
      modeStr.trim();
      mode = (WiFiMode_t)modeStr.toInt();
      file.close();
    }
  }
  return mode;
}
 
bool checkInternetConnection() {
  WiFiClient client;
  if (!client.connect("www.google.com", 80)) {
    Serial.println("Gagal terhubung ke www.google.com");
    return false;
  }
  Serial.println("Berhasil terhubung ke www.google.com");
  client.stop();
  return true;
}
 
void connectToWiFi() {
  int attempts = 0;
  const int maxAttempts = 20;
 
  Serial.println("Menghubungkan ke WiFi...");
  WiFi.begin(routerSSID.c_str(), routerPassword.c_str());
 
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
 
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nTerhubung ke WiFi");
    Serial.print("Alamat IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nGagal terhubung ke WiFi");
  }
}
 
void resetWiFiConnection() {
  WiFi.disconnect(true);
  delay(1000);
  connectToWiFi();
}
 
bool connectWiFiAndNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      return false;
    }
  }
  
  if (!checkInternetConnection()) {
    Serial.println("Tidak ada koneksi internet.");
    return false;
  }
  Serial.println("Koneksi internet tersedia.");
 
  Serial.println("Memperbarui waktu NTP...");
  timeClient.setUpdateInterval(60000);
  timeClient.setPoolServerName(ntpServer.c_str());
  
  int retryCount = 0;
  while (!timeClient.update() && retryCount < 5) {
    Serial.println("Gagal memperbarui NTP, mencoba lagi...");
    delay(1000);
    retryCount++;
  }
  
  if (retryCount == 5) {
    Serial.println("Gagal memperbarui NTP setelah 5 percobaan.");
    failedNTPUpdates++;
    return false;
  }
 
  currentTime = timeClient.getEpochTime();
  setTime(currentTime);
  lastSuccessfulNTPUpdate = millis();
  
  int hh = hour(currentTime);
  int mm = minute(currentTime);
  
  setTime(hh, mm, 0, day(currentTime), month(currentTime), year(currentTime));
  currentTime = now();
  
  Serial.printf("Waktu NTP diterima: %02d:%02d\n", hh, mm);
  Serial.println("Waktu NTP berhasil diperbarui");
  displayNeedsUpdate = true;
  failedNTPUpdates = 0;
  return true;
}
 
void setupServerRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  server.on("/assets/css/foundation.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/assets/css/foundation.css", "text/css");
  });
 
  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    resetSettings();
    request->send(200, "text/plain", "Semua pengaturan direset. Memulai ulang...");
    ESP.restart();
  });
 
  server.on("/setap", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      String newSSID = request->getParam("ssid", true)->value();
      String newPassword = request->getParam("password", true)->value();
      
      strncpy(apSSID, newSSID.c_str(), sizeof(apSSID) - 1);
      strncpy(apPassword, newPassword.c_str(), sizeof(apPassword) - 1);
      apSSID[sizeof(apSSID) - 1] = '\0';  // Ensure null termination
      apPassword[sizeof(apPassword) - 1] = '\0';  // Ensure null termination
      
      saveAPSettings();
      request->send(200, "text/plain", "Pengaturan AP diperbarui. Memulai ulang...");
      ESP.restart();
    } else {
      request->send(400, "text/plain", "Permintaan tidak valid");
    }
  });
 
  server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      routerSSID = request->getParam("ssid", true)->value();
      routerPassword = request->getParam("password", true)->value();
      saveWiFiCredentials();
      request->send(200, "text/plain", "Kredensial WiFi diperbarui. Memulai ulang...");
      ESP.restart();
    } else {
      request->send(400, "text/plain", "Permintaan tidak valid");
    }
  });
  
  server.on("/getnow", HTTP_GET, [](AsyncWebServerRequest *request) {
    char buffer[9];
    sprintf(buffer, "%02d:%02d", hour(currentTime), minute(currentTime));
    Serial.print("Mengirim waktu saat ini: ");
    Serial.println(buffer);
    request->send(200, "text/plain", buffer);
  });
  
  server.on("/settime", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("time", true)) {
      String timeParam = request->getParam("time", true)->value();
      int hh, mm;
      if (sscanf(timeParam.c_str(), "%d:%d", &hh, &mm) == 2) {
        setTime(hh, mm, 0, day(currentTime), month(currentTime), year(currentTime));
        currentTime = now();
        Serial.print("Waktu diterima: ");
        Serial.println(timeParam);
        displayNeedsUpdate = true;
        request->send(200, "text/plain", "Waktu diatur");
      } else {
        request->send(400, "text/plain", "Format waktu tidak valid");
      }
    } else {
      request->send(400, "text/plain", "Permintaan tidak valid");
    }
  });
  
  server.on("/devicestatus", HTTP_GET, [](AsyncWebServerRequest *request) {
    String response = "{";
    response += "\"ssid\":\"" + String(WiFi.SSID()) + "\",";
    response += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    response += "\"ntpConnected\":" + String(timeClient.isTimeSet() ? "true" : "false") + ",";
    response += "\"currentTime\":\"" + timeClient.getFormattedTime() + "\",";
    response += "\"lastNTPUpdate\":\"" + String(lastSuccessfulNTPUpdate) + "\",";
    response += "\"freeHeap\":\"" + String(ESP.getFreeHeap()) + "\"";
    response += "}";
    request->send(200, "application/json", response);
  });
  
  server.on("/getntpserver", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", ntpServer);
  });
  
  server.on("/setntpserver", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("server", true)) {
      String newServer = request->getParam("server", true)->value();
      ntpServer = newServer;
      saveNTPServer(ntpServer);
      timeClient.setPoolServerName(ntpServer.c_str());
      Serial.println("Menerima permintaan untuk mengatur server NTP baru: " + newServer);
      request->send(200, "text/plain", "Server NTP diperbarui");
      Serial.println("Server NTP baru disimpan dan diatur: " + ntpServer);
    } else {
      request->send(400, "text/plain", "Permintaan tidak valid");
    }
  });

  server.on("/setsubuh", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("time", true)) {
      subuhTime = request->getParam("time", true)->value();
      savePrayerTimes();
      request->send(200, "text/plain", "Waktu Subuh diperbarui");
    } else {
      request->send(400, "text/plain", "Permintaan tidak valid");
    }
  });
  
  server.on("/setzuhur", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("time", true)) {
      zuhurTime = request->getParam("time", true)->value();
      savePrayerTimes();
      request->send(200, "text/plain", "Waktu Zuhur diperbarui");
    } else {
      request->send(400, "text/plain", "Permintaan tidak valid");
    }
  });
  
  server.on("/setasar", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("time", true)) {
      asarTime = request->getParam("time", true)->value();
      savePrayerTimes();
      request->send(200, "text/plain", "Waktu Asar diperbarui");
    } else {
      request->send(400, "text/plain", "Permintaan tidak valid");
    }
  });
  
  server.on("/setmaghrib", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("time", true)) {
      maghribTime = request->getParam("time", true)->value();
      savePrayerTimes();
      request->send(200, "text/plain", "Waktu Maghrib diperbarui");
    } else {
      request->send(400, "text/plain", "Permintaan tidak valid");
    }
  });
  
  server.on("/setisya", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("time", true)) {
      isyaTime = request->getParam("time", true)->value();
      savePrayerTimes();
      request->send(200, "text/plain", "Waktu Isya diperbarui");
    } else {
      request->send(400, "text/plain", "Permintaan tidak valid");
    }
  });
  
  server.on("/getprayertimes", HTTP_GET, [](AsyncWebServerRequest *request) {
    String response = "{";
    response += "\"subuh\":\"" + subuhTime + "\",";
    response += "\"zuhur\":\"" + zuhurTime + "\",";
    response += "\"asar\":\"" + asarTime + "\",";
    response += "\"maghrib\":\"" + maghribTime + "\",";
    response += "\"isya\":\"" + isyaTime + "\"";
    response += "}";
    request->send(200, "application/json", response);
  });
}

void updateDisplay() {
  char timeStr[6];
  sprintf(timeStr, "%02d%c%02d", hour(currentTime), colonOn ? ':' : ' ', minute(currentTime));
  
  dmd.drawString(1, 0, timeStr);
  
  // Menampilkan waktu sholat
  char prayerStr[21];
  sprintf(prayerStr, "S%s Z%s A%s M%s I%s", 
          subuhTime.substring(0, 5).c_str(),
          zuhurTime.substring(0, 5).c_str(),
          asarTime.substring(0, 5).c_str(),
          maghribTime.substring(0, 5).c_str(),
          isyaTime.substring(0, 5).c_str());
  dmd.drawString(1, 8, prayerStr);

  colonOn = !colonOn;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Memulai Setup...");
  
  #if defined(ESP32)
    if (!SPIFFS.begin(true)) {
      Serial.println("Terjadi kesalahan saat memasang SPIFFS");
      return;
    }
  #elif defined(ESP8266)
    if (!SPIFFS.begin()) {
      Serial.println("Terjadi kesalahan saat memasang SPIFFS");
      return;
    }
  #endif
  Serial.println("SPIFFS berhasil dipasang");
  
  loadWiFiCredentials();
  loadNTPServer();
  loadAPSettings();

  #if defined(ESP8266)
    WiFiMode_t savedMode = WIFI_AP_STA;
  #elif defined(ESP32)
    WiFiMode_t savedMode = loadWiFiMode();
  #endif
  WiFi.mode(savedMode);

  delay(1000);
  connectToWiFi();
  
  WiFi.softAP(apSSID, apPassword);
  Serial.println("WiFi AP Diinisialisasi");
  
  timeClient.begin();
  timeClient.setTimeOffset(25200);  // Offset untuk WIB (UTC+7)
  
  if (connectWiFiAndNTP()) {
    Serial.println("WiFi dan NTP berhasil terhubung");
  } else {
    Serial.println("Gagal menghubungkan WiFi atau NTP");
  }
  
  dmd.setBrightness(255);
  dmd.selectFont(FONT);
  dmd.begin();
  
  setupServerRoutes();

  loadPrayerTimes();
  
  server.begin();
  Serial.println("Server dimulai");
  #if defined(ESP8266)
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  #elif defined(ESP32)
    Serial.printf("Free heap: %d bytes\n", ESP.getFreePsram());
  #endif
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillisclock >= intervalclock) {
    previousMillisclock = currentMillis;
    currentTime++;
    displayNeedsUpdate = true;
  }
  
  if (currentMillis - lastWiFiCheckMillis >= wifiCheckInterval) {
    lastWiFiCheckMillis = currentMillis;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi terputus. Mencoba menghubungkan kembali...");
      connectToWiFi();
      if (WiFi.status() != WL_CONNECTED) {
        failedWiFiConnections++;
        if (failedWiFiConnections >= 3) {
          Serial.println("Mengatur ulang koneksi WiFi...");
          resetWiFiConnection();
          failedWiFiConnections = 0;
        }
      } else {
        failedWiFiConnections = 0;
        connectWiFiAndNTP();
      }
    }
  }
  
  if (currentMillis - lastNTPUpdate >= ntpUpdateInterval) {
    lastNTPUpdate = currentMillis;
    if (timeClient.update()) {
      currentTime = timeClient.getEpochTime();
      setTime(currentTime);
      lastSuccessfulNTPUpdate = currentMillis;
      
      int hh = hour(currentTime);
      int mm = minute(currentTime);
      
      setTime(hh, mm, 0, day(currentTime), month(currentTime), year(currentTime));
      currentTime = now();
      
      Serial.printf("Waktu NTP diperbarui: %02d:%02d\n", hh, mm);
      displayNeedsUpdate = true;
      failedNTPUpdates = 0;
    } else {
      Serial.println("Gagal memperbarui waktu NTP");
      failedNTPUpdates++;
    }
  }
  
  if (displayNeedsUpdate) {
    updateDisplay();
    displayNeedsUpdate = false;
  }

  if (failedNTPUpdates > 10) {
    Serial.println("Terlalu banyak kegagalan NTP. Melakukan restart...");
    ESP.restart();
  }
}