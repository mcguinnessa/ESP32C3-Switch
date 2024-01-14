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

//EEPROM Details for non-volatile config
const int EEPROM_SIZE          = 512;
const int WIFI_DATA_START_ADDR = 0;
const int PIN_DATA_START_ADDR  =150;
const int MAX_WIFI_SSID_LENGTH = 32;
const int MAX_WIFI_PASSWORD_LENGTH = 63;

const int WIFI_CLIENT_READ_TIMEOUT_MS = 5000;

const int SERVER_PORT = 80;
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
const char* const WIFI_PASSWORD     ="password";

//JSON Defs
const char* const LIGHTS_STATUS_UNKNOWN = "unknown";
const char* const LIGHTS_STATUS_ON = "on";
const char* const LIGHTS_STATUS_OFF = "off";
const char* const JSON_LIGHTS_TAG = "lights";
const char* const JSON_VCC_TAG = "vcc";
const char* const JSON_WIFI_RESET_TAG = "wifi-reset";
const char* const JSON_ERROR_TAG = "error";
const char* const POST_RESP_FAILURE = "Failure";
const char* const POST_RESP_SUCCESS = "Success";

//HTTP
const int WIFI_WEB_PAGE_SIZE = 1536;
const int JSON_RESPONSE_SIZE = 256;
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

const char* const SET_WIFI_URL = "/admin/wifi";
const char* const SET_WIFI_DETAILS_COMMAND = "/set-wifi-details";

//const byte BUILTIN_LED_PIN=2;
//const byte BUILTIN_TX_PIN=1;
//const byte BUILTIN_RX_PIN=3;
 
/**
 * Define AP WIFI Details
 */
const char* const AP_SSID = "ESP32C3";
const char* const AP_PASS = "bruckfexit";
const int AP_MAX_CON = 1;

const IPAddress AP_LOCAL_IP(192,168,1,1);
const IPAddress AP_GATEWAY(192,168,1,9);
const IPAddress AP_SUBNET(255,255,255,0);

 
 
// define led according to pin diagram
int led1 = D10;
int led2 = D1;



//Definition of structure to be stored in memory
struct storeStruct_t{
  char myVersion[3];
  char ssid[MAX_WIFI_SSID_LENGTH];
  char password[MAX_WIFI_PASSWORD_LENGTH];
};


/**
 * Define Global Variables
 */
// Create an instance of the server
WiFiServer server(SERVER_PORT);

bool g_pin_state = false;

storeStruct_t g_settings = {
  "01",
  "SSID"
  "Password"
};



void setup() {
  Serial.begin(115200);
  // initialize digital pin led as an output
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  while(!Serial);
  Serial.println("Starting...");


  loadWifiData();
  loadPinState();  
  //setLights(g_pin_state);

  //Connect to configured network
  if(!connectToWiFiNetwork()){
    setupAccessPoint();    
  }
  server.begin();
  Serial.println("Server started");
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
  Serial.print(led1);
  Serial.print(":");
  Serial.println(p1state);
  Serial.print("PIN:");
  Serial.print(led2);
  Serial.print(":");
  Serial.println(p2state);

  digitalWrite(led1, p1state);
  digitalWrite(led2, p2state);
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
  Serial.print(g_settings.password);
  Serial.print(" for ");
  Serial.print(CONNECT_TIMEOUT_MS);
  Serial.print("ms (");
  Serial.print(attempts);
  Serial.println(") attempts");
  WiFi.begin(g_settings.ssid, g_settings.password);

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
     Serial.println(g_settings.password);
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
void loadWifiData(){

  Serial.println("WiFi Config from EEPROM");
  storeStruct_t load;
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get( WIFI_DATA_START_ADDR, load);
  EEPROM.end();

  Serial.println("-----------------------");
  Serial.print("SSID:");
  Serial.println(load.ssid);
  Serial.print("Password:");
  Serial.println(load.password);
  Serial.println("-----------------------");
  Serial.print("size:");
  Serial.println(sizeof(load));
  Serial.println("-----------------------");

  g_settings = load;  
}


/**
 * Stores the WiFi config in EEPROM
 */
void saveWifiData() {
  
  Serial.println("Saving WiFi Config to EEPROM.");
  Serial.print("   G_SETTINGS_SSID:");
  Serial.println(g_settings.ssid);
  Serial.print("   G_SETTINGS_PASSWORD:");
  Serial.println(g_settings.password);

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
      //memset(hdr_char_buf, '\0', hdr_line_len+1);
//      Serial.print("hdr_char_buf:");
//      Serial.print(hdr_char_buf);

//      hdr_line.toCharArray(hdr_char_buf, hdr_line_len+1) //TODO check this
      hdr_line.toCharArray(hdr_char_buf, hdr_line_len+1);
      
      split(hdr_char_buf, ":", hdr_line_elements, 1);

      if(NULL != strstr(hdr_line_elements[0], HTTP_HDR_CONTENT_LEN_TAG)){
         Serial.print("Found length:");
         Serial.println(hdr_line_elements[1]);      
         aBodySize = atoi(hdr_line_elements[1]);
      }        

      if (hdr_line=="\r"){
//         ++newline;
//         Serial.println("newline found");
//         if(2==newline){
           Serial.println("END OF HEADER");
//         Serial.println(hdr_line);      
           header_found = true;
//         }
       }    
   }

//   free(hdr_char_buf);
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
  if(0==strcmp(SET_WIFI_DETAILS_COMMAND, aEndPoint)){
     handleSetWifiSettings(aClient);
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
    showWifiChangeForm(aClient);     
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
void showWifiChangeForm(WiFiClient& aClient){

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
         "               <label for=\"password\">Password:</label>"
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
         SET_WIFI_DETAILS_COMMAND, WIFI_SSID, WIFI_SSID, WIFI_PASSWORD, WIFI_PASSWORD);

  Serial.print("Size of web page:");
  Serial.println(strlen(web_page));

  aClient.flush();
  aClient.print(web_page);

  Serial.print("Show Complete\n");

}

/**
 * This handles the POST command that sets the WiFi Credentials
 */
void handleSetWifiSettings(WiFiClient& aClient){

   int body_size_from_hdr = 0;
   bool found_ssid = false;
   bool found_password = false;
   
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
                  strcpy(g_settings.password, key_value[1]);
                  found_password = true;
               }
               Serial.print(":");
               Serial.println(key_value[1]);
            }
         }
      }
   }

   bool success = false;
   if (found_ssid && found_password ){
      saveWifiData();
      json[JSON_WIFI_RESET_TAG] = POST_RESP_SUCCESS;
      success = true;
   }   
     
   json[WIFI_SSID] = g_settings.ssid;
   json[WIFI_PASSWORD] = g_settings.password;

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


//void loop() {
//
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
//
//}

/************************************************************************
 * 
 * This is the main loop. This runs once the setup is complete
 * 
 ***********************************************************************/
void loop() {
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
