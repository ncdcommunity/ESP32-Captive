#include <ArduinoJson.h>
#define _TASK_TIMEOUT
#include <TaskScheduler.h>
#include <WiFi.h>
#include "FS.h"
#include "SPIFFS.h"
#include <EEPROM.h>
#include <WebServer.h>


//**************DEEP SLEEP CONFIG******************//
#define uS_TO_S_FACTOR 1000000  
#define TIME_TO_SLEEP  5

//************** Auxillary functions******************//
WebServer server(80);
DynamicJsonBuffer jsonBuffer;

//**********Task Timer**************//
unsigned long taskSensorTimer = 0;
unsigned long taskWiFiTimer = 0;

//**********softAPconfig Timer*************//
unsigned long APTimer = 0;
unsigned long APInterval = 120000;

//**********staticAPconfig Timer*************//
unsigned long STimer = 0;
unsigned long SInterval = 120000;

//**********dhcpAPconfig Timer*************//
unsigned long DTimer = 0;
unsigned long DInterval = 120000;

//**********Config Timer*************//
unsigned long ConfigTimer = 0;
unsigned long ConfigInterval = 20000;

//*********SSID and Pass for AP**************//
 static char ssidWiFi[30];//Stores the router name
 static char passWiFi[30];//Stores the password
 char ssidAP[30];//Stores the router name
 char passAP[30];//Stores the password
 const char *ssidAPConfig = "adminesp32";
 const char *passAPConfig = "adminesp32";

//**********check for connection*************//
bool isConnected = true;
bool isAPConnected = true;


//*********Static IP Config**************//
IPAddress ap_local_IP(192,168,1,77);
IPAddress ap_gateway(192,168,1,254);
IPAddress ap_subnet(255,255,255,0);
IPAddress ap_dhcp(192,168,4,1);


//*********Static IP WebConfig**************//
IPAddress ap_localWeb_IP;
IPAddress ap_Webgateway;
IPAddress ap_Websubnet;
IPAddress ap_dhcpWeb_IP;


//*********hold IP octet**************//
uint8_t ip0;
uint8_t ip1;
uint8_t ip2;
uint8_t ip3;


//*********IP Char Array**************//
char ipv4Arr[20];
char gatewayArr[20];           
char subnetArr[20];
char ipv4dhcpArr[20];

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
          if(readRoot.containsKey("statickey")){
             String ipStaticValue= readRoot["staticIP"];
             String gatewayValue = readRoot["gateway"];
             String subnetValue =  readRoot["subnet"];
             String ssidStatic = readRoot["ssidStatic"];
             String passStatic = readRoot["passkeyStatic"];
             Serial.println("handle Started at"+'\t' + ipStaticValue);
             staticAPConfig(ipStaticValue,gatewayValue,subnetValue,ssidStatic,passStatic);}
           else if(readRoot.containsKey("dhcpDefault")){
                   String ipdhcpValue= readRoot["dhcpIP"];
                   String ssidDhcp = readRoot["ssidDhcp"];  
                   String passDhcp = readRoot["passkeyDhcp"];
                   Serial.println("handle Started at"+'\t' + ipdhcpValue);
                   dhcpAPConfig(ssidDhcp,passDhcp);}
           else if(readRoot.containsKey("dhcpManual")){
                   String ipdhcpValue= readRoot["staticIP"];
                   String ssidDhcp = readRoot["ssidDhcp"];  
                   String passDhcp = readRoot["passkeyDhcp"];
                   Serial.println("handle Started at"+'\t' + ipdhcpValue);
                   dhcpAPConfig(ssidDhcp,passDhcp);}
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
      }else if(server.arg("ipv4")!= ""){
          dhcpSetManual();
        }else{
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
  file.close();}

//****************************HANDLE STATIC***************************//
void handleStatic(){
  File  file = SPIFFS.open("/page_static.html", "r");
  server.streamFile(file,"text/html");
  file.close();}

//*************Helper Meathod for Writing IP CONFIG**************//

//*************Helper 1 STATIC**************//

void staticSet(){
           JsonObject& root =jsonBuffer.createObject();
           String response="<p>The static ip is ";
           response += server.arg("ipv4static");
           response +="<br>";
           response +="The gateway ip is ";
           response +=server.arg("gateway");
           response +="<br>";
           response +="The subnet Mask is ";
           response +=server.arg("subnet");
           response +="</P><BR>";
           response +="<H2><a href=\"/\">go home</a></H2><br>";
           response += "<script> alert(\"Settings Saved\"); </script>";
           server.send(200, "text/html", response);
           String ipv4static = String(server.arg("ipv4static"));
           String gateway = String(server.arg("gateway"));
           String subnet = String(server.arg("subnet"));
           String ssid = String(server.arg("ssidStatic"));
           String passkey = String(server.arg("passkeyStatic"));
           root["statickey"]="staticSet";
           root["staticIP"] = ipv4static;
           root["gateway"] = gateway;
           root["subnet"] = subnet;
           root["ssidStatic"] = ssid;
           root["passkeyStatic"] = passkey;
           root.printTo(Serial);
           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
              return;
            }
           if(root.printTo(fileToWrite)){
                Serial.println("--File Written");
            }else{
                Serial.println("--Error Writing File");
              } 
            fileToWrite.close();   
             isConnected = false;
    }

//*************Helper 2 DHCP MANUAL**************//

void dhcpSetManual(){
           JsonObject& root =jsonBuffer.createObject();
           String response="<p>The dhcp IPv4 address is ";
           response += server.arg("ipv4");
           response +="</P><BR>";
           response +="<H2><a href=\"/\">go home</a></H2><br>";
           response += "<script> alert(\"Settings Saved\"); </script>";
           server.send(200, "text/html", response);
           String ssid = String(server.arg("ssidDhcp"));
           String pass = String(server.arg("passkeyDhcp"));
          
           root["dhcpManual"]="dhcpManual";
           root["dhcpIP"] = "192.168.4.1";
           root["ssidDhcp"] = ssid;
           root["passkeyDhcp"] = pass;
           String JSONStatic;
           root.printTo(Serial);
           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
            }
            
           if(root.printTo(fileToWrite)){
                Serial.println("--File Written");
            }else{
                Serial.println("--Error Writing File");
              }
               fileToWrite.close();           
              
             
           isConnected = false;        
  }

//*************Helper 3 DHCP DEFAULT**************//
  
void dhcpSetDefault(){
           JsonObject& root =jsonBuffer.createObject();
           String response="<p>The dhcp IPv4 address is ";
           response += server.arg("configure");
           response +="</P><BR>";
           response +="<H2><a href=\"/\">go home</a></H2><br>";
           response += "<script> alert(\"Settings Saved\"); </script>";
           server.send(200, "text/html", response);
           String ssid = String(server.arg("ssidDhcp"));
           String pass = String(server.arg("passkeyDhcp"));
           root["dhcpDefault"]="dhcpDefault";
           root["dhcpIP"] = "192.168.4.1";
           root["ssidDhcp"] = ssid;
           root["passkeyDhcp"] = pass;
           root.printTo(Serial);
           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
            }
           if(root.printTo(fileToWrite)){
                Serial.println("--File Written");
            }else{
                Serial.println("--Error Writing File");
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
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  message +="<H2><a href=\"/\">go home</a></H2><br>";
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
 
   WiFi.mode(WIFI_AP);
   Serial.println(WiFi.softAP(ssidAPConfig,passAPConfig) ? "soft-AP setup": "Failed to connect");
   delay(100);
   Serial.println(WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet)? "Configuring Soft AP" : "Error in Configuration");      
  Serial.println(WiFi.softAPIP());
  server.begin();
  server.on("/", handleRoot); 
  server.on("/dhcp", handleDHCP);
  server.on("/static", handleStatic);
  server.onNotFound(handleNotFound);  
   
  APTimer = millis();
    
  while(isConnected && millis()-APTimer<= APInterval) {
        server.handleClient();  }       
   esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
   esp_deep_sleep_start();    
  }
  
//***************************STATIC Helper method**************************//

void  staticAPConfig(String IPStatic, String gateway, String subnet, String ssid, String pass){
           IPStatic.toCharArray(ipv4Arr,sizeof(IPStatic)+2);
           gateway.toCharArray(gatewayArr,sizeof(gateway)+2);
           subnet.toCharArray(subnetArr,sizeof(subnet)+2);
           ssid.toCharArray(ssidAP,sizeof(ssid)+2);
           pass.toCharArray(passAP, sizeof(pass)+2);
           Serial.print(ssidAP);
           Serial.print(passAP);
           byte ip[4];
           parseBytes(ipv4Arr,'.', ip, 4, 10);
           ip0 = (uint8_t)ip[0];
           ip1 = (uint8_t)ip[1];
           ip2 = (uint8_t)ip[2];
           ip3 = (uint8_t)ip[3];
           IPAddress ap_local(ip0,ip1,ip2,ip3);
           ap_localWeb_IP = ap_local;
           parseBytes(gatewayArr,'.', ip, 4, 10);
           ip0 = (uint8_t)ip[0];
           ip1 = (uint8_t)ip[1];
           ip2 = (uint8_t)ip[2];
           ip3 = (uint8_t)ip[3];
           IPAddress ap_gate(ip0,ip1,ip2,ip3);
           ap_Webgateway = ap_gate;
           parseBytes(subnetArr,'.', ip, 4, 10);
           ip0 = (uint8_t)ip[0];
           ip1 = (uint8_t)ip[1];
           ip2 = (uint8_t)ip[2];
           ip3 = (uint8_t)ip[3];
           IPAddress ap_net(ip0,ip1,ip2,ip3);  
           ap_Websubnet= ap_net;
          
           WiFi.disconnect(true);
           WiFi.mode(WIFI_AP);   
           Serial.println(WiFi.softAP(ssidAP,passAP) ? "Setting up SoftAP" : "error setting up");
           delay(100);       
           Serial.println(WiFi.softAPConfig(ap_localWeb_IP, ap_gate, ap_net) ? "Configuring softAP" : "kya yaar not connected");    
           Serial.println(WiFi.softAPIP());
           server.begin();
           server.on("/", handleStaticForm); 
           server.onNotFound(handleNotFound);
   
            STimer = millis();
            while(isAPConnected && millis()-STimer<= SInterval) {
               server.handleClient();  }       
           
}

//***************************WiFi Credintial Form**************************//

void dhcpAPConfig(String ssid, String pass){
      ssid.toCharArray(ssidAP,sizeof(ssid)+2);
      pass.toCharArray(passAP, sizeof(pass)+2);
      WiFi.mode(WIFI_OFF);
      WiFi.softAPdisconnect(true);
      delay(1000);
      WiFi.mode(WIFI_AP);
      Serial.println(WiFi.softAP(ssidAP,passAP) ? "Setting up SoftAP" : "error setting up");
      delay(200);
      Serial.println(WiFi.softAPIP());
      
      server.begin();
      server.on("/", handleStaticForm); 
      server.onNotFound(handleNotFound);
      DTimer = millis();
      while(isAPConnected && millis()-DTimer<= DInterval) {
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
//           File  file = SPIFFS.open("/WiFi.html", "r");
//           server.streamFile(file,"text/html");
//           file.close();
         
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
      String response="<p>The ssid is ";
      response += server.arg("ssid");
      response +="<br>";
      response +="And the password is ";
      response +=server.arg("passkey");
      response +="</P><BR>";
      response +="<H2><a href=\"/\">go home</a></H2><br>";
      server.send(200, "text/html", response);
      ROMwrite(String(server.arg("ssid")),String(server.arg("passkey")),String(server.arg("token")));
      isAPConnected = false;    
  }
  

//----------Write to ROM-----------//
void ROMwrite(String s, String p, String t){
 s+=";";
 write_EEPROM(s,0);
 p+=";";
 write_EEPROM(p,50);
 t+=";";
 write_EEPROM(t,100);
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
        string_Password.toCharArray(passWiFi,30);
        string_Ssid.toCharArray(ssidWiFi,30);
               
  delay(400);
  WiFi.begin(ssidWiFi,passWiFi);
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

