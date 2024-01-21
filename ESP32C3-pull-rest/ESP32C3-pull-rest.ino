/*
 * Alex McGuinness 
 * 08/01/2024
 * 
 * This sketch will set a PIN high or low depending on communication via a REST/HTTP interface
 * PUT /lights/on          - Sets the pin high and turns on the lights
 * PUT /lights/off         - Sets the pin low and turns off the lights
 * GET /lights/status      - gets the current state of the lights
 * GET /admin/vcc          - gets the VCC voltage
 * 
 * The WiFi configuration can be set with new SSID/Password 
 * POST /admin/wifi
 */

#include <EEPROM.h> //The EEPROM libray 
#include "WiFi.h"
#include <Arduino_JSON.h>
//#include <ArduinoJson.h>
#include <HTTPClient.h>
//#include "EspMQTTClient.h"
#include <PubSubClient.h>

//EEPROM Details for non-volatile config
const int EEPROM_SIZE              = 512;
const int WIFI_DATA_START_ADDR     = 0;
const int PIN_DATA_START_ADDR      = 150;
const int MAX_WIFI_SSID_LENGTH     = 32;
const int MAX_WIFI_PASSWORD_LENGTH = 63;
//const int PORT_STR_LENGTH          = 7;
const int IP_ADDR_LENGTH           = 17;
const int MAX_MQTT_USER_LENGTH     = 12; 
const int MAX_MQTT_PASSWORD_LENGTH = 32;
const int MAX_MQTT_TOPIC_LENGTH    = 32;



const int WIFI_CLIENT_READ_TIMEOUT_MS = 5000;

const int AP_SERVER_PORT = 80;
const int CONNECT_TIMEOUT_MS  = 30000;  //How long it should take to connect
const int CONNECT_INTERVAL_MS = 500;    //How long it should wait inbetween attempts
//
////REST API Defs
const char* const REST_PUT          ="PUT";
const char* const REST_POST         ="POST";
const char* const REST_GET          ="GET";
const char* const ADMIN_COMMAND     ="admin";
const char* const LIGHTS_COMMAND    ="lights";
const char* const LIGHTS_ON         ="on";
const char* const LIGHTS_OFF        ="off";
const char* const LIGHTS_STATUS     ="status";
const char* const VCC               ="vcc";
const char* const WIFI              ="wifi";
const char* const WIFI_SSID         ="ssid";
const char* const WIFI_PASSWORD     ="wifi-password";

const char* const SERVER_IP         = "server-ip"; 
const char* const SERVER_PORT       = "server-port"; 
const char* const MQTT_IP           = "mqtt-ip";
const char* const MQTT_PORT         = "mqtt-port"; 
const char* const MQTT_USER         = "mqtt-user";
const char* const MQTT_PASSWORD     = "mqtt-password"; 
const char* const MQTT_TOPIC        = "mqtt-topic";


//JSON Defs
const char* const LIGHTS_STATUS_UNKNOWN = "unknown";
const char* const LIGHTS_STATUS_ON = "on";
const char* const LIGHTS_STATUS_OFF = "off";
const char* const JSON_LIGHTS_TAG = "lights";
const char* const JSON_RESET_TAG = "reset";
const char* const RESET_STATUS_TRUE = "true";
const char* const JSON_VCC_TAG = "vcc";
const char* const JSON_WIFI_RESET_TAG = "wifi-reset";
const char* const JSON_ERROR_TAG = "error";
const char* const POST_RESP_FAILURE = "Failure";
const char* const POST_RESP_SUCCESS = "Success";

//HTTP
const int WIFI_WEB_PAGE_SIZE = 2560;
const int JSON_RESPONSE_SIZE = 384;
const char* const HTTP_VERSION = "HTTP/1.1";
const char* const HTTP_CODE = "200 OK";
const char* const HTTP_HDR_CONTENT_TYPE_TAG = "Content-Type";
const char* const HTTP_HDR_CONTENT_TYPE_HTML = "text/html";
const char* const HTTP_HDR_CONTENT_TYPE_JSON = "application/json";
const char* const HTTP_HDR_CONTENT_LEN_TAG = "Content-Length";
const char* const END_POINT_SEPARATOR      = "/";
const char* const HTTP_LINE_SEPARATOR      = " ";
const char* const POST_PARAM_SEPARATOR     = "&";
const char* const POST_KEY_VALUE_SEPARATOR = "=";
const char* const JSON_ERROR_BAD_SSID_LEN = "Invalid SSID Length";
const char* const JSON_ERROR_BAD_PWD_LEN = "Invalid password Length";
const char* const JSON_ERROR_BAD_SERVER_IP_LEN = "Invalid Server IP length";
const char* const JSON_ERROR_BAD_SERVER_PORT_LEN = "Invalid Server Port Length";
const char* const JSON_ERROR_BAD_MQTT_IP_LEN = "Invalid MQTT IP length";
const char* const JSON_ERROR_BAD_MQTT_PORT_LEN = "Invalid MQTT Port Length";
const char* const JSON_ERROR_BAD_MQTT_USER_LEN = "Invalid MQTT User Length";
const char* const JSON_ERROR_BAD_MQTT_PASSWORD_LEN = "Invalid MQTT Password Length";
const char* const JSON_ERROR_BAD_MQTT_TOPIC_LEN = "Invalid MQTT Port Length";


const char* const SET_WIFI_URL = "/admin/wifi";
const char* const SET_CONFIG_COMMAND = "/settings";

//const byte BUILTIN_LED_PIN=2;
//const byte BUILTIN_TX_PIN=1;
//const byte BUILTIN_RX_PIN=3;

/**
 * Set default values for factory reset
 */
const char* const FR_WIFI_SSID     = "wifi-ssid";
const char* const FR_WIFI_PASS     = "wifipassword";
const char* const FR_SERVER_IP     = "0.0.0.0";
const unsigned int FR_SERVER_PORT   = (short)0;
const char* const FR_MQTT_IP       = "0.0.0.0";
const unsigned int FR_MQTT_PORT     = (short)0;
const char* const FR_MQTT_USER     = "mqtt";
const char* const FR_MQTT_PASSWORD = "mqttpass";
const char* const FR_MQTT_TOPIC    = "/mqtt/topic";


 
/**
 * Define AP WIFI Details
 */
const char* const AP_SSID = "ESP32C3";
const char* const AP_PASS = "bruckfexit";
const int AP_MAX_CON = 1;

const IPAddress AP_LOCAL_IP(192,168,1,1);
const IPAddress AP_GATEWAY(192,168,1,9);
const IPAddress AP_SUBNET(255,255,255,0);

const short SERVER_MODE = 1;
const short CLIENT_MODE = 2;
 
// define led according to pin diagram
int ON_PIN = D10;
int OFF_PIN = D1;

int VCC_PIN = A0;


//Definition of structure to be stored in memory
struct storeStruct_t{
  char myVersion[3];
  char ssid[MAX_WIFI_SSID_LENGTH];
  char wifiPassword[MAX_WIFI_PASSWORD_LENGTH];
  char serverIPAddress[IP_ADDR_LENGTH];
  uint16_t serverPort;
  char mqttIPAddress[IP_ADDR_LENGTH];
  uint16_t mqttPort;
  char mqttUser[MAX_MQTT_USER_LENGTH];
  char mqttPassword[MAX_MQTT_PASSWORD_LENGTH];
  char mqttTopic[MAX_MQTT_TOPIC_LENGTH];
};

// Create an instance of the server
WiFiServer server(AP_SERVER_PORT);

bool g_pin_state = false;
short g_mode = SERVER_MODE;


storeStruct_t g_settings = {
  "01",
  *FR_WIFI_SSID,
  *FR_WIFI_PASS,
  *FR_SERVER_IP,
  FR_SERVER_PORT,
  *FR_MQTT_IP,
  FR_MQTT_PORT,
  *FR_MQTT_USER,
  *FR_MQTT_PASSWORD,
  *FR_MQTT_TOPIC  
};



void setup() {
  Serial.begin(115200);
  // initialize digital pin led as an output
  pinMode(ON_PIN, OUTPUT);
  pinMode(OFF_PIN, OUTPUT);

  pinMode(VCC_PIN, INPUT);  

//This stop it running on battery
//  while(!Serial);
//  Serial.println("Starting...");


  loadSettingsData();
  loadPinState();  
  //setLights(g_pin_state);

  //Connect to configured network
  if(!connectToWiFiNetwork()){
    setupAccessPoint();    
    server.begin();
    g_mode = SERVER_MODE;
    Serial.println("Server started");
  } else {
    Serial.println("Client started");    
    g_mode = CLIENT_MODE;
  }
}


/**
 * Sets the PIN to be the state
 */
void setLights(bool aState){
   g_pin_state = aState;

   int p1state = LOW;
   int p2state = HIGH;

   if(g_pin_state){
      p1state = HIGH;
      p2state = LOW;
   }
     
  Serial.print("PIN:");
  Serial.print(ON_PIN);
  Serial.print(":");
  Serial.println(p1state);
  Serial.print("PIN:");
  Serial.print(OFF_PIN);
  Serial.print(":");
  Serial.println(p2state);

  digitalWrite(ON_PIN, p1state);
  digitalWrite(OFF_PIN, p2state);
//  digitalWrite(BUILTIN_TX_PIN, val);
//  digitalWrite(BUILTIN_RX_PIN, val);

   savePinState();  


//  Serial.println("LED on D10 on");
//  digitalWrite(led1, HIGH);   // turn the LED on   
//  delay(5000);               // wait for a second
//  Serial.println("LED on D10 off");
//  digitalWrite(led1, LOW);    // turn the LED off
//  
//  delay(1000);               // wait for a second
//
//  Serial.println("LED on D1 on");
//  digitalWrite(led2, HIGH);   // turn the LED on   
//  delay(5000);               // wait for a second
//  Serial.println("LED on D1 off");
//  digitalWrite(led2, LOW);    // turn the LED off
//  delay(1000);               // wait for a second

   
}


/**
 * Connect to wifi network
 */
int connectToWiFiNetwork(){

  bool rc = false;

  Serial.print("WL_IDLE_STATUS:");
  Serial.println(WL_IDLE_STATUS);
  Serial.print("WL_NO_SSID_AVAIL:");
  Serial.println(WL_NO_SSID_AVAIL);
  Serial.print("WL_SCAN_COMPLETED:");
  Serial.println(WL_SCAN_COMPLETED);  
  Serial.print("WL_CONNECTED:");
  Serial.println(WL_CONNECTED);
  Serial.print("WL_CONNECT_FAILED:");
  Serial.println(WL_CONNECT_FAILED);
  Serial.print("WL_CONNECTION_LOST:");
  Serial.println(WL_CONNECTION_LOST);  
//  Serial.print("WL_WRONG_PASSWORD:");
//  Serial.println(WL_WRONG_PASSWORD);  
  Serial.print("WL_DISCONNECTED:");
  Serial.println(WL_DISCONNECTED);  
  Serial.print("WL_NO_SHIELD:");
  Serial.println(WL_NO_SHIELD);

  int attempts = CONNECT_TIMEOUT_MS / CONNECT_INTERVAL_MS;

  // Connect to WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(g_settings.ssid);  
  Serial.print(" with password:");
  Serial.print(g_settings.wifiPassword);
  Serial.print(" for ");
  Serial.print(CONNECT_TIMEOUT_MS);
  Serial.print("ms (");
  Serial.print(attempts);
  Serial.println(") attempts");
  WiFi.begin(g_settings.ssid, g_settings.wifiPassword);

  /**
   * Don't assign a static IP address, let the router decide and if the same IP is desired
   * configure the router to reserve an IP for this MAC address.
   * That way we are not assigning a static address the router doesn't know about
   */
  while ((WiFi.status() != WL_CONNECTED) && (attempts > 0)) {
    delay(CONNECT_INTERVAL_MS);
    Serial.print(".");
    Serial.print(WiFi.status());

    --attempts;
  }

  if(WiFi.status() == WL_CONNECTED) {
     Serial.print("\nWiFi connected with IP ");
     Serial.println(WiFi.localIP());    
  
     rc = true;
  } else {
     Serial.print("\nUnable to connect to ");
     Serial.print(g_settings.ssid);
     Serial.print(" using ");
     Serial.println(g_settings.wifiPassword);
     WiFi.disconnect();
  }

  return rc;
}



/**
 * Sets up the wireless access point, so the ESP8266 can be accessed with no wifi details
 */
void setupAccessPoint(){

  WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY, AP_SUBNET);

  Serial.print("Starting as Access Point: Connect to ");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS, /*channel no*/ NULL, /*hidden ssid*/ NULL, AP_MAX_CON);

  Serial.print(AP_SSID);
  Serial.print(" using ");
  Serial.println(AP_PASS);
  Serial.println("Then access the URL:");
  Serial.print("http://");
  Serial.print(WiFi.softAPIP());
  Serial.print(SET_WIFI_URL);
  Serial.print("\n");
  Serial.println("And set new password.");

  uint8_t macAddr[6];
  WiFi.softAPmacAddress(macAddr);
  Serial.printf("ESP32C3 MAC address = %02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);

//  Serial.print("Local IP:");
//  Serial.println(WiFi.localIP());   
//  Serial.print("\n");
}

/**
 * Sets up the wireless access point, so the ESP8266 can be accessed with no wifi details
 */
void removeAccessPoint(){

  Serial.print("Removing Access Point.");  
  WiFi.softAPdisconnect (true);
  WiFi.mode(WIFI_AP);  
}
 
/**
 * Reads in from EEPROM Memory the WIFI settings
 */
void loadSettingsData(){

  Serial.println("WiFi Config from EEPROM");
  storeStruct_t load;
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get( WIFI_DATA_START_ADDR, load);
  EEPROM.end();

  Serial.println("-----------------------");
  Serial.print("SSID:");
  Serial.println(load.ssid);
  Serial.print("Password:");
  Serial.println(load.wifiPassword);
  Serial.print("Server IP:");
  Serial.println(load.serverIPAddress);
  Serial.print("ServerPort:");
  Serial.println((uint16_t)load.serverPort);
  Serial.print("MQTT IP:");
  Serial.println(load.mqttIPAddress);
  Serial.print("MQTT Port:");
  Serial.println((uint16_t)load.mqttPort);
  Serial.print("MQTT User:");
  Serial.println(load.mqttUser);
  Serial.print("MQTT Password:");
  Serial.println(load.mqttPassword);
  Serial.print("Topic Name:");
  Serial.println(load.mqttTopic);
  Serial.println("-----------------------");
  Serial.print("size:");
  Serial.println(sizeof(load));
  Serial.println("-----------------------");

  g_settings = load;  
}


/**
 * Stores the WiFi config in EEPROM
 */
void saveSettingsData() {
  
  Serial.println("Saving WiFi Config to EEPROM.");
  Serial.print("   G_SETTINGS_SSID:");
  Serial.println(g_settings.ssid);
  Serial.print("   G_SETTINGS_PASSWORD:");
  Serial.println(g_settings.wifiPassword);
  Serial.print("   G_SETTINGS_SERVER_IP:");
  Serial.println(g_settings.serverIPAddress);
  Serial.print("   G_SETTINGS_SERVER_PORT:");
  Serial.println((unsigned short)g_settings.serverPort);
  Serial.print("   G_SETTINGS_MQTT_IP:");
  Serial.println(g_settings.mqttIPAddress);
  Serial.print("   G_SETTINGS_MQTT_PORT:");
  Serial.println((unsigned short)g_settings.mqttPort);
  Serial.print("   G_SETTINGS_MQTT_USER:");
  Serial.println(g_settings.mqttUser);
  Serial.print("   G_SETTINGS_MQTT_PASSWORD:");
  Serial.println(g_settings.mqttPassword);
  Serial.print("   G_SETTINGS_TOPIC_NAME:");
  Serial.println(g_settings.mqttTopic);


  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put (WIFI_DATA_START_ADDR, g_settings);
  EEPROM.commit();
  EEPROM.end();
}

/**
 * Stores PIN State in EEPROM
 */
void savePinState() {
  Serial.print("Saving PIN State:");
  Serial.println(g_pin_state);

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put (PIN_DATA_START_ADDR, g_pin_state);
  EEPROM.commit();
  EEPROM.end();
}

/**
 * Stores PIN State in EEPROM
 */
void loadPinState() {
  Serial.print("Loading PIN State:");

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get( PIN_DATA_START_ADDR, g_pin_state);
  EEPROM.end();
  Serial.println(g_pin_state);  
}

/**
 * Splits a string into an array
 */
void split(char* aString, const char *aSeparator, char *aArray[], const int aMax){

//  for(int j=0; j<sizeof(aArray); j++){
//    aArray[j] = *aString[sizeof(aString)-1];    
//  }

  int i=0;
  char* token = strtok(aString, aSeparator);

  while ((token != NULL) && (i<=aMax)){
//    Serial.println(i);
//    Serial.print("token found:");
//    Serial.print(token);
//    Serial.print(" at ");
//    Serial.println(i);       
    aArray[i] = token;
    token = strtok(0, aSeparator);
    ++i;
  }
  
  return;
}

/**
 * Gets the number of occurrances of a char in a given string
 */
int getNumOccurrances(char* aString, const char* aChar){
  int i = 0;
  char *pch=strchr(aString, *aChar);

  while (pch!=NULL) {
    i++;
    pch=strchr(pch+1, *aChar);
  }

  return i;
}

/**
 * Gets the HTTP Header
 */
void getHeader(WiFiClient& aClient, int& aBodySize){

   bool header_found = false;
   int newline = 0;

//   int num_lines = 0;
//   int max_header_line = 20;

//   char *hdr_char_buf = (char*)malloc(45);

//   while(!header_found && num_lines < max_header_line){
   while(!header_found){
      //HTTP protocol ends each line wi CRLF
//      String hdr_line = aClient.readStringUntil('\r');
      String hdr_line = aClient.readStringUntil('\n');

//      Serial.println(hdr_line.length());
      Serial.print("HEADER LINE[");
      Serial.print(hdr_line.length());
      Serial.print("]:");
      Serial.println(hdr_line);

      Serial.print("[0]:0x");
      Serial.println(hdr_line[0], HEX);


      char* hdr_line_elements[2];
      int hdr_line_len = hdr_line.length();
      char hdr_char_buf[hdr_line_len+1];

      hdr_line.toCharArray(hdr_char_buf, hdr_line_len+1);
      
      split(hdr_char_buf, ":", hdr_line_elements, 1);

      if(NULL != strstr(hdr_line_elements[0], HTTP_HDR_CONTENT_LEN_TAG)){
         Serial.print("Found length:");
         Serial.println(hdr_line_elements[1]);      
         aBodySize = atoi(hdr_line_elements[1]);
      }        

      if (hdr_line=="\r"){
           Serial.println("END OF HEADER");
//         Serial.println(hdr_line);      
           header_found = true;
       }    
   }
}

/**
 * Handles requests that are not supported, such as PATCH, DELETE
 */
void handleUnsupported(WiFiClient& aClient, char* command){
  Serial.print(command);  
  Serial.println(" is not supported");  
}


/**
 * Handles lights status request
 */
void handleLightsView(WiFiClient& aClient, char** aCommandArray){
  Serial.print("handleLightsView:");  
  Serial.println(aCommandArray[1]);  
  Serial.print(" len:");  
  Serial.println(strlen(aCommandArray[1]));  

  int val = 0;
  JSONVar json;
  const char * status = LIGHTS_STATUS_UNKNOWN;
  if (0==strcmp(LIGHTS_STATUS, aCommandArray[1])){
     status = LIGHTS_STATUS_OFF;
     if (g_pin_state == true){
        status = LIGHTS_STATUS_ON;
     } 
  } else {
    handleUnsupported(aClient, aCommandArray[1]);    
  }
  json[JSON_LIGHTS_TAG] = status;

  // Prepare the response
  char response[JSON_RESPONSE_SIZE];
  sprintf(response, "%s %s\r\n" 
         "%s: %s\r\n\r\n"  
         "%s\r\n", HTTP_VERSION, HTTP_CODE, HTTP_HDR_CONTENT_TYPE_TAG, HTTP_HDR_CONTENT_TYPE_JSON,
         JSON.stringify(json).c_str());

  
  Serial.print("RESPONSE:");  
  Serial.println(response);  

  // Send the response to the client
  aClient.flush();
  aClient.print(response);
}

/**
 * Handles VCC request
 */
void handleVccView(WiFiClient& aClient){
  Serial.print("handleVccView - VCC:");  

  JSONVar json;
  float vcc = 0;
//  vcc = ESP.getVcc()/1000.00;
  Serial.println(vcc);

  json[JSON_VCC_TAG] = vcc;

  // Prepare the response
  char response[JSON_RESPONSE_SIZE];
  sprintf(response, "%s %s\r\n" 
         "%s: %s\r\n\r\n"  
         "%s\r\n", HTTP_VERSION, HTTP_CODE, HTTP_HDR_CONTENT_TYPE_TAG, HTTP_HDR_CONTENT_TYPE_JSON,
         JSON.stringify(json).c_str());

  Serial.print("RESPONSE:");  
  Serial.println(response);  

  // Send the response to the client
  aClient.flush();
  aClient.print(response);
}


/**
 * Handles all Get Requests 
 * These will not change the state of the system
 *    light status
 *    VCC value
 */
void handleGet(WiFiClient& aClient, char* aEndPoint){
  int num_seps = getNumOccurrances(aEndPoint, END_POINT_SEPARATOR);
  
  char* ep_commands[num_seps];
  split(aEndPoint, END_POINT_SEPARATOR, ep_commands, num_seps);

  Serial.print("End Point:");
  Serial.println(aEndPoint);      

  if(0==strcmp(ADMIN_COMMAND, ep_commands[0])){
    handleAdminView(aClient, ep_commands);
  } else if (0==strcmp(LIGHTS_COMMAND, ep_commands[0])){
    handleLightsView(aClient, ep_commands);
  } else {
    handleUnsupported(aClient, aEndPoint);    
  }

  
  Serial.print("Get Complete\n");

}

/**
 * Handles all Put Requests
 *    lights on/off
 */
void handlePut(WiFiClient& aClient, char* aEndPoint){
  int num_seps = getNumOccurrances(aEndPoint, END_POINT_SEPARATOR);

  char* ep_commands[num_seps];
  split(aEndPoint, END_POINT_SEPARATOR, ep_commands, num_seps);

  if (0==strcmp(LIGHTS_COMMAND, ep_commands[0])){
    handleLightsChange(aClient, ep_commands);
  } else {
    handleUnsupported(aClient, aEndPoint);    
  }  
}

/**
 * Handles all Put Requests
 */
void handlePost(WiFiClient& aClient, char* aEndPoint){

  Serial.print("End Point:");
  Serial.println(aEndPoint);      
  if(0==strcmp(SET_CONFIG_COMMAND, aEndPoint)){
     handleUpdateSettings(aClient);
  } else {
     handleUnsupported(aClient, aEndPoint);    
  }
}


/**
 * Handles admin commands
 */
void handleAdminView(WiFiClient& aClient, char** aCommandArray){

   Serial.print("handleAdminView:");
   Serial.println(aCommandArray[1]);

  JSONVar json;
  if(0==strcmp(WIFI, aCommandArray[1])){
    showSettingsChangeForm(aClient);     
//    Serial.print("Here101\n");
  }else if(0==strcmp(VCC, aCommandArray[1])){
    handleVccView(aClient);
  } else {
    handleUnsupported(aClient, aCommandArray[1]);    
  }
  Serial.print("Admin View Complete\n");

}


/**
 * Handles lights commands
 */
void handleLightsChange(WiFiClient& aClient, char** aCommandArray){
  Serial.print("handleLightsChange:");  
  Serial.println(aCommandArray[1]);  

  int val = 0;

  JSONVar json;
  const char * status = LIGHTS_STATUS_UNKNOWN;
  if(0==strcmp(LIGHTS_ON, aCommandArray[1])){
     status = LIGHTS_STATUS_ON;
     setLights(true);
  } else if (0==strcmp(LIGHTS_OFF, aCommandArray[1])){
    status = LIGHTS_STATUS_OFF;
    setLights(false);
  } else {
    handleUnsupported(aClient, aCommandArray[1]);    
  }
  json[JSON_LIGHTS_TAG] = status;

  // Prepare the response
  char response[JSON_RESPONSE_SIZE];
  sprintf(response, "%s %s\r\n" 
         "%s: %s\r\n\r\n"  
         "%s\r\n", HTTP_VERSION, HTTP_CODE, HTTP_HDR_CONTENT_TYPE_TAG, HTTP_HDR_CONTENT_TYPE_JSON,
         JSON.stringify(json).c_str());
  
  Serial.print("RESPONSE:");  
  Serial.println(response);  

  // Send the response to the client
  aClient.flush();
  aClient.print(response);
}



/**
 * Creates WiFi SSID/ Password details page
 */
void showSettingsChangeForm(WiFiClient& aClient){

   Serial.println("Showing Wifi Change Form:");
  char web_page[WIFI_WEB_PAGE_SIZE];

   sprintf(web_page, "%s %s\r\n" 
         "%s: %s\r\n\r\n"  
         "<head>\n"
         "   <style>\n"
         "      body {\n"
         "         color: white;\n"
         "         background-color: black;\n"
         "      }\n"
         "      #contact-form fieldset {\n"
         "         width:70%;}\n"
         "      #contact-form li {\n"
         "         margin:10px; list-style: none;}\n"
         "      #contact-form input {\n"
         "         margin-left:45px; text-align: left;}\n"
         "      #contact-form textarea {\n"
         "         margin-left:10px; text-align: left;}\n"
         "   </style>\n"
         "</head>\n"
         "<body>\n"
         "   <form id=\"contact-form\" action=\"%s\" method=\"post\">\n"
         "      <fieldset>\n"
         "         <legend>WiFi Configuration</legend>\n"
         "         <ul>\n"
         "            <li>\n"
         "               <label for=\"ssid\">SSID:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" />\n"
         "            </li>\n"        
         "            <li>\n"
         "               <label for=\"wifi-password\">Password:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" />\n"
         "            </li>\n"
         "         </ul>\n"
         "         <legend>Server Configuration</legend>\n"
         "         <ul>\n"
         "            <li>\n"
         "               <label for=\"server-ip\">Server IP Address:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" />\n"
         "            </li>\n"        
         "            <li>\n"
         "               <label for=\"server-port\">Server Port:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" />\n"
         "            </li>\n"
         "         </ul>\n"
         "         <legend>MQTT Configuration</legend>\n"
         "         <ul>\n"
         "            <li>\n"
         "               <label for=\"mqtt-ip\">MQTT IP Address:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" />\n"
         "            </li>\n"        
         "            <li>\n"
         "               <label for=\"mqtt-port\">MQTT Port:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" />\n"
         "            </li>\n"
         "            <li>\n"
         "               <label for=\"mqtt-user\">MQTT IP User:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" />\n"
         "            </li>\n"        
         "            <li>\n"
         "               <label for=\"mqtt-password\">MQTT Password:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" />\n"
         "            </li>\n"
         "            <li>\n"
         "               <label for=\"mqtt-topic\">MQTT Topic:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" />\n"
         "            </li>\n"
         "            <li class=\"button\">\n"
         "               <button type=\"submit\">Set</button>\n"
         "            </li>\n"
         "         </ul>\n"

         "      </fieldset>\n"
         "   </form>\n"
         "</body>\n",
         HTTP_VERSION, HTTP_CODE, HTTP_HDR_CONTENT_TYPE_TAG, HTTP_HDR_CONTENT_TYPE_HTML,
         SET_CONFIG_COMMAND, WIFI_SSID, WIFI_SSID, WIFI_PASSWORD, WIFI_PASSWORD, 
         SERVER_IP, SERVER_IP, SERVER_PORT, SERVER_PORT, MQTT_IP, MQTT_IP, MQTT_PORT, MQTT_PORT,
         MQTT_USER, MQTT_USER, MQTT_PASSWORD, MQTT_PASSWORD, MQTT_TOPIC, MQTT_TOPIC );

  Serial.print("Size of web page:");
  Serial.println(strlen(web_page));

  aClient.flush();
  aClient.print(web_page);

  Serial.print("Show Complete\n");

}

/**
 * This handles the POST command that sets the WiFi Credentials
 */
void handleUpdateSettings(WiFiClient& aClient){

   int body_size_from_hdr = 0;
   bool found_ssid = false;
   bool found_password = false;
   bool found_server_ip = false;
   bool found_server_port = false;
   bool found_mqtt_ip = false;
   bool found_mqtt_port = false;
   bool found_mqtt_user = false;
   bool found_mqtt_password = false;
   bool found_mqtt_topic = false;
   
   JSONVar json;
   json[JSON_WIFI_RESET_TAG] = POST_RESP_FAILURE;

   //Reads the header
   getHeader(aClient, body_size_from_hdr);

   Serial.print("Body length from Header:");
   Serial.println(body_size_from_hdr);      

   //Reads Body
//   Serial.println("Here 401");    
   aClient.setTimeout(500);

//   String body = "test";
   char body_buf[body_size_from_hdr+1];
   memset(body_buf, '\0', body_size_from_hdr+1);
   
   char* body_p = body_buf;
//   aClient.readBytes(body_buf, 1); //read newline
//   aClient.readBytes(body_buf, body_size_from_hdr+1);
   int b_read = aClient.readBytes(body_buf, body_size_from_hdr);
   Serial.print("Bytes Read:");    
   Serial.println(b_read);    
   Serial.print("buffer:");    
   Serial.println(body_buf);    
   
   //String body = aClient.readString(); 
//   String body = aClient.readStringUntil('\r');
//   Serial.println("Here 402");    
//   body.trim();
//   Serial.println("Here 403");    
   
//   Serial.print("HTTP Body:");
//   Serial.println(buffer);      
//
//   int read_body_len = body.length();
//   Serial.print("Body length read:");
//   Serial.println(read_body_len);           
//     
//   char body_buf[read_body_len+1]; //for terminator
//   body.toCharArray(body_buf, read_body_len+1);

   if('\n' == body_buf[0]){
      Serial.println("First char is newline removing");
//      *body_buf++;
       ++body_p;
   }
//   Serial.print("First char is:");
//   Serial.println(body_buf[0]);

   //Gets the number of parameters (should be 2, ssid and password)
//   int num_sep = getNumOccurrances(body_buf, POST_PARAM_SEPARATOR);
   int num_sep = getNumOccurrances(body_p, POST_PARAM_SEPARATOR);
   Serial.print("num of parameters:");
   Serial.println(num_sep);

   if (num_sep > 0){
      char* parameters[num_sep+1];
//      split(body_buf, POST_PARAM_SEPARATOR, parameters, num_sep);
      split(body_p, POST_PARAM_SEPARATOR, parameters, num_sep);
      Serial.print("params[0]:");
      Serial.println(parameters[0]);
      Serial.print("params[1]:");
      Serial.println(parameters[1]);


      for(int i=0; i<(num_sep+1); i++){
         //Processes each parameter separately
         int kv_sep = getNumOccurrances(parameters[i], POST_KEY_VALUE_SEPARATOR);

         if (kv_sep > 0){ //check 'equals' is found and therefore parameter is valid
            char* key_value[2] = {NULL, NULL};
            split(parameters[i], POST_KEY_VALUE_SEPARATOR, key_value, 1);


            if(0==strcmp(WIFI_SSID, key_value[0])){
               Serial.print("Found SSID:");
               Serial.print("SSID Len:");
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_WIFI_SSID_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_SSID_LEN;
                  Serial.println(JSON_ERROR_BAD_SSID_LEN);
               } else {               
                 Serial.println(strlen(key_value[1]));
                 strcpy(g_settings.ssid, key_value[1]);
                 found_ssid = true;
               }
               Serial.print(":");
               Serial.println(key_value[1]);
            } else if (0==strcmp(WIFI_PASSWORD, key_value[0])){
               Serial.print("Found Password:");
               Serial.print("Password Len:");
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_WIFI_PASSWORD_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_PWD_LEN;
                  Serial.println(JSON_ERROR_BAD_PWD_LEN);
               } else {               
                  Serial.println(strlen(key_value[1]));
                  strcpy(g_settings.wifiPassword, key_value[1]);
                  found_password = true;
               }
               Serial.print(":");
               Serial.println(key_value[1]);
            } else if (0==strcmp(SERVER_IP, key_value[0])){
               Serial.print("Found Server IP:");
               Serial.print("Server IP Len:");
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > IP_ADDR_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_SERVER_IP_LEN;
                  Serial.println(JSON_ERROR_BAD_SERVER_IP_LEN);
               } else {               
                  Serial.println(strlen(key_value[1]));
                  strcpy(g_settings.serverIPAddress, key_value[1]);
                  found_server_ip = true;
               }
               Serial.print(":");
               Serial.println(key_value[1]);
            } else if (0==strcmp(SERVER_PORT, key_value[0])){
               Serial.print("Found Server Port:");
               Serial.println(key_value[1]);

               uint16_t value = (uint16_t)strtol(key_value[1], NULL, 10);
               if((value < 1023) ||
                  (value > 65535)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_PORT_LEN;
                  Serial.println(JSON_ERROR_BAD_MQTT_PORT_LEN);
               } else {
                  //Serial.println(strlen(key_value[1]));
                  //strcpy(g_settings.mqttPort, key_value[1]);
                  g_settings.serverPort = value;
                  found_server_port = true;                
               }
               
//               Serial.print("Server Port Len:");
//               if((NULL == key_value[1]) ||
//                  (strlen(key_value[1]) > PORT_STR_LENGTH) ||
//                  (strlen(key_value[1]) <= 0)){
//                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_SERVER_PORT_LEN;
//                  Serial.println(JSON_ERROR_BAD_SERVER_PORT_LEN);
//               } else {               
//                  Serial.println(strlen(key_value[1]));
//                  //strcpy(g_settings.serverPort, key_value[1]);
//                  g_settings.serverPort = (unsigned short)strtol(key_value[1], NULL, 10);
//                  found_server_port = true;
//               }
               Serial.print(":");
               Serial.println(key_value[1]);
            } else if (0==strcmp(MQTT_IP, key_value[0])){
               Serial.print("Found MQTT IP:");
               Serial.print("MQTT IP Len:");
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > IP_ADDR_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_IP_LEN;
                  Serial.println(JSON_ERROR_BAD_MQTT_IP_LEN);
               } else {               
                  Serial.println(strlen(key_value[1]));
                  strcpy(g_settings.mqttIPAddress, key_value[1]);
                  found_mqtt_ip = true;
               }
               Serial.print(":");
               Serial.println(key_value[1]);
            } else if (0==strcmp(MQTT_PORT, key_value[0])){
               Serial.print("Found MQTT Port:");
               Serial.println(key_value[1]);

               uint16_t value = (uint16_t)strtol(key_value[1], NULL, 10);
               if((value < 1023) ||
                  (value > 65535)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_PORT_LEN;
                  Serial.println(JSON_ERROR_BAD_MQTT_PORT_LEN);
               } else {
                  //Serial.println(strlen(key_value[1]));
                  //strcpy(g_settings.mqttPort, key_value[1]);
                  g_settings.mqttPort = value;
                  found_mqtt_port = true;                
               }

               
               //Serial.print("MQTT Port Len:");               
//               if((NULL == key_value[1]) ||
//                  (strlen(key_value[1]) > PORT_STR_LENGTH) ||
//                  (strlen(key_value[1]) <= 0)){
//                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_PORT_LEN;
//                  Serial.println(JSON_ERROR_BAD_MQTT_PORT_LEN);
//               } else {               
//                  Serial.println(strlen(key_value[1]));
//                  //strcpy(g_settings.mqttPort, key_value[1]);
//                  g_settings.mqttPort = (unsigned short)strtol(key_value[1], NULL, 10);
//                  found_mqtt_port = true;
//               }
               Serial.print(":");
               Serial.println(key_value[1]);
            } else if (0==strcmp(MQTT_USER, key_value[0])){
               Serial.print("Found MQTT User:");
               Serial.print("MQTT User Len:");
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_MQTT_USER_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_USER_LEN;
                  Serial.println(JSON_ERROR_BAD_MQTT_USER_LEN);
               } else {               
                  Serial.println(strlen(key_value[1]));
                  strcpy(g_settings.mqttUser, key_value[1]);
                  found_mqtt_user = true;
               }
               Serial.print(":");
               Serial.println(key_value[1]);
            } else if (0==strcmp(MQTT_PASSWORD, key_value[0])){
               Serial.print("Found MQTT Password:");
               Serial.print("MQTT Password Len:");
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_MQTT_PASSWORD_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_PASSWORD_LEN;
                  Serial.println(JSON_ERROR_BAD_MQTT_PASSWORD_LEN);
               } else {               
                  Serial.println(strlen(key_value[1]));
                  strcpy(g_settings.mqttPassword, key_value[1]);
                  found_mqtt_password = true;
               }
               Serial.print(":");
               Serial.println(key_value[1]);
            } else if (0==strcmp(MQTT_TOPIC, key_value[0])){
               Serial.print("Found MQTT Port:");
               Serial.print("MQTT Port Len:");
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_MQTT_TOPIC_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_TOPIC_LEN;
                  Serial.println(JSON_ERROR_BAD_MQTT_TOPIC_LEN);
               } else {               
                  Serial.println(strlen(key_value[1]));
                  strcpy(g_settings.mqttTopic, key_value[1]);
                  found_mqtt_topic = true;
               }
               Serial.print(":");
               Serial.println(key_value[1]);
            }
         }
      }
   }

   

   bool success = false;
   if (found_ssid && found_password & found_server_ip & found_server_port & 
       found_mqtt_ip & found_mqtt_port & found_mqtt_user & found_mqtt_password &
       found_mqtt_topic){
      saveSettingsData();
      json[JSON_WIFI_RESET_TAG] = POST_RESP_SUCCESS;
      success = true;
   }   
     
   json[WIFI_SSID] = g_settings.ssid;
   json[WIFI_PASSWORD] = g_settings.wifiPassword;
   json[SERVER_IP] = g_settings.serverIPAddress;
   json[SERVER_PORT] = g_settings.serverPort;
   json[MQTT_IP] = g_settings.mqttIPAddress;
   json[MQTT_PORT] = g_settings.mqttPort;
   json[MQTT_USER] = g_settings.mqttUser;
   json[MQTT_PASSWORD] = g_settings.mqttPassword;
   json[MQTT_TOPIC] = g_settings.mqttTopic;

   // Prepare the response
   char response[JSON_RESPONSE_SIZE];
   sprintf(response, "%s %s\r\n" 
       "%s: %s\r\n\r\n"  
       "%s\r\n", HTTP_VERSION, HTTP_CODE, HTTP_HDR_CONTENT_TYPE_TAG, HTTP_HDR_CONTENT_TYPE_JSON,
       JSON.stringify(json).c_str());
  
   Serial.print("RESPONSE:");  
   Serial.println(response);  

   // Send the response to the client
   aClient.flush();
   aClient.print(response);

   if(success){
      delay(10);       
      Serial.print("About ro restart chip");  
      ESP.restart();
   }
}

/***************************************************************************
 * Resets the settings data to defaults. This is the only way to change
 * settings as the board is not normalling in server mode
 ***************************************************************************/
void reset_to_factory_settings(){

  strcpy(g_settings.ssid, FR_WIFI_SSID);
  strcpy(g_settings.wifiPassword, FR_WIFI_PASS);
  strcpy(g_settings.serverIPAddress, FR_SERVER_IP);
  g_settings.serverPort = FR_SERVER_PORT;
  strcpy(g_settings.mqttIPAddress, FR_MQTT_IP);
  g_settings.mqttPort = FR_MQTT_PORT;
  strcpy(g_settings.mqttUser, FR_MQTT_USER);
  strcpy(g_settings.mqttPassword, FR_MQTT_PASSWORD);
  strcpy(g_settings.mqttTopic, FR_MQTT_TOPIC);

   saveSettingsData();
}


float getVCC(){
  uint32_t vbattery = 0;

  for(int i = 0; i < 16; i++){
     vbattery = vbattery + analogReadMilliVolts(VCC_PIN); //ACV with correction     
  }
  float vbattery_f = 2 * vbattery / 16 / 1000.0; // Attenuation ratio 1/2 and mV to V
  Serial.print("Battery:");
  Serial.println(vbattery_f);

  return vbattery_f;
}


/*****************************************************************
 * 
 * This is the main loop. This runs once the setup is complete
 * 
 */
void loop() {
  if(CLIENT_MODE == g_mode){
     doClient();
  } else {
     doServer();    
  }
}


void doServer() {
//  Serial.print("\nLoopy");
  
 // Check if a client has connected
  WiFiClient client = server.available();

  if (!client) {
    delay(1);
    return;
  }
  client.setTimeout(WIFI_CLIENT_READ_TIMEOUT_MS);
  
  Serial.print("\nNew client connected from ");
  Serial.println(client.remoteIP());
  
  while(!client.available()){
    delay(1);
  }
  
  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.print("REQUEST:");
  Serial.println(req);

  int req_len = req.length();
  char req_char_buf[req_len+1];

  req.toCharArray(req_char_buf, req_len+1);
  int num_seps = getNumOccurrances(req_char_buf, HTTP_LINE_SEPARATOR);

  if (2 <= num_seps){
    char* components[num_seps+1];  
    split(req_char_buf, HTTP_LINE_SEPARATOR, components, num_seps);
  
    char* command = components[0];
    char* end_point = components[1];
    
    if(0==strcmp(REST_GET, command)){
      handleGet(client, end_point);
    } else if (0==strcmp(REST_PUT, command)){
      handlePut(client, end_point);
    } else if (0==strcmp(REST_POST, command)){
      handlePost(client, end_point);
    } else {
      handleUnsupported(client, command);    
    }
  }

  client.flush();
  delay(1);
  Serial.println("Client disconnected");

  // The client will actually be disconnected 
  // when the function returns and 'client' object is detroyed
}



void doClient() {

   Serial.println("Creating Client");

/******************************************** MQTT ******************************************************/
//   const char* mqtt_server = "192.168.0.124";
//   const uint16_t mqtt_server_port = 1883; 
//   const char* mqtt_server = mqttIPAddress;
//   const uint16_t mqtt_server_port = 1883; 
//   const char* mqtt_user = "alex";
//   const char* mqtt_password = "Pl@5m0d!um";
//   const char *topic = "esp8266/db5";

   float vcc = getVCC();


//  char mqttIPAddress[IP_ADDR_LENGTH];
//  char mqttPort[PORT_STR_LENGTH];
//  char mqttUser[MAX_MQTT_USER_LENGTH];
//  char mqttPassword[MAX_MQTT_PASSWORD_LENGTH];
//  char mqttTopic[MAX_MQTT_TOPIC_LENGTH];


/***********************************************************************************************************/

   WiFiClient wifi_client;  // or WiFiClientSecure for HTTPS
   HTTPClient http;
   

/*******************************************************************************************************/

   // Send request
   const char *server_url = "http://192.168.0.124:3000/db5/status";

//   http.useHTTP10(true);
   Serial.printf("Sending request to %s\n", server_url);
   http.begin(wifi_client, server_url);
   Serial.print("Calling GET:");
   int resp_code = http.GET();
   Serial.println(resp_code);


   if(resp_code > 0) {
      // Print the response

      String http_response = http.getString();
      Serial.print("Response:");
      Serial.println(http_response);

      if (http_response.length() > 3) { // remove UTF8 BOM
         if (http_response[0] == char(0xEF) && http_response[1] == char(0xBB) && http_response[2] == char(0xBF)) {
            Serial.println("remove BOM from JSON");
            http_response = http_response.substring(3);
         }
      }
      

      JSONVar resp_json = JSON.parse(http_response);
      Serial.println(JSON.stringify(http_response).c_str());


      Serial.print("JSON.typeof:");
      Serial.println(JSON.typeof(resp_json));


      if (JSON.typeof(resp_json) == "undefined") {
         Serial.println("Parsing input failed!");
      } else {

         Serial.print("Lights Status:");
         Serial.println(resp_json[JSON_LIGHTS_TAG]);
         Serial.print("Reset Status:");
         Serial.println(resp_json[JSON_RESET_TAG]);

         if(null != resp_json[JSON_LIGHTS_TAG]){
            if (0 == strcmp(resp_json[JSON_LIGHTS_TAG], LIGHTS_STATUS_ON)){
               Serial.println("Server says lights are on");
               setLights(true); 
            } else if (0 == strcmp(resp_json[JSON_LIGHTS_TAG], LIGHTS_STATUS_OFF)){
               Serial.println("Server says lights are off");
               setLights(false);
            } 
         }

         if(null != resp_json[JSON_LIGHTS_TAG]){
            if(0 == strcmp(resp_json[JSON_RESET_TAG], RESET_STATUS_TRUE)){
               Serial.println("Reset recevied, clearing memory");
               reset_to_factory_settings();
               Serial.print("About to restart chip");  
               ESP.restart();
            }
         }

      }
    
   }else {
      Serial.println("Error Connecting");    
   }
   http.end();

   /**
    * Sending MQTT 
    */
   PubSubClient mqtt_client(wifi_client);
   Serial.print("Connecting to MQTT on ");
   Serial.print(g_settings.mqttIPAddress);
   Serial.print(" on port ");
   Serial.println(g_settings.mqttPort);
   
   mqtt_client.setServer(g_settings.mqttIPAddress, g_settings.mqttPort);

   while (!mqtt_client.connected()) {
      String client_id = "esp32c3-client-";
      client_id += String(WiFi.macAddress());
      Serial.printf("The client %s connects to the public mqtt broker with %s/%s \n", client_id.c_str(), g_settings.mqttUser, g_settings.mqttPassword);
      if (mqtt_client.connect(client_id.c_str(), g_settings.mqttUser, g_settings.mqttPassword)) {
      } else {
         Serial.print("failed with state ");
         Serial.println(mqtt_client.state());
         delay(2000); //To prevent looping
         break;
      }
   }

   JSONVar json_packet;

   if(g_pin_state){
      json_packet[JSON_LIGHTS_TAG] = LIGHTS_STATUS_ON;
   } else {
      json_packet[JSON_LIGHTS_TAG] = LIGHTS_STATUS_OFF;
   }
   json_packet[JSON_VCC_TAG] = vcc;


   Serial.println(JSON.stringify(json_packet).c_str());
   
   mqtt_client.publish(g_settings.mqttTopic, JSON.stringify(json_packet).c_str());
   /**************************************************************************************/


   int sleep_len_ms = 10000;
   delay(sleep_len_ms);



////   int loop_delay = 60000;
////   int loop_delay_ms = 60000;
//   int loop_delay_ms = 10000;
//   int led_duration_ms = 500;
//   digitalWrite(BUILTIN_LED_PIN, 0);
//   delay(led_duration_ms);
//   digitalWrite(BUILTIN_LED_PIN, 1);
//
////   Serial.print("Sleeping for ");
////   Serial.print((loop_delay - led_duration));
////   Serial.println("ms");
////   delay(loop_delay - led_duration);
//
//
//   gpio_hold_en((gpio_num_t)BUILTIN_RX_PIN);
//
//
////   long sp_duration = 10e6;
//   long sp_duration_us = loop_delay_ms * 1000;
//   Serial.print("Deep Sleep for ");
//   Serial.print((sp_duration_us - (led_duration_ms * 1000)));
//   Serial.println("us");
//   ESP.deepSleep((sp_duration_us - (led_duration_ms * 1000))); // 20e6 is 20 microseconds

}
