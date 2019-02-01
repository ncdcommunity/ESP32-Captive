#include <ArduinoJson.h>
#include <WiFi.h>
#include "FS.h"
#include "SPIFFS.h"
#include <EEPROM.h>
#include <WebServer.h>
#define MESSAGE_MAX_LEN 256

//**************DEEP SLEEP CONFIG******************//
#define uS_TO_S_FACTOR 1000000  
#define TIME_TO_SLEEP  5

//************** Auxillary functions******************//
WebServer server(80);
StaticJsonBuffer<234> jsonBuffer;

//**********softAPconfig Timer*************//
unsigned long APTimer = 0;
unsigned long APInterval = 120000;

//*********SSID and Pass for AP**************//
const char* ssidAPConfig = "adminesp32";
const char* passAPConfig = "adminesp32";

//**********check for connection*************//
bool isConnected = true;
bool isAPConnected = true;

//*********Contains SPIFFS Info*************//
String debugLogData;

//HTML char array
const char HTTP_HEAD_HTML[] PROGMEM = "<!DOCTYPE HTML><html><head><meta name = \"viewport\" http-equiv=\"content-type\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\"><title>ESP32 Demo</title>";
const char HTTP_STYLE[] PROGMEM= "<style>body { background-color: #0067B3  ; font-family: Arial, Helvetica, Sans-Serif; Color: #FFFFFF; }</style></head>";
const char HTTP_HEAD_STYLE[] PROGMEM= "<body><center><h1 style=\"color:#FFFFFF; font-family:verdana;font-family: verdana;padding-top: 10px;padding-bottom: 10px;font-size: 36px\">ESP32 Captive Portal</h1><h2 style=\"color:#FFFFFF;font-family: Verdana;font: caption;font-size: 27px;padding-top: 10px;padding-bottom: 10px;\">Give Your WiFi Credentials</h2>";
const char HTTP_FORM_START[] PROGMEM= "<FORM action=\"/\" method= \"post\">";
const char HTTP_CONTENT1_START[] PROGMEM= "<div style=\"padding-left:100px;text-align:left;display:inline-block;min-width:150px;\"><a href=\"#pass\" onclick=\"c(this)\" style=\"text-align:left\">{v}</a></div>&nbsp&nbsp&nbsp <div style=\"display:inline-block;min-width:260px;\"><span class=\"q\" style=\"text-align:right\">{r}%</span></div><br>";
const char HTTP_CONTENT2_START[] PROGMEM= "<P ><label style=\"font-family:Times New Roman\">SSID</label><br><input maxlength=\"30px\" id=\"ssid\" type=\"text\" name=\"ssid\" placeholder='Enter WiFi SSID' style=\"width: 400px; padding: 5px 10px ; margin: 8px 0; border : 2px solid #3498db; border-radius: 4px; box-sizing:border-box\" ><br></P>";
const char HTTP_CONTENT3_START[] PROGMEM= "<P><label style=\"font-family:Times New Roman\">PASSKEY</label><br><input maxlength=\"30px\" type = \"text\" id=\"pass\" name=\"passkey\"  placeholder = \"Enter WiFi PASSKEY\" style=\"width: 400px; padding: 5px 10px ; margin: 8px 0; border : 2px solid #3498db; border-radius: 4px; box-sizing:border-box\" ><br><P>";
const char HTTP_CONTENT4_START[] PROGMEM= "<input type=\"checkbox\" name=\"configure\" value=\"change\"> Change IP Settings </P>";
const char HTTP_CONTENT5_START[] PROGMEM= "<INPUT type=\"submit\">&nbsp&nbsp&nbsp&nbsp<INPUT type=\"reset\"><style>input[type=\"reset\"]{background-color: #3498DB; border: none; color: white; padding:  15px 48px; text-align: center; text-decoration: none;display: inline-block; font-size: 16px;}input[type=\"submit\"]{background-color: #3498DB; border: none; color: white; padding:  15px 48px;text-align: center; text-decoration: none;display: inline-block;font-size: 16px;}</style>";
const char HTTP_FORM_END[] PROGMEM= "</FORM>";
const char HTTP_SCRIPT[] PROGMEM= "<script>function c(l){document.getElementById('ssid').value=l.innerText||l.textContent;document.getElementById('pass').focus();}</script>";
const char HTTP_END[] PROGMEM= "</body></html>";

//const char* const WEBPAGE_TABLE[] PROGMEM = {HTTP_HEAD_HTML, HTTP_STYLE, HTTP_HEAD_STYLE, HTTP_FORM_START, HTTP_CONTENT1_START, HTTP_CONTENT2_START,HTTP_CONTENT3_START,HTTP_CONTENT4_START,HTTP_CONTENT5_START,HTTP_FORM_END,HTTP_SCRIPT, };
const char* messageStatic PROGMEM= "{\"staticSet\":\"staticValue\", \"staticIP\":\"%s\", \"staticGate\":\"%s\", \"staticSub\":\"%s\",\"ssidStatic\":\"%s\",\"staticPass\":\"%s\"}";
const char* messageDhcp PROGMEM= "{\"dhcpSet\":\"dhcpValue\",\"ssidDHCP\":\"%s\", \"passDHCP\":\"%s\"}";

const char HTTP_PAGE_STATIC[] PROGMEM = "<p>{s}<br>{g}<br>{n}<br></p>";
const char HTTP_PAGE_DHCP[] PROGMEM = "<p>{s}</p>";
const char HTTP_PAGE_WiFi[] PROGMEM = "<p>{s}<br>{p}</p>";
const char HTTP_PAGE_GOHOME[] PROGMEM = "<H2><a href=\"/\">go home</a></H2><br>";

char messageBuf[MESSAGE_MAX_LEN]; 

void setup() {
  Serial.begin(115200);
  while(!Serial);
  WiFi.persistent(false);
  WiFi.disconnect(true);
  SPIFFS.begin();
  delay(100);  
  EEPROM.begin(512);
  delay(100);
  File file = SPIFFS.open("/ip_set.txt", "r");     
  Serial.println("- read from file:");
     if(!file){
        Serial.println("- failed to open file for reading");
        return;
    }
    while(file.available()){
        debugLogData += char(file.read());
    } 
    file.close();
    if(debugLogData.length()>10){
       JsonObject& readRoot =jsonBuffer.parseObject(debugLogData);
          Serial.println("=====================================");
          Serial.println(debugLogData);
          Serial.println("=====================================");
          if(readRoot.containsKey("staticSet")){
             Serial.println("Static IP Started ");
             staticAPConfig(readRoot["staticIP"],readRoot["staticGate"],readRoot["staticSub"],readRoot["ssidStatic"],readRoot["staticPass"]);
             }
           else if(readRoot.containsKey("dhcpSet")){
                   Serial.println("DHCP IP Started" );
                   dhcpAPConfig(readRoot["ssidDHCP"],readRoot["passDHCP"]);
                   }
           else{
               handleClientAP();
               }
     }else{ 
      handleClientAP();
   } 
  reconnectWiFi();
}


void loop() {
  Serial.println(WiFi.localIP());
  delay(500);
}

  
//****************************HANDLE ROOT***************************//
void handleRoot() {
   //Redisplay the form
   if(server.args()>0){
       for(int i=0; i<=server.args();i++){
          Serial.println(String(server.argName(i))+'\t' + String(server.arg(i)));
        }
     if(server.hasArg("ipv4static") && server.hasArg("gateway") &&  server.hasArg("subnet")){
      staticSet();
      }else if(server.hasArg("passkeyDhcp")&&server.hasArg("ssidDhcp")){
           dhcpSetDefault();
        }    
    }else{
      File file = SPIFFS.open("/Select_Settings.html", "r");
         server.streamFile(file,"text/html");
         file.close();
      }
}  

//****************************HANDLE DHCP***************************//
void handleDHCP(){
  File  file = SPIFFS.open("/page_dhcp.html", "r");
  server.streamFile(file,"text/html");
  file.close();
  }

//****************************HANDLE STATIC***************************//
void handleStatic(){
  File  file = SPIFFS.open("/page_static.html", "r");
  server.streamFile(file,"text/html");
  file.close();
  }

//*************Helper Meathod for Writing IP CONFIG**************//

//*************Helper 1 STATIC**************//

void staticSet(){
           String response=FPSTR(HTTP_PAGE_STATIC);
           response.replace("{s}",server.arg("ipv4static"));
           response.replace("{g}",server.arg("gateway"));
           response.replace("{n}",server.arg("subnet"));
           response+=FPSTR(HTTP_PAGE_GOHOME);
           server.send(200, "text/html", response);
           snprintf(messageBuf,MESSAGE_MAX_LEN,messageStatic,String(server.arg("ipv4static")),String(server.arg("gateway")),String(server.arg("subnet")),String(server.arg("ssidStatic")),String(server.arg("passkeyStatic")));
           String str(messageBuf);
           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
              return;
            }
             if(fileToWrite.write((uint8_t*)str.c_str(),str.length())){
                Serial.println("--File Written");
            }else{
                Serial.println("--Error Writing File");
              } 
            fileToWrite.close();   
             isConnected = false;
    }

//*************Helper 3 DHCP DEFAULT**************//
  
void dhcpSetDefault(){
           String response=FPSTR(HTTP_PAGE_DHCP);
           response.replace("{s}","192.168.4.1");
           response+=FPSTR(HTTP_PAGE_GOHOME);
           server.send(200, "text/html", response);
           snprintf(messageBuf,MESSAGE_MAX_LEN,messageDhcp,String(server.arg("ssidDhcp")).c_str(),String(server.arg("passkeyDhcp")).c_str());
           String str(messageBuf);
           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
            }
           if(fileToWrite.write((uint8_t*)str.c_str(),str.length())){
                Serial.println(F("--File Written"));
            }else{
                Serial.println(F("--Error Writing File"));
              }           
           fileToWrite.close();  
           isConnected = false;          
      }

//****************HANDLE NOT FOUND*********************//
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  server.send(404, "text/plain", message);
}

//***************Parse bytes from string******************//

void parseBytes(const char* str, char sep, byte* bytes, int maxBytes, int base) {
    for (int i = 0; i < maxBytes; i++) {
        bytes[i] = strtoul(str, NULL, base);  // Convert byte
        str = strchr(str, sep);               // Find next separator
        if (str == NULL || *str == '\0') {
            break;                            // No more separators, exit
        }
        str++;                                // Point to next character after separator
    }
}

//****************HANDLE CLIENT 192.168.1.77*********************//

void handleClientAP(){
   //*********Static IP Config**************//
   WiFi.mode(WIFI_AP);
   Serial.println(WiFi.softAP(ssidAPConfig,passAPConfig) ? "soft-AP setup": "Failed to connect");
   delay(100);
   Serial.println(WiFi.softAPConfig( IPAddress(192,168,1,77),IPAddress(192,168,1,254), IPAddress(255,255,255,0))? "Configuring Soft AP" : "Error in Configuration");      
   Serial.println(WiFi.softAPIP());
   server.begin();
   server.on("/", handleRoot); 
   server.on("/dhcp", handleDHCP);
   server.on("/static", handleStatic);
   server.onNotFound(handleNotFound);  
   
   APTimer = millis();
    
   while(isConnected && millis()-APTimer<= APInterval) {
        server.handleClient();}  
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();    
  }
  
//***************************STATIC Helper method**************************//

void  staticAPConfig(String IPStatic, String gateway, String subnet, String ssid, String pass){
      //*********hold IP octet**************//
      uint8_t ip0,ip1,ip2,ip3;
      //*********IP Char Array**************//
      Serial.print(ssid);
      Serial.print(pass);
      byte ip[4];
      parseBytes(IPStatic.c_str(),'.', ip, 4, 10);
      ip0 = (uint8_t)ip[0];
      ip1 = (uint8_t)ip[1];
      ip2 = (uint8_t)ip[2];
      ip3 = (uint8_t)ip[3];
      IPAddress ap_local(ip0,ip1,ip2,ip3);
      parseBytes(gateway.c_str(),'.', ip, 4, 10);
      ip0 = (uint8_t)ip[0];
      ip1 = (uint8_t)ip[1];
      ip2 = (uint8_t)ip[2];
      ip3 = (uint8_t)ip[3];
      IPAddress ap_gate(ip0,ip1,ip2,ip3);
      parseBytes(subnet.c_str(),'.', ip, 4, 10);
      ip0 = (uint8_t)ip[0];
      ip1 = (uint8_t)ip[1];
      ip2 = (uint8_t)ip[2];
      ip3 = (uint8_t)ip[3];
      IPAddress ap_net(ip0,ip1,ip2,ip3);  
      WiFi.disconnect(true);
      WiFi.mode(WIFI_AP);   
      Serial.println(WiFi.softAP(ssid.c_str(),pass.c_str()) ? "Setting up SoftAP" : "error setting up");
      delay(100);       
      Serial.println(WiFi.softAPConfig(ap_local, ap_gate, ap_net) ? "Configuring softAP" : "kya yaar not connected");    
      Serial.println(WiFi.softAPIP());
      server.begin();
      server.on("/", handleStaticForm); 
      server.onNotFound(handleNotFound);
   
      APTimer = millis();
      while(isAPConnected && millis()-APTimer<= APInterval) {
         server.handleClient();  }            
    }

//***************************WiFi Credintial Form**************************//

void dhcpAPConfig(String ssid, String pass){
      WiFi.mode(WIFI_OFF);
      WiFi.softAPdisconnect(true);
      delay(1000);
      WiFi.mode(WIFI_AP);
      Serial.println(WiFi.softAP(ssid.c_str(),pass.c_str()) ? "Setting up SoftAP" : "error setting up");
      delay(200);
      Serial.println(WiFi.softAPIP());
      
      server.begin();
      server.on("/", handleStaticForm); 
      server.onNotFound(handleNotFound);
      APTimer = millis();
      while(isAPConnected && millis()-APTimer<= APInterval) {
       server.handleClient();  }
       
  }

//****************************HANDLE STATIC FORM***************************//

void handleStaticForm() {
    JsonObject& root =jsonBuffer.createObject();
    root["no"]= "";
    root.printTo(Serial);  
if(server.hasArg("ssid") && server.hasArg("passkey")){
       if(server.arg("configure") != ""){
            File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
              return;
            }
           if(root.printTo(fileToWrite)){
                Serial.println("--File Written");
                esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
                esp_deep_sleep_start();  
            }else{
                Serial.println("--Error Writing File");
              }   
        }
       handleSubmitForm();
    }else{
            int n = WiFi.scanNetworks();
           int indices[n];     
       if(n == 0){
       Serial.println("No networks found");
      }else{   
           for (int i = 0; i < n; i++) {
           indices[i] = i;
         }
        
        for (int i = 0; i < n; i++) {
          Serial.println(WiFi.SSID(indices[i]));
          Serial.print('\t');
          Serial.println(getRSSIasQuality(WiFi.RSSI(indices[i])));
           }
         }
         
         String webpage = FPSTR(HTTP_HEAD_HTML);
         webpage += FPSTR(HTTP_STYLE);
         webpage += FPSTR(HTTP_HEAD_STYLE);
         webpage += FPSTR(HTTP_FORM_START);
         for(int i=0;i<n;i++){
           int quality = getRSSIasQuality(WiFi.RSSI(indices[i]));
           String item = FPSTR(HTTP_CONTENT1_START);
           String RssiQuality = String(quality);
           item.replace("{v}",WiFi.SSID(indices[i]));
           item.replace("{r}",RssiQuality); 
           webpage+=item;
         }
         webpage += FPSTR(HTTP_CONTENT2_START);
         webpage += FPSTR(HTTP_CONTENT3_START);
         webpage += FPSTR(HTTP_CONTENT4_START);
         webpage += FPSTR(HTTP_CONTENT5_START);
         webpage += FPSTR(HTTP_FORM_END);
         webpage += FPSTR(HTTP_SCRIPT);
         webpage += FPSTR(HTTP_END);
         server.send(200,"text/html",webpage);
      }  
  }
  
//****************************WiFi Credintial Submit****************************//

void handleSubmitForm() {
      String response=FPSTR(HTTP_PAGE_WiFi);
      response.replace("{s}",server.arg("ssid"));
      response.replace("{p}",String(server.arg("passkey")));
      response+=FPSTR(HTTP_PAGE_GOHOME);
      server.send(200, "text/html", response);
      ROMwrite(String(server.arg("ssid")),String(server.arg("passkey")));
      isAPConnected = false;    
  }
  

//----------Write to ROM-----------//
void ROMwrite(String s, String p){
 s+=";";
 write_EEPROM(s,0);
 p+=";";
 write_EEPROM(p,50);
 EEPROM.commit();   
}

//***********Write to ROM**********//
void write_EEPROM(String x,int pos){
  for(int n=pos;n<x.length()+pos;n++){
  //write the ssid and password fetched from webpage to EEPROM
   EEPROM.write(n,x[n-pos]);
  }
}

  
//****************************EEPROM Read****************************//
String read_string(int l, int p){
  String temp;
  for (int n = p; n < l+p; ++n)
    {
   // read the saved password from EEPROM
     if(char(EEPROM.read(n))!=';'){
     
       temp += String(char(EEPROM.read(n)));
     }else n=l+p;
    }
  return temp;
}


//****************************Connect to WiFi****************************//
void reconnectWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
        String string_Ssid="";
        String string_Password="";
        string_Ssid= read_string(30,0); 
        string_Password= read_string(30,50);        
        Serial.println("ssid: "+ string_Ssid);
        Serial.println("Password: "+string_Password);
               
  delay(400);
  WiFi.begin(string_Ssid.c_str(),string_Password.c_str());
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {   
      delay(500);
      Serial.print(".");
      if(counter == 20){
          String response = "<script>alert(\"Password not connected\")</script";
          server.send(200,"text/html",response);
          ESP.restart();
        }
        counter++;
  }

  Serial.print("Connected to:\t");
  Serial.println(WiFi.localIP());
}


int getRSSIasQuality(int RSSI) {
  int quality = 0;
  if (RSSI <= -100) {
    quality = 0;
  } else if (RSSI >= -50) {
    quality = 100;
  } else {
    quality = 2 * (RSSI + 100);
  }
  return quality;
}
