#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "FS.h"

//Variables
int i = 0;
int statusCode;
String st;
String content;
  
//Function Decalration
void launchWeb(void);
void setupAP(void);
bool saveConfig(void);
bool loadConfig(void); 
 
//Establishing Local server at port 80 whenever required
ESP8266WebServer server(80);

char wifi_ssid[10];
char wifi_password[15];

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if(!configFile) {
    Serial.println("Failed to open config file:");
    return false;
  }
  size_t size = configFile.size();
  Serial.println("size ");
  Serial.println(size);
  if(size > 256){
    Serial.println("Config file size is to large");
    return false;
  }

  //Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);
  Serial.println(buf.get());
  
  StaticJsonBuffer<256> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if(!json.success()){
    Serial.println("Failed to parse config file");
    return false; 
  }
    
  const char* json_ssid = json["json_ssid"];
  strncpy(wifi_ssid, json_ssid, 10);
  Serial.print("ssid Loaded: ");
  Serial.println(wifi_ssid);
 
  const char* json_paswd = json["json_paswd"];
  strncpy(wifi_password, json_paswd, 15);
  Serial.print("paswd Loaded: ");
  Serial.println(wifi_password);

  return true;
}

bool saveConfig() {
  StaticJsonBuffer<256> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  
  json["json_ssid"] = wifi_ssid;
  json["json_paswd"] = wifi_password;
  
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}
 
void setup()
{
  Serial.begin(9600); //Initialising if(DEBUG)Serial Monitor
  Serial.println();
  
  Serial.println("Monting FS...");  
  if(!SPIFFS.begin()){
    Serial.println("Failed to mount file system");
    return;
  }  

  loadConfig();
  saveConfig();
    
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.print("\n"); 
  Serial.println("Turning the HotSpot On");
  launchWeb();
  setupAP();// Setup HotSpot
 
  Serial.println();
  Serial.println("Waiting...");
  
  while ((WiFi.status() != WL_CONNECTED))
  {
    delay(100);
    server.handleClient();
  } 
}

void loop() {
}
 
void launchWeb()
{
  Serial.println("");
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  server.begin();
  Serial.println("Server started");
}
 
void setupAP(void)
{
  WiFi.mode(WIFI_STA);
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.print("\n");
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
 
    st += ")";
    st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  WiFi.softAP(wifi_ssid, wifi_password);
  launchWeb();
}
 
void createWebServer()
{
 {
    server.on("/", []() {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Hello from Transline ";
      content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
      content += ipStr;
      content += "<p>";
      content += st;
      content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=10><input name='pass' length=15><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });
    
    server.on("/scan", []() {
      //setupAP();
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
 
      content = "<!DOCTYPE HTML>\r\n<html>go back";
      server.send(200, "text/html", content);
    });
 
    server.on("/setting", []() {
    String qsid = server.arg("ssid");
    String qpass = server.arg("pass");

    int sid_str_len = qsid.length() + 1; 
    char webssid[sid_str_len];
    qsid.toCharArray(webssid, sid_str_len);

    int pass_str_len = qpass.length() + 1; 
    char webpass[pass_str_len];
    qpass.toCharArray(webpass, pass_str_len);
      
      if ((qsid.length() > 0) && (qpass.length() >= 8) && (qpass.length() <=15)) {  

        strncpy(wifi_ssid, webssid, 10);
        Serial.print("writing eeprom ssid:");
        Serial.println(wifi_ssid);
 
        strncpy(wifi_password, webpass, 15); 
        Serial.print("writing eeprom pass:");
        Serial.println(wifi_password);
        Serial.print("\n");
           
        saveConfig();   
 
        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
        ESP.restart();
      }
      else {
        content = "{\"Error\":\"404 not found\"}";
        content += "{\"password length must be of 8-15\"}";
        statusCode = 404;
        Serial.println("Sending 404");
        Serial.println("password length must be of 8-15");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content); 
    });
  } 
}
