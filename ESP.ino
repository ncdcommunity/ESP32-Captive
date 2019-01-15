#include <ArduinoJson.h>
#include <WiFi.h>
#include "FS.h"
#include "SPIFFS.h"
#include <EEPROM.h>
#include <WebServer.h>

WebServer server(80);
DynamicJsonBuffer jsonBuffer;
   
//**********check for connection*************/
bool isConnected = true;
bool isDisconnect = false;

//**********softAPconfig Timer*************/
unsigned long APTimer = 0;
unsigned long APInterval = 120000;

//*********SSID and Pass for AP**************/
const char *ssidAP = "ESPuser";
const char *ssidAPWeb = "ESPWebUser";
const char *ssidDhcpWeb = "ESPDHCPUser";
const char *passDhcpWeb = "123456789";

// Please input the SSID and password of WiFi
//const char* ssid     = "NETGEAR";
//const char* password = "ab123456789";


JsonObject& root =jsonBuffer.createObject();
           
//*********Static IP Config**************/
IPAddress ap_local_IP(192,168,1,77);
IPAddress ap_gateway(192,168,1,254);
IPAddress ap_subnet(255,255,255,0);
IPAddress ap_dhcp(192,168,4,1);

//*********Static IP WebConfig**************/
IPAddress ap_localWeb_IP;
IPAddress ap_Webgateway;
IPAddress ap_Websubnet;
IPAddress ap_dhcpWeb_IP;

uint8_t ip0;
uint8_t ip1;
uint8_t ip2;
uint8_t ip3;

//*********IP Char Array**************/
char ipv4Arr[20];
char gatewayArr[20];           
char subnetArr[20];
char ipv4dhcpArr[20];

void setup()
{   
  Serial.begin(9600);
  while(!Serial);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  Serial.println(WiFi.softAP(ssidAP) ? "soft-AP setup": "Failed to connect");
  delay(100);
  Serial.println(WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet)? "Configuring Soft AP" : "Error in Configuration");    
  Serial.println(WiFi.softAPIP());
    SPIFFS.begin();

    server.begin();

    server.on("/", handleRoot); 
    server.on("/dhcp", handleDHCP);
    server.on("/static", handleStatic);
    server.onNotFound(handleNotFound);  
    
    APTimer = millis();
    
    while(isConnected && millis()-APTimer<= APInterval) {
        server.handleClient();  }       
}


void loop()
{
  
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


void handleDHCP(){
  File  file = SPIFFS.open("/page_dhcp.html", "r");
  server.streamFile(file,"text/html");
  file.close();}

void handleStatic(){
  File  file = SPIFFS.open("/page_static.html", "r");
  server.streamFile(file,"text/html");
  file.close();}

void staticSet(){
           
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
           root["statickey"]="staticSet";
           root["staticIP"] = ipv4static;
           root["gateway"] = gateway;
           root["subnet"] = subnet;
           String JSONStatic;
           char JSON[120];
           root.prettyPrintTo(JSONStatic);
           JSONStatic.toCharArray(JSON,sizeof(JSONStatic)+2);
           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
            }
           if(fileToWrite.print(JSON)){
                Serial.println("--File Written");
            }else{
                Serial.println("--Error Writing File");
              } 
              fileToWrite.close();            
           ipv4static.toCharArray(ipv4Arr,sizeof(ipv4static)+2);
           gateway.toCharArray(gatewayArr,sizeof(gateway)+2);
           subnet.toCharArray(subnetArr,sizeof(subnet)+2);
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
           Serial.println(WiFi.softAP(ssidAPWeb) ? "Setting up SoftAP" : "error setting up");
           delay(100);
           while(!(WiFi.softAPIP()== ap_localWeb_IP)){
               Serial.println(WiFi.softAPConfig(ap_localWeb_IP, ap_gate, ap_net) ? "Configuring softAP" : "kya yaar not connected");    
       }
    isConnected = false;     
    }

void dhcpSetManual(){
           String response="<p>The dhcp IPv4 address is ";
           response += server.arg("ipv4");
           response +="</P><BR>";
           response +="<H2><a href=\"/\">go home</a></H2><br>";
           response += "<script> alert(\"Settings Saved\"); </script>";
           server.send(200, "text/html", response);
           root["dhcpManual"]="dhcpManual";
           root["dhcpIP"] = "192.168.4.1";
           String JSONStatic;
           root.printTo(Serial);
           char JSON[120];
           root.prettyPrintTo(JSONStatic);
           JSONStatic.toCharArray(JSON,sizeof(JSONStatic)+2);
           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
            }
           if(fileToWrite.print(JSON)){
                Serial.println("--File Written");
            }else{
                Serial.println("--Error Writing File");
              }
              fileToWrite.close();           
           WiFi.disconnect(true);
           delay(1000);
           WiFi.mode(WIFI_AP);
           Serial.println(WiFi.softAP(ssidDhcpWeb, passDhcpWeb) ? "Setting up SoftAP" : "error setting up");
           delay(100);
           isConnected = false;        
  }
  
void dhcpSetDefault(){
           String response="<p>The dhcp IPv4 address is ";
           response += server.arg("configure");
           response +="</P><BR>";
           response +="<H2><a href=\"/\">go home</a></H2><br>";
           response += "<script> alert(\"Settings Saved\"); </script>";
           server.send(200, "text/html", response);
           root["dhcpDefault"]="dhcpDefault";
           root["dhcpIP"] = "192.168.4.1";
           String JSONStatic;
           char JSON[120];
           root.printTo(Serial);
           root.prettyPrintTo(JSONStatic);
           JSONStatic.toCharArray(JSON,sizeof(JSONStatic)+2);
           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
            }
           if(fileToWrite.print(JSON)){
                Serial.println("--File Written");
            }else{
                Serial.println("--Error Writing File");
              }           
           fileToWrite.close();  
           WiFi.disconnect(false);
           WiFi.enableAP(false);
           InitAP();  
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


void InitAP(){
   WiFi.enableAP(true);
   WiFi.mode(WIFI_AP);
   Serial.println(WiFi.softAP(ssidDhcpWeb,passDhcpWeb) ? "SoftAP setup" : "Error Setting UP AP" );
   delay(100);
   Serial.println(WiFi.softAPConfig(ap_dhcp, ap_gateway, ap_subnet)? "Configuring Soft AP" : "Error in Configuration");  
   Serial.println(WiFi.softAPIP());
  }

  
