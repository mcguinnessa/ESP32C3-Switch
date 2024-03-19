/*
 * Alex McGuinness 
 * 08/01/2024
 * 
 * This sketch will interrogate a server using the given URL for the status of the light and the time to sleep.
 * It will then set the light state and sleep for the given time.
 * 
 * It will also send an MQTT message including the light state and the battery voltage.
 *  
 * On first use the ESP will fail to connect to the wifi and go into server mode, where it will present a 
 * configuration page on where the wifi, server and mqtt parameters can be set.
 *  
 * A reset command can be sent to the server which will force the ESP into factory reset mode when it next checks
 */

#include <EEPROM.h> //The EEPROM libray 
#include "WiFi.h"
#include <Arduino_JSON.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

#include "arduino_percent.hpp"

/*****************************************************************************
* If due to corruption or development, the reset can't be send via the server.
* This will force a reset if set to true.
* 
* Once sent to chip, unset and resend
******************************************************************************/
bool FORCE_RESET = false;
/*****************************************************************************/

/*****************************************************************************
* These are expected to be configurable.
* 
******************************************************************************/
const String HW_MANUFACTURER = "xiao seeed";
const String HW_MODEL        = "esp32c3";
const String HW_VERSION      = HW_MODEL;
const String SW_VERSION      = "adm-esp32pull-1.25";

// define PINs according to pin diagram
const int ON_PIN = D10;
const int OFF_PIN = D1;
const int VCC_PIN = A0;

const uint16_t MIN_SERVER_PORT         = 80;
const uint16_t MIN_MQTT_PORT           = 1023;

//Server Connection
const int WIFI_CLIENT_READ_TIMEOUT_MS = 5000;
const int SERVER_READ_TIMEOUT_MS = 500; 

//MQTT Connection
const uint16_t MQTT_CONNECT_TIMEOUT_S = 5;
const uint16_t MQTT_CONNECT_DELAY_MS = 100;

/*****************************************************************************/

//EEPROM Details for non-volatile config
const int EEPROM_SIZE                = 512;
const int STATE_DATA_START_ADDR      = 0;
const int CONFIG_DATA_START_ADDR     = 32;
const int VERSION_LENGTH             = 4;
const int MAX_WIFI_SSID_LENGTH       = 32;
const int MAX_WIFI_PASSWORD_LENGTH   = 63;
const int HOST_IP_LENGTH             = 32;
const int MAX_SERVER_URL_PREFIX_LENGTH   = 8;
const int MAX_MQTT_USER_LENGTH       = 12; 
const int MAX_MQTT_PASSWORD_LENGTH   = 32;
//const int MAX_MQTT_TOPIC_LENGTH    = 32;
const int MAX_INSTANCE_ID_LENGTH     = 32;
const int MAX_MQTT_DISPLAY_NAME_LENGTH   = 32;
const int MAX_MQTT_DISC_PREFIX_LENGTH    = 32;

const int SEND_MQTT_DISCOVERY_EVERY_ITERATIONS = 10;


const int AP_SERVER_PORT = 80;
const int CONNECT_TIMEOUT_MS  = 30000;  //How long it should take to connect
const int CONNECT_INTERVAL_MS = 500;    //How long it should wait inbetween attempts

////REST API Defs
const char* const REST_PUT          ="PUT";
const char* const REST_POST         ="POST";
const char* const REST_GET          ="GET";
const char* const ADMIN_COMMAND     ="admin";
const char* const LIGHTS_STATUS     ="status";
const char* const SETTINGS          ="settings";

/* Config Parameters */
const char* const WIFI_SSID             ="ssid";
const char* const WIFI_PASSWORD         ="wifi-password";
const char* const SERVER_IP             = "server-ip"; 
const char* const SERVER_PORT           = "server-port"; 
const char* const SERVER_URL_PREFIX     = "server-url-prefix"; 
const char* const MQTT_IP               = "mqtt-ip";
const char* const MQTT_PORT             = "mqtt-port"; 
const char* const MQTT_USER             = "mqtt-user";
const char* const MQTT_PASSWORD         = "mqtt-password"; 
const char* const INSTANCE_ID           = "instance-id"; 
const char* const MQTT_DISPLAY_NAME     = "mqtt-display-name";           
const char* const MQTT_DISCOVERY_PREFIX = "mqtt-disc-prefix";      

//JSON Defs
const char* const LIGHTS_SERVER_STATUS_ON = "on";
const char* const LIGHTS_SERVER_STATUS_OFF = "off";
const char* const LIGHTS_MQTT_STATUS_ON = "on";
const char* const LIGHTS_MQTT_STATUS_OFF = "off";
//const char* const CONN_MQTT_STATUS_Y = "y";
//const char* const CONN_MQTT_STATUS_N = "n";
const char* const JSON_RESET_TAG = "reset";
const char* const RESET_STATUS_TRUE = "true";
const char* const JSON_DISCOVERY_TAG = "discovery";
const char* const DISCOVERY_STATUS_TRUE = "true";
const char* const JSON_SEQID_TAG = "seqid";
const char* const MODE_TAG = "mode";
const char* const MODE_DEEP   = "deep";
const char* const MODE_PROG   = "prog";
const char* const MODE_SERVER = "server";
const String MQTT_JSON_LIGHTS_TAG = "lights";
const String MQTT_JSON_TTS_TAG = "tts";
const String MQTT_JSON_VCC_TAG = "vcc";
const String MQTT_JSON_LSC_TAG = "lsc";
const String MQTT_JSON_MODE_TAG = "md";
const char* const JSON_CONFIG_RESET_TAG = "reset-config";
const char* const JSON_ERROR_TAG = "error";
const char* const POST_RESP_FAILURE = "Failure";
const char* const POST_RESP_SUCCESS = "Success";
const int MAX_TTS_SLEEP_PERIOD_MS = 60 * 60 * 1000;
const int MIN_TTS_SLEEP_PERIOD_MS = 60 * 1000;


const String MQTT_JSON_DEV_NAME_TAG = "name";
const String MQTT_JSON_DEV_IDS_TAG = "ids";
const String MQTT_JSON_DEV_HW_TAG = "hw";
const String MQTT_JSON_DEV_SW_TAG = "sw";
const String MQTT_JSON_DEV_MANUFACTURER_TAG = "manufacturer";
const String MQTT_JSON_DEV_MODEL_TAG = "model";
const String MQTT_JSON_NAME_TAG = "name";
const String MQTT_JSON_STAT_TOPIC_TAG = "stat_t";
const String MQTT_JSON_UNIQ_ID_TAG = "uniq_id";
const String MQTT_JSON_DEVICE_CLASS_TAG = "dev_cla";
const String MQTT_JSON_DEV_TAG = "dev";
const String MQTT_JSON_ICON_TAG = "ic";
const String MQTT_JSON_MEAS_UNIT_TAG = "unit_of_meas";
const String MQTT_JSON_VAL_TEMPLATE_TAG = "val_tpl";
const String MQTT_JSON_FORCE_UPDATE_TAG = "frc_upd";
const String MQTT_JSON_PL_ON_TAG = "pl_on";
const String MQTT_JSON_PL_OFF_TAG = "pl_off";


//HTTP
const int WIFI_WEB_PAGE_SIZE = 3778;
const int JSON_RESPONSE_SIZE = 384;
const int MAX_SERVER_URL_LEN = 48;
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
const char* const JSON_ERROR_BAD_SSID_LEN                  = "Invalid SSID Length";
const char* const JSON_ERROR_BAD_PWD_LEN                   = "Invalid password Length";
const char* const JSON_ERROR_BAD_SERVER_IP_LEN             = "Invalid Server IP length";
const char* const JSON_ERROR_BAD_SERVER_PORT_LEN           = "Invalid Server Port length";
const char* const JSON_ERROR_BAD_SERVER_URL_PREFIX_LEN     = "Invalid Server URL Prefix length";
const char* const JSON_ERROR_BAD_MQTT_IP_LEN               = "Invalid MQTT IP length";
const char* const JSON_ERROR_BAD_MQTT_PORT_LEN             = "Invalid MQTT Port length";
const char* const JSON_ERROR_BAD_MQTT_USER_LEN             = "Invalid MQTT User length";
const char* const JSON_ERROR_BAD_MQTT_PASSWORD_LEN         = "Invalid MQTT Password length";
const char* const JSON_ERROR_BAD_INSTANCE_ID_LEN           = "Invalid Instance ID length";
const char* const JSON_ERROR_BAD_MQTT_DISCOVERY_PREFIX_LEN = "Invalid Discovery Prefix length";
const char* const JSON_ERROR_BAD_MQTT_DISPLAY_NAME_LEN     = "Invalid Display Name length";
const uint16_t MAX_PORT                = 65535;

const char* const SET_CONFIG_URL = "/admin/settings";
const char* const SET_CONFIG_COMMAND = "/settings";


/**
 * Set default values for factory reset
 */
const char* const FR_CFG_VERSION        = "007";
const char* const FR_WIFI_SSID          = "wifi-ssid";
const char* const FR_WIFI_PASS          = "wifipassword";
const char* const FR_SERVER_IP          = "0.0.0.0";
const unsigned int FR_SERVER_PORT       = (short)0;
const char* const FR_SERVER_URL_PREFIX  = "prefix";
const char* const FR_MQTT_IP            = "0.0.0.0";
const unsigned int FR_MQTT_PORT         = (short)0;
const char* const FR_MQTT_USER          = "mqtt";
const char* const FR_MQTT_PASSWORD      = "mqttpass";
const char* const FR_INSTANCE_ID        = "id-esp32c3"; 
const char* const FR_DISCOVERY_PREFIX   = "homeassistant"; 
const char* const FR_DISPLAY_NAME       = "Display Name"; 


const char* const DISCOVERY_TOPIC_SUFFIX= "/config";
const char* const MQTT_TOPIC_SUFFIX = "/state";
const String DISCOVERY_TOPIC_TTS = "tts"; 
const String DISCOVERY_TOPIC_VCC = "vcc"; 
const String DISCOVERY_TOPIC_LIGHTS = "lights"; 
const String DISCOVERY_TOPIC_LSC = "lsc";
const String DISCOVERY_TOPIC_MODE = "mode";

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
 

//These are for the wait for serial
const int TIME_TO_WAIT_FOR_SERIAL_MS = 2000;
const int DELAY_WAIT_FOR_SERIAL_MS = 100;

//Sleep period


//Definition of structure to be stored in memory
struct storeStruct_t{
  char myVersion[VERSION_LENGTH];
//  byte isBooleanStates;
  char ssid[MAX_WIFI_SSID_LENGTH];
  char wifiPassword[MAX_WIFI_PASSWORD_LENGTH];
  char instanceId[MAX_INSTANCE_ID_LENGTH];
  char serverIPAddress[HOST_IP_LENGTH];
  uint16_t serverPort;
  char serverUrlPrefix[MAX_SERVER_URL_PREFIX_LENGTH];
  char mqttIPAddress[HOST_IP_LENGTH];
  uint16_t mqttPort;
  char mqttUser[MAX_MQTT_USER_LENGTH];
  char mqttPassword[MAX_MQTT_PASSWORD_LENGTH];
  char mqttDiscoveryPrefix[MAX_MQTT_DISC_PREFIX_LENGTH];
  char mqttDisplayName[MAX_MQTT_DISPLAY_NAME_LENGTH];

//  char mqttTopic[MAX_MQTT_TOPIC_LENGTH];
};

// Create an instance of the server
WiFiServer server(AP_SERVER_PORT);

//const int MAX_FAILURES_ALLOWED_BEFORE_RESET = 10;

//bool g_has_been_successful_at_least_once = false;
//int g_failures_count = 0;

const short RESET_FLAG_BIT     = 0;
const short LIGHT_FLAG_BIT     = 1;
const short DISCOVERY_FLAG_BIT = 2;
const short DEEPSLEEP_FLAG_BIT = 3;
const byte RESET_STATE_MASK       = 0b00000000;
const byte RESET_FLAG_BITMASK     = 0b00000001;
const byte LIGHT_FLAG_BITMASK     = 0b00000010;
const byte DISCOVERY_FLAG_BITMASK = 0b00000100;
const byte DEEPSLEEP_FLAG_BITMASK = 0b00001000;

byte g_state = RESET_STATE_MASK;

//bool g_light_state = false;
short g_mode = SERVER_MODE;

String g_uniq_id = "";
String g_mqtt_state_topic = "";

storeStruct_t g_settings = {
  *FR_CFG_VERSION,
//  0b00000000,
  *FR_WIFI_SSID,
  *FR_WIFI_PASS,
  *FR_INSTANCE_ID,
  *FR_SERVER_IP,
  FR_SERVER_PORT,
  *FR_SERVER_URL_PREFIX,
  *FR_MQTT_IP,
  FR_MQTT_PORT,
  *FR_MQTT_USER,
  *FR_MQTT_PASSWORD,
  *FR_DISCOVERY_PREFIX,
  *FR_DISPLAY_NAME
};


#ifdef ESP8266
int CHIP_ID = ESP.getChipId();
#elif defined(ESP32)
int CHIP_ID = ESP.getEfuseMac();
#endif

/**
 * Normal set up method
 */
void setup() {
  Serial.begin(115200);
  // initialize digital pin led as an output
  pinMode(ON_PIN, OUTPUT);
  pinMode(OFF_PIN, OUTPUT);

  pinMode(VCC_PIN, INPUT);  


  //If I wait forever, it will never start on battery, but I want intial messages on startup when debugging
  //So this waits for a small time for serial to start, then gives up
  int serial_attempts = TIME_TO_WAIT_FOR_SERIAL_MS / DELAY_WAIT_FOR_SERIAL_MS;
  while((!Serial) && (0 < serial_attempts)){
    delay(DELAY_WAIT_FOR_SERIAL_MS);
    --serial_attempts;
  }
  Serial.println("Serial Started...");

  Serial.print("Chip ID:");
  Serial.println(CHIP_ID);

  loadSettingsDataFromEEPROM();
  loadStateFromEEPROM();  

  Serial.print("EEPROM Version:");
  Serial.print(g_settings.myVersion);
  Serial.print(" SWVersion:");
  Serial.println(FR_CFG_VERSION);
  if (0 != strcmp(g_settings.myVersion, FR_CFG_VERSION)){
    Serial.println("Version Mismatch!");
    setReset(true);
  }
  

  //If can't connect to configured network or
  // reset flag is set, go into server mode
  if((isReset()) || (!connectToWiFiNetwork())){
    setupAccessPoint();    
    server.begin();
    g_mode = SERVER_MODE;
    Serial.println("Started in Server Mode");
  } else {
    Serial.println("Started in Client Mode");    
    g_mode = CLIENT_MODE;

   g_mqtt_state_topic = HW_VERSION +"/" + String(g_settings.instanceId) + MQTT_TOPIC_SUFFIX;

    
  }
}

/**
 * Sets the PIN based on the state
 */
void setLights(bool aState){
//   g_light_state = aState;
   setLightsState(aState);

   int p1state = LOW;
   int p2state = HIGH;

   if(aState){
      p1state = HIGH;
      p2state = LOW;
   }
     
  Serial.print("PIN:");
  Serial.print(ON_PIN, HEX);
  Serial.print(":");
  Serial.println(p1state);
  Serial.print("PIN:");
  Serial.print(OFF_PIN, HEX);
  Serial.print(":");
  Serial.println(p2state);

  digitalWrite(ON_PIN, p1state);
  digitalWrite(OFF_PIN, p2state);

  saveStateToEEPROM();     
}

/**
 * Converts the MQTT connection into help text state
 *         
 * -4 : MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time
 * -3 : MQTT_CONNECTION_LOST - the network connection was broken
 * -2 : MQTT_CONNECT_FAILED - the network connection failed
 * -1 : MQTT_DISCONNECTED - the client is disconnected cleanly
 *  0 : MQTT_CONNECTED - the client is connected 
 *  1 : MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT
 *  2 : MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier
 *  3 : MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection
 *  4 : MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected
 *  5 : MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect
 */
String get_readable_mqtt_connection_status(short aCode){
  String rc = "Unknown Error Code:" + aCode;
  switch (aCode){
    case MQTT_CONNECTION_TIMEOUT:
       rc = "MQTT_CONNECTION_TIMEOUT"; break;
    case MQTT_CONNECTION_LOST:
       rc = "MQTT_CONNECTION_LOST"; break;       
    case MQTT_CONNECT_FAILED:
       rc = "MQTT_CONNECT_FAILED"; break;       
    case MQTT_DISCONNECTED:
       rc = "MQTT_DISCONNECTED"; break;       
    case MQTT_CONNECTED:
       rc = "MQTT_CONNECTED"; break;       
    case MQTT_CONNECT_BAD_PROTOCOL:
       rc = "MQTT_CONNECT_BAD_PROTOCOL"; break;       
    case MQTT_CONNECT_BAD_CLIENT_ID:
       rc = "MQTT_CONNECT_BAD_CLIENT_ID"; break;       
    case MQTT_CONNECT_UNAVAILABLE:
       rc = "MQTT_CONNECT_UNAVAILABLE"; break;       
    case MQTT_CONNECT_BAD_CREDENTIALS:
       rc = "MQTT_CONNECT_BAD_CREDENTIALS"; break;       
    case MQTT_CONNECT_UNAUTHORIZED:
       rc = "MQTT_CONNECT_UNAUTHORIZED"; break;       
  }
  return rc;
}

/**
 * Converts the WiFi status code into help text state
 */
String get_readable_connection_status(wl_status_t aCode){
  String rc = "Unknown Error Code:" + aCode;
  switch (aCode){
    case WL_IDLE_STATUS:
       rc = "WL_IDLE_STATUS"; break;
    case WL_NO_SHIELD:
       rc = "WL_NO_SHIELD"; break;       
    case WL_NO_SSID_AVAIL:
       rc = "WL_NO_SSID_AVAIL"; break;       
    case WL_SCAN_COMPLETED:
       rc = "WL_SCAN_COMPLETED"; break;       
    case WL_CONNECTED:
       rc = "WL_CONNECTED"; break;       
    case WL_CONNECT_FAILED:
       rc = "WL_CONNECT_FAILED"; break;       
    case WL_CONNECTION_LOST:
       rc = "WL_CONNECTION_LOST"; break;       
    case WL_DISCONNECTED:
       rc = "WL_DISCONNECTED"; break;       
  }
  return rc;
}


/**
 * Connect to wifi network
 */
int connectToWiFiNetwork(){

  bool rc = false;

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
  wl_status_t old_status = WL_DISCONNECTED;
  Serial.print(get_readable_connection_status(old_status));
  while ((WiFi.status() != WL_CONNECTED) && (attempts > 0)) {
    delay(CONNECT_INTERVAL_MS);

    wl_status_t status_code = WiFi.status();
    if(old_status == status_code){
       Serial.print(".");  
    } else {
       Serial.println("");  
       Serial.print(get_readable_connection_status(status_code));      
    }
    old_status = status_code;
    
    --attempts;
  }
  Serial.println("");  

  if(WiFi.status() == WL_CONNECTED) {
     Serial.print("\nWiFi connected with IP ");
     Serial.println(WiFi.localIP());      
     rc = true;

   String mac_addr = String(WiFi.macAddress());   
   mac_addr.replace(":","");
   g_uniq_id = HW_VERSION +"-" + String(g_settings.instanceId) + "-" + mac_addr;
   Serial.print("Unique ID:");   
   Serial.println(g_uniq_id);
     
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
 * Sets up the wireless access point, so the ESP can be accessed with no wifi details
 */
void setupAccessPoint(){

  WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY, AP_SUBNET);

  Serial.println("#############################################################################\n");
  Serial.print("Starting as Access Point: Connect to ");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS, /*channel no*/ NULL, /*hidden ssid*/ NULL, AP_MAX_CON);

  Serial.print(AP_SSID);
  Serial.print(" using ");
  Serial.println(AP_PASS);
  Serial.println("Then access the URL:");
  Serial.print("http://");
  Serial.print(WiFi.softAPIP());
  Serial.print(SET_CONFIG_URL);
  Serial.println("\nAnd configure WiFi and other settings.");
  Serial.println("\n#############################################################################");

  uint8_t macAddr[6];
  WiFi.softAPmacAddress(macAddr);
  Serial.printf("MAC address = %02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

/**
 * Destroys the access point
 */
void removeAccessPoint(){

  Serial.print("Removing Access Point.");  
  WiFi.softAPdisconnect (true);
  WiFi.mode(WIFI_AP);  
}

/**
 * Checks if the Factory Reset flag has been set
 */
bool isReset(){
  
  Serial.print("isReset() - ");
  Serial.println(g_state & RESET_FLAG_BITMASK, BIN);
  return g_state & RESET_FLAG_BITMASK;
}

/**
 * Checks if the Factory Reset flag has been set
 */
bool isDeepSleepMode(){
  
  Serial.print("isDeepSleepMode() - ");
//  Serial.print(g_state, BIN);
//  Serial.pr+int(" | ");
//  Serial.println(DEEPSLEEP_FLAG_BITMASK, BIN);
  Serial.println(g_state & DEEPSLEEP_FLAG_BITMASK, BIN);
//  Serial.println(g_state, BIN);
  return g_state & DEEPSLEEP_FLAG_BITMASK;
}


/**
 * Checks if the MQTT Discovery flag has been set
 */
bool isMQTTDiscoverySent(){
  Serial.print("isMQTTDiscoverySent() - ");
//  Serial.println(g_state, BIN);
  Serial.println(g_state & DISCOVERY_FLAG_BITMASK, BIN);
  return g_state & DISCOVERY_FLAG_BITMASK;
}

/**
 * Checks if the MQTT Discovery flag has been set
 */
bool lightState(){
  Serial.print("lightState() - ");
  Serial.println(g_state & LIGHT_FLAG_BITMASK, BIN);
  return g_state & LIGHT_FLAG_BITMASK;
}

/**
 * Sets the Factory Reset flag
 */
void setReset(boolean aReset){
  if(aReset){
     bitSet(g_state, RESET_FLAG_BIT);
  } else {
     bitClear(g_state, RESET_FLAG_BIT);    
  }     
  Serial.print("setReset() - ");
  Serial.println(g_state & RESET_FLAG_BITMASK, BIN);
}

/**
 * Sets the MQTT Discovery flag
 */
void setMQTTDiscoverySent(boolean aSent){
  if(aSent){
     bitSet(g_state, DISCOVERY_FLAG_BIT);
  } else {
     bitClear(g_state, DISCOVERY_FLAG_BIT);    
  }
  Serial.print("setMQTTDiscoverySent() - ");
  Serial.println(g_state & DISCOVERY_FLAG_BITMASK, BIN);
}

/**
 * Sets the MQTT Discovery flag
 */
void setDeepSleepMode(boolean aDeepSleep){
  if(aDeepSleep){
     bitSet(g_state, DEEPSLEEP_FLAG_BIT);
  } else {
     bitClear(g_state, DEEPSLEEP_FLAG_BIT);    
  }
  Serial.print("setDeepSleepMode() - g_state:");
  Serial.println(g_state & DEEPSLEEP_FLAG_BITMASK, BIN);
}

/**
 * Sets the Lights flag
 */
void setLightsState(boolean aSent){
  if(aSent){
     bitSet(g_state, LIGHT_FLAG_BIT);
  } else {
     bitClear(g_state, LIGHT_FLAG_BIT);    
  }
  Serial.print("setLightsFlag() - ");
  Serial.println(g_state & LIGHT_FLAG_BITMASK, BIN);
//  Serial.println(g_state, BIN);
}

/**
 * Sets the Lights flag
 */
void zeroAllStates(){

  g_state = RESET_STATE_MASK;
  Serial.print("zeroAllStates() - g_state:");
  Serial.println(g_state, BIN);
}
 
/**
 * Reads in from EEPROM Memory the WIFI settings
 */
void loadSettingsDataFromEEPROM(){

  Serial.println("-----------------------");
  Serial.println("WiFi Config from EEPROM");
  Serial.println("-----------------------");
  storeStruct_t load;
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get( CONFIG_DATA_START_ADDR, load);
  EEPROM.end();

  Serial.print("Version:");
  Serial.println(load.myVersion);
  Serial.print("SSID:");
  Serial.println(load.ssid);
  Serial.print("Password:");
  Serial.println(load.wifiPassword);
  Serial.print("Instance ID:");
  Serial.println(load.instanceId);
  Serial.print("Server IP:");
  Serial.println(load.serverIPAddress);
  Serial.print("ServerPort:");
  Serial.println((uint16_t)load.serverPort);
  Serial.print("ServerUrlPrefix:");
  Serial.println(load.serverUrlPrefix);
  Serial.print("MQTT IP:");
  Serial.println(load.mqttIPAddress);
  Serial.print("MQTT Port:");
  Serial.println((uint16_t)load.mqttPort);
  Serial.print("MQTT User:");
  Serial.println(load.mqttUser);
  Serial.print("MQTT Password:");
  Serial.println(load.mqttPassword);
  Serial.print("MQTT Display Name:");
  Serial.println(load.mqttDisplayName);
  Serial.print("MQTT Discovery Prefix:");
  Serial.println(load.mqttDiscoveryPrefix);

  Serial.println("-----------------------");
  Serial.print("size:");
  Serial.println(sizeof(load));
  Serial.println("-----------------------");

  g_settings = load;  
}


/**
 * Stores the WiFi config in EEPROM
 */
void saveSettingsDataToEEPROM() {

  strcpy(g_settings.myVersion, FR_CFG_VERSION);

  Serial.println("-----------------------");
  Serial.println("Saving WiFi Config to EEPROM.");
  Serial.println("-----------------------");
  Serial.print("   Version:");
  Serial.println(g_settings.myVersion);
  Serial.print("   SSID:");
  Serial.println(g_settings.ssid);
  Serial.print("   PASSWORD:");
  Serial.println(g_settings.wifiPassword);
  Serial.print("   INSTANCE_ID:");
  Serial.println(g_settings.instanceId);
  Serial.print("   SERVER_IP:");
  Serial.println(g_settings.serverIPAddress);
  Serial.print("   SERVER_PORT:");
  Serial.println((uint16_t)g_settings.serverPort);
  Serial.print("   SERVER_URL_PREFIX:");
  Serial.println(g_settings.serverUrlPrefix);
  Serial.print("   MQTT_IP:");
  Serial.println(g_settings.mqttIPAddress);
  Serial.print("   MQTT_PORT:");
  Serial.println((uint16_t)g_settings.mqttPort);
  Serial.print("   MQTT_USER:");
  Serial.println(g_settings.mqttUser);
  Serial.print("   MQTT_PASSWORD:");
  Serial.println(g_settings.mqttPassword);
  Serial.print("   MQTT_DISPLAYNAME:");
  Serial.println(g_settings.mqttDisplayName);
  Serial.print("   MQTT DISCOVERYPREFIX:");
  Serial.println(g_settings.mqttDiscoveryPrefix);
  Serial.println("-----------------------");
  Serial.print("size:");
  Serial.println(sizeof(g_settings));
  Serial.println("-----------------------");

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put (CONFIG_DATA_START_ADDR, g_settings);
  EEPROM.commit();
  EEPROM.end();
}

/**
 * Stores State State in EEPROM
 */
void saveStateToEEPROM() {
  Serial.print("Saving EEPROM State:");
  Serial.println(g_state, BIN);

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put (STATE_DATA_START_ADDR, g_state);
  EEPROM.commit();
  EEPROM.end();
  Serial.print("size:");
  Serial.println(sizeof(g_state));
}

/**
 * Stores PIN State in EEPROM
 */
void loadStateFromEEPROM() {
  Serial.print("Loading EEPROM State:");

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get( STATE_DATA_START_ADDR, g_state);
  EEPROM.end();
  Serial.println(g_state, BIN);  
  Serial.print("size:");
  Serial.println(sizeof(g_state));
}

/**
 * replaces characters in a string
 */
char * replace(char * aInput, char aFind, char aReplaced) 
{
    char * output = (char*)malloc(strlen(aInput));

    for (int i = 0; i < strlen(aInput); i++)
    {
        if (aInput[i] == aFind) output[i] = aReplaced;
        else output[i] = aInput[i];
    }

    output[strlen(aInput)] = '\0';

    return output;
}

/**
 * Splits a string into an array
 */
void split(char* aString, const char *aSeparator, char *aArray[], const int aMax){

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

   while(!header_found){
      //HTTP protocol ends each line wi CRLF
      String hdr_line = aClient.readStringUntil('\n');
      Serial.print("HEADER LINE[");
      Serial.print(hdr_line.length());
      Serial.print("]:");
      Serial.println(hdr_line);

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
//           Serial.println("END OF HEADER");
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
 * Handles all Get Requests 
 * These will not change the state of the system
 * 
 * This is basically the setting page
 */
void handleGet(WiFiClient& aClient, char* aEndPoint){
  int num_seps = getNumOccurrances(aEndPoint, END_POINT_SEPARATOR);
  
  char* ep_commands[num_seps];
  split(aEndPoint, END_POINT_SEPARATOR, ep_commands, num_seps);

  Serial.print("End Point:");
  Serial.println(aEndPoint);      

  if(0==strcmp(ADMIN_COMMAND, ep_commands[0])){
    handleAdminView(aClient, ep_commands);
  } else {
    handleUnsupported(aClient, aEndPoint);    
  }
  
  Serial.print("Get Complete\n");
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
  if(0==strcmp(SETTINGS, aCommandArray[1])){
    showSettingsChangeForm(aClient);     
  } else {
    handleUnsupported(aClient, aCommandArray[1]);    
  }
  Serial.print("Admin View Complete\n");

}

/**
 * Creates WiFi SSID/ Password details page
 */
void showSettingsChangeForm(WiFiClient& aClient){

   Serial.println("Showing Setttings Form:");
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
         "   <form id=\"contact-form\" action=\"%s\" method=\"post\" "
         "         enctype=\"application/x-www-form-urlencoded\" >\n"
//         "         enctype=\"multipart/form-data\" >\n"
//         "         enctype=\"text/plain\" >\n"
         "      <fieldset>\n"
         "         <legend>Configuration</legend>\n"
         "         <ul>\n"
         "            <li>\n"
         "               <label for=\"ssid\">SSID:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%s\" />\n"
         "            </li>\n"        
         "            <li>\n"
         "               <label for=\"wifi-password\">Password:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%s\" />\n"
         "            </li>\n"
         "         </ul>\n"
         "         <legend>Instance</legend>\n"
         "           <p>This will be used to generate the following:<br>"
         "           Server end point: &lt;prefix&gt;/&lt;id&gt;/&lt;status&gt;<br>" 
         "           MQTT Topic      : &lt;prefix&gt;/&lt;id&gt;<br>"
         "           MQTT Client ID  : esp32c3-id-MAC-ADDR<br>"
         "           </p>"
         "         <ul>\n"
         "            <li>\n"
         "               <label for=\"instance-id\">ID:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%s\" />\n"
         "            </li>\n"        
         "         </ul>\n"
         "         <legend>Server Configuration</legend>\n"
         "         <ul>\n"
         "            <li>\n"
         "               <label for=\"server-ip\">Server IP Address:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%s\" />\n"
         "            </li>\n"        
         "            <li>\n"
         "               <label for=\"server-port\">Server Port:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%d\" />\n"
         "            </li>\n"
         "            <li>\n"
         "               <label for=\"server-url-prefix\">Server Url Prefix:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%s\" />\n"
         "            </li>\n"
         "         </ul>\n"
         "         <legend>MQTT Configuration</legend>\n"
         "         <ul>\n"
         "            <li>\n"
         "               <label for=\"mqtt-ip\">MQTT IP Address:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%s\" />\n"
         "            </li>\n"        
         "            <li>\n"
         "               <label for=\"mqtt-port\">MQTT Port:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%d\" />\n"
         "            </li>\n"
         "            <li>\n"
         "               <label for=\"mqtt-user\">MQTT IP User:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%s\" />\n"
         "            </li>\n"        
         "            <li>\n"
         "               <label for=\"mqtt-password\">MQTT Password:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%s\" />\n"
         "            </li>\n"
         "         </ul>\n"
         "         <legend>MQTT Display</legend>\n"
         "         <ul>\n"
         "            <li>\n"
         "               <label for=\"mqtt-disc-prefix\">MQTT Discovery Prefix:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%s\" />\n"
         "            </li>\n"        
         "            <li>\n"
         "               <label for=\"mqtt-display-name\">MQTT Display Name:</label>"
         "               <input type=\"text\" id=\"%s\" name=\"%s\" value=\"%s\" />\n"
         "            </li>\n"        
         "            <li class=\"button\">\n"
         "               <button type=\"submit\">Set</button>\n"
         "            </li>\n"
         "         </ul>\n"
         "      </fieldset>\n"
         "   </form>\n"
         "</body>\n",
         HTTP_VERSION, HTTP_CODE, HTTP_HDR_CONTENT_TYPE_TAG, HTTP_HDR_CONTENT_TYPE_HTML,
         SET_CONFIG_COMMAND, WIFI_SSID, WIFI_SSID, g_settings.ssid, WIFI_PASSWORD, WIFI_PASSWORD, g_settings.wifiPassword, 
         INSTANCE_ID, INSTANCE_ID, g_settings.instanceId, SERVER_IP, SERVER_IP, g_settings.serverIPAddress, 
         SERVER_PORT, SERVER_PORT, g_settings.serverPort,
         SERVER_URL_PREFIX, SERVER_URL_PREFIX, g_settings.serverUrlPrefix,
         MQTT_IP, MQTT_IP, g_settings.mqttIPAddress, MQTT_PORT, MQTT_PORT, g_settings.mqttPort,
         MQTT_USER, MQTT_USER, g_settings.mqttUser, MQTT_PASSWORD, MQTT_PASSWORD, g_settings.mqttPassword,
         MQTT_DISCOVERY_PREFIX, MQTT_DISCOVERY_PREFIX, g_settings.mqttDiscoveryPrefix,
         MQTT_DISPLAY_NAME, MQTT_DISPLAY_NAME, g_settings.mqttDisplayName);

  Serial.print("Size of web page:");
  Serial.println(strlen(web_page));

  aClient.flush();
  aClient.print(web_page);
}

/**
 * This handles the POST command that sets the WiFi Credentials 
 * and other settings
 */
void handleUpdateSettings(WiFiClient& aClient){

   int body_size_from_hdr = 0;
   bool found_ssid = false;
   bool found_password = false;
   bool found_instance_id = false;
   bool found_server_ip = false;
   bool found_server_port = false;
   bool found_server_url_prefix = false;  //Can be optional
   bool found_mqtt_ip = false;
   bool found_mqtt_port = false;
   bool found_mqtt_user = false;
   bool found_mqtt_password = false;
   bool found_mqtt_discovery_prefix = false;
   bool found_mqtt_display_name = false;
   
   JSONVar json;
   json[JSON_CONFIG_RESET_TAG] = POST_RESP_FAILURE;

   //Reads the header
   getHeader(aClient, body_size_from_hdr);

   Serial.print("Body length from Header:");
   Serial.println(body_size_from_hdr);      

   //Reads Body
   aClient.setTimeout(SERVER_READ_TIMEOUT_MS);

   char body_buf[body_size_from_hdr+1];
   memset(body_buf, '\0', body_size_from_hdr+1);
   
   char* body_p = body_buf;
   int b_read = aClient.readBytes(body_buf, body_size_from_hdr);
   Serial.print("Bytes Read:");    
   Serial.println(b_read);    
   Serial.print("buffer:");    
   Serial.println(body_buf);    
  
   if('\n' == body_buf[0]){
      Serial.println("First char is newline removing");
       ++body_p;
   }

   //Replace URL encoded + to be a single space
   char* spaced_body = replace(body_p, '+', ' ');

   //This decodes the special characters like @ etc which are encoded as %40 (URL Decoding)
   char decoded_str[percent::decodeLength(spaced_body)];
   percent::decode(spaced_body, decoded_str);
   Serial.print("URL Decoded Output:");
   Serial.println(decoded_str);
   
   //Gets the number of parameters (should be 2, ssid and password)
   int num_sep = getNumOccurrances(decoded_str, POST_PARAM_SEPARATOR);
   Serial.print("num of parameters:");
   Serial.println(num_sep);

   if (num_sep > 0){
      char* parameters[num_sep+1];
      split(decoded_str, POST_PARAM_SEPARATOR, parameters, num_sep);
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
               Serial.printf("Found SSID:%s len(%d)\n", key_value[1], strlen(key_value[1]));
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_WIFI_SSID_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_SSID_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {               
                 strcpy(g_settings.ssid, key_value[1]);
                 found_ssid = true;
               }

            } else if (0==strcmp(WIFI_PASSWORD, key_value[0])){
               Serial.printf("Found Password:%s len(%d)\n", key_value[1], strlen(key_value[1]));
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_WIFI_PASSWORD_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_PWD_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {               
                  strcpy(g_settings.wifiPassword, key_value[1]);
                  found_password = true;
               }

            } else if (0==strcmp(INSTANCE_ID, key_value[0])){
               Serial.printf("Found Instance ID:%s len(%d)\n", key_value[1], strlen(key_value[1]));
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_INSTANCE_ID_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_INSTANCE_ID_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {               
                  strcpy(g_settings.instanceId, key_value[1]);
                  found_instance_id = true;
               }

            } else if (0==strcmp(SERVER_IP, key_value[0])){
               Serial.printf("Found Server IP:%s len(%d)\n", key_value[1], strlen(key_value[1]));
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > HOST_IP_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_SERVER_IP_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {               
                  strcpy(g_settings.serverIPAddress, key_value[1]);
                  found_server_ip = true;
               }

            } else if (0==strcmp(SERVER_PORT, key_value[0])){
               Serial.printf("Found Server Port:%s (Min:%d, Max:%d)\n", key_value[1], MIN_SERVER_PORT, MAX_PORT);

               uint16_t value = (uint16_t)strtol(key_value[1], NULL, 10);
               if((value < MIN_SERVER_PORT) ||
                  (value > MAX_PORT)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_SERVER_PORT_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {
                  g_settings.serverPort = value;
                  found_server_port = true;                
               }               

            } else if (0==strcmp(SERVER_URL_PREFIX, key_value[0])){
               Serial.printf("Found MQTT Server URL Prefix:%s len(%d)\n", key_value[1], strlen(key_value[1]));
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_SERVER_URL_PREFIX_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_SERVER_URL_PREFIX_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {               
                  
                  strcpy(g_settings.serverUrlPrefix, key_value[1]);
                  found_server_url_prefix = true;
               }

            } else if (0==strcmp(MQTT_IP, key_value[0])){
               Serial.printf("Found MQTT IP:%s len(%d)\n", key_value[1], strlen(key_value[1]));
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > HOST_IP_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_IP_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {               
                  
                  strcpy(g_settings.mqttIPAddress, key_value[1]);
                  found_mqtt_ip = true;
               }

            } else if (0==strcmp(MQTT_PORT, key_value[0])){
               Serial.printf("Found MQTT Port:%s (Min:%d, Max:%d)\n", key_value[1], MIN_MQTT_PORT, MAX_PORT);

               uint16_t value = (uint16_t)strtol(key_value[1], NULL, 10);
               if((value < MIN_MQTT_PORT) ||
                  (value > MAX_PORT )){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_PORT_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {
                  g_settings.mqttPort = value;
                  found_mqtt_port = true;                
               }               

            } else if (0==strcmp(MQTT_USER, key_value[0])){
               Serial.printf("Found MQTT User:%s len(%d)\n", key_value[1], strlen(key_value[1]));
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_MQTT_USER_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_USER_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {               
                  strcpy(g_settings.mqttUser, key_value[1]);
                  found_mqtt_user = true;
               }

            } else if (0==strcmp(MQTT_PASSWORD, key_value[0])){
               Serial.printf("Found MQTT Password:%s len(%d)\n", key_value[1], strlen(key_value[1]));
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_MQTT_PASSWORD_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_PASSWORD_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {               
                  strcpy(g_settings.mqttPassword, key_value[1]);
                  found_mqtt_password = true;
               }
            } else if (0==strcmp(MQTT_DISCOVERY_PREFIX, key_value[0])){
               Serial.printf("Found MQTT Discovery Prefix:%s len(%d)\n", key_value[1], strlen(key_value[1]));
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_MQTT_DISC_PREFIX_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_DISCOVERY_PREFIX_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {               
                  strcpy(g_settings.mqttDiscoveryPrefix, key_value[1]);
                  found_mqtt_discovery_prefix = true;
               }

            } else if (0==strcmp(MQTT_DISPLAY_NAME, key_value[0])){
               Serial.printf("Found MQTT Display Name:%s len(%d)\n", key_value[1], strlen(key_value[1]));
               if((NULL == key_value[1]) ||
                  (strlen(key_value[1]) > MAX_MQTT_DISPLAY_NAME_LENGTH) ||
                  (strlen(key_value[1]) <= 0)){
                  json[JSON_ERROR_TAG] = JSON_ERROR_BAD_MQTT_DISPLAY_NAME_LEN;
                  Serial.println(json[JSON_ERROR_TAG]);
               } else {               
                  strcpy(g_settings.mqttDisplayName, key_value[1]);
                  found_mqtt_display_name = true;
               }
            }
         }
      }
   }

   bool success = false;
   if (found_ssid && found_password & found_server_ip & found_server_port & 
       found_server_url_prefix & found_mqtt_ip & found_mqtt_port & found_mqtt_user & 
       found_mqtt_password & found_instance_id & found_mqtt_discovery_prefix & found_mqtt_display_name ){

      zeroAllStates();
      saveStateToEEPROM();
      saveSettingsDataToEEPROM();

      json[JSON_CONFIG_RESET_TAG] = POST_RESP_SUCCESS;
      success = true;
   }   
     
   json[WIFI_SSID] = g_settings.ssid;
   json[WIFI_PASSWORD] = g_settings.wifiPassword;
   json[INSTANCE_ID] = g_settings.instanceId;
   json[SERVER_IP] = g_settings.serverIPAddress;
   json[SERVER_PORT] = g_settings.serverPort;
   json[SERVER_URL_PREFIX] = g_settings.serverUrlPrefix;
   json[MQTT_IP] = g_settings.mqttIPAddress;
   json[MQTT_PORT] = g_settings.mqttPort;
   json[MQTT_USER] = g_settings.mqttUser;
   json[MQTT_PASSWORD] = g_settings.mqttPassword;
   json[MQTT_DISCOVERY_PREFIX] = g_settings.mqttDiscoveryPrefix;
   json[MQTT_DISPLAY_NAME] = g_settings.mqttDisplayName;

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
   aClient.flush();

   if(success){
      
      delay(100);       
      removeAccessPoint();
      delay(100);       
      Serial.println("About to restart chip");  
      ESP.restart();
   }
}

/***************************************************************************
 * Sets the flag for a factory reset
 ***************************************************************************/
void factory_reset(){
   Serial.println("Reset recevied, clearing memory");
   setReset(true);
   saveSettingsDataToEEPROM();

//   g_light_state = false;
//   savePinState();
//   resetState();
   saveStateToEEPROM();

   Serial.print("About to restart chip");  
   ESP.restart();
}


/***************************************************************************
 * Sets the flag to send a discovery
 ***************************************************************************/
void discovery_reset(bool aFlag){
   Serial.println("Discovery request received");

    setMQTTDiscoverySent(aFlag);
    saveStateToEEPROM();
}

/***************************************************************************
 * Sets the flag to deep sleep mode
 ***************************************************************************/
void set_deep_sleep_mode(bool aFlag){
   Serial.print("Deep Sleep Mode Flag:");
   Serial.println(aFlag);

    setDeepSleepMode(aFlag);
    saveStateToEEPROM();
}

/**
 * Gets the battery voltage - will only work if the voltage divider has been 
 * soldered to that pin
 */
float getVCC(){
  uint32_t vbattery = 0;

  for(int i = 0; i < 16; i++){
     vbattery = vbattery + analogReadMilliVolts(VCC_PIN); //ACV with correction     
  }
  float vbattery_f = 2 * vbattery / 16 / 1000.0; // Attenuation ratio 1/2 and mV to V
  Serial.print("Battery(V):");
  Serial.println(vbattery_f);

  return vbattery_f;
}

/***************************************************************************
 * Sends the discovery Message
 ***************************************************************************/
void sendMQTTDiscoveryLights(WiFiClient& wifi_client){
   Serial.println("Sending Lights Discovery Message");

   JSONVar json_packet;

   String mqtt_discovery_topic = String(g_settings.mqttDiscoveryPrefix) + "/binary_sensor/"+String(g_settings.instanceId)+"/" + DISCOVERY_TOPIC_LIGHTS + DISCOVERY_TOPIC_SUFFIX;

   JSONVar json_dev;
   json_dev[MQTT_JSON_DEV_IDS_TAG] = g_uniq_id;
   json_dev[MQTT_JSON_DEV_NAME_TAG] = g_settings.mqttDisplayName;
   json_dev[MQTT_JSON_DEV_HW_TAG] = HW_VERSION;
   json_dev[MQTT_JSON_DEV_SW_TAG] = SW_VERSION;
   json_dev[MQTT_JSON_DEV_MANUFACTURER_TAG] = HW_MANUFACTURER;
   json_dev[MQTT_JSON_DEV_MODEL_TAG] = HW_MODEL;
   
   json_packet[MQTT_JSON_NAME_TAG] = "Lights";
   json_packet[MQTT_JSON_STAT_TOPIC_TAG] = g_mqtt_state_topic; //Must match the mqtt topic in the data packet
   json_packet[MQTT_JSON_UNIQ_ID_TAG] = g_uniq_id+"-"+MQTT_JSON_LIGHTS_TAG;
   json_packet[MQTT_JSON_ICON_TAG] = "mdi:lightbulb-group";
   json_packet[MQTT_JSON_VAL_TEMPLATE_TAG] = "{{ value_json."+MQTT_JSON_LIGHTS_TAG+" }}";
   json_packet[MQTT_JSON_FORCE_UPDATE_TAG] = true;
   json_packet[MQTT_JSON_DEV_TAG] = json_dev;
   json_packet[MQTT_JSON_PL_ON_TAG] = LIGHTS_MQTT_STATUS_ON;
   json_packet[MQTT_JSON_PL_OFF_TAG] = LIGHTS_MQTT_STATUS_OFF;
   
   sendMQTTMessage(json_packet, mqtt_discovery_topic, wifi_client);
}


/***************************************************************************
 * Sends the discovery Message
 ***************************************************************************/
void sendMQTTDiscoveryVcc(WiFiClient& wifi_client){
   Serial.println("Sending VCC Discovery Message");

   JSONVar json_packet;

   String mqtt_discovery_topic = String(g_settings.mqttDiscoveryPrefix) + "/sensor/"+String(g_settings.instanceId)+"/"+DISCOVERY_TOPIC_VCC+DISCOVERY_TOPIC_SUFFIX;

   JSONVar json_dev;
   json_dev[MQTT_JSON_DEV_IDS_TAG] = g_uniq_id;
   
   json_packet[MQTT_JSON_NAME_TAG] = "Battery";
   json_packet[MQTT_JSON_STAT_TOPIC_TAG] = g_mqtt_state_topic; //Must match the mqtt topic in the data packet
   json_packet[MQTT_JSON_UNIQ_ID_TAG] = g_uniq_id+"-"+MQTT_JSON_VCC_TAG;
   json_packet[MQTT_JSON_DEVICE_CLASS_TAG] = "voltage";
   json_packet[MQTT_JSON_MEAS_UNIT_TAG] = "V";
   json_packet[MQTT_JSON_ICON_TAG] = "mdi:battery";
   json_packet[MQTT_JSON_VAL_TEMPLATE_TAG] = "{{ value_json."+MQTT_JSON_VCC_TAG+"|default(0) }}";
   json_packet[MQTT_JSON_FORCE_UPDATE_TAG] = true;
   json_packet[MQTT_JSON_DEV_TAG] = json_dev;

   sendMQTTMessage(json_packet, mqtt_discovery_topic, wifi_client);
}

/***************************************************************************
 * Sends the discovery Message
 ***************************************************************************/
void sendMQTTDiscoveryTts(WiFiClient& wifi_client){
   Serial.println("Sending TTS Discovery Message");

   JSONVar json_packet;

   String mqtt_discovery_topic = String(g_settings.mqttDiscoveryPrefix) + "/sensor/"+String(g_settings.instanceId)+"/"+DISCOVERY_TOPIC_TTS+DISCOVERY_TOPIC_SUFFIX;

   JSONVar json_dev;
   json_dev[MQTT_JSON_DEV_IDS_TAG] = g_uniq_id;
   
   json_packet[MQTT_JSON_NAME_TAG] = "Time To Sleep";
   json_packet[MQTT_JSON_STAT_TOPIC_TAG] = g_mqtt_state_topic; //Must match the mqtt topic in the data packet
   json_packet[MQTT_JSON_UNIQ_ID_TAG] = g_uniq_id+"-"+MQTT_JSON_TTS_TAG;
   json_packet[MQTT_JSON_ICON_TAG] = "mdi:clock-outline";
   json_packet[MQTT_JSON_MEAS_UNIT_TAG] = "ms";
   json_packet[MQTT_JSON_VAL_TEMPLATE_TAG] = "{{ value_json."+MQTT_JSON_TTS_TAG+"|default(0) }}";
   json_packet[MQTT_JSON_FORCE_UPDATE_TAG] = true;
   json_packet[MQTT_JSON_DEV_TAG] = json_dev;
   
   sendMQTTMessage(json_packet, mqtt_discovery_topic, wifi_client);
}

/***************************************************************************
 * Sends the discovery Message
 ***************************************************************************/
void sendMQTTDiscoveryLsc(WiFiClient& wifi_client){
   Serial.println("Sending LSC Discovery Message");

   JSONVar json_packet;

//   String mqtt_discovery_topic = String(g_settings.mqttDiscoveryPrefix) + "/binary_sensor/"+String(g_settings.instanceId)+"/" + DISCOVERY_TOPIC_LSC + DISCOVERY_TOPIC_SUFFIX;
   String mqtt_discovery_topic = String(g_settings.mqttDiscoveryPrefix) + "/sensor/"+String(g_settings.instanceId)+"/" + DISCOVERY_TOPIC_LSC + DISCOVERY_TOPIC_SUFFIX;

   JSONVar json_dev;
   json_dev[MQTT_JSON_DEV_IDS_TAG] = g_uniq_id;
   
   json_packet[MQTT_JSON_NAME_TAG] = "Last Connect";
   json_packet[MQTT_JSON_STAT_TOPIC_TAG] = g_mqtt_state_topic; //Must match the mqtt topic in the data packet
   json_packet[MQTT_JSON_UNIQ_ID_TAG] = g_uniq_id+"-"+MQTT_JSON_LSC_TAG;
   json_packet[MQTT_JSON_DEV_TAG] = "connectivity";
   json_packet[MQTT_JSON_ICON_TAG] = "mdi:connection";
   json_packet[MQTT_JSON_VAL_TEMPLATE_TAG] = "{{ value_json."+MQTT_JSON_LSC_TAG+" }}";
   json_packet[MQTT_JSON_FORCE_UPDATE_TAG] = true;
   json_packet[MQTT_JSON_DEV_TAG] = json_dev;
   json_packet[MQTT_JSON_PL_ON_TAG] = 200;
//   json_packet[MQTT_JSON_PL_OFF_TAG] = CONN_MQTT_STATUS_N;
   
   sendMQTTMessage(json_packet, mqtt_discovery_topic, wifi_client);   
  }

/***************************************************************************
 * Sends the discovery Message
 ***************************************************************************/
void sendMQTTDiscoveryMode(WiFiClient& wifi_client){
   Serial.println("Sending Mode Discovery Message");

   JSONVar json_packet;

   String mqtt_discovery_topic = String(g_settings.mqttDiscoveryPrefix) + "/sensor/"+String(g_settings.instanceId)+"/"+DISCOVERY_TOPIC_MODE+DISCOVERY_TOPIC_SUFFIX;

   JSONVar json_dev;
   json_dev[MQTT_JSON_DEV_IDS_TAG] = g_uniq_id;
   
   json_packet[MQTT_JSON_NAME_TAG] = "Sleep Mode";
   json_packet[MQTT_JSON_STAT_TOPIC_TAG] = g_mqtt_state_topic; //Must match the mqtt topic in the data packet
   json_packet[MQTT_JSON_UNIQ_ID_TAG] = g_uniq_id+"-"+MQTT_JSON_MODE_TAG;
   json_packet[MQTT_JSON_VAL_TEMPLATE_TAG] = "{{ value_json."+MQTT_JSON_MODE_TAG+"|default(0) }}";
   json_packet[MQTT_JSON_FORCE_UPDATE_TAG] = true;
   json_packet[MQTT_JSON_DEV_TAG] = json_dev;
   
   sendMQTTMessage(json_packet, mqtt_discovery_topic, wifi_client);
}



/***************************************************************************
 * Connects to the MQTT Server 
 * Sends a message
 ***************************************************************************/
void sendMQTTMessage(JSONVar& json_packet, String& mqtt_topic, WiFiClient& wifi_client){

   PubSubClient mqtt_client(wifi_client);
   Serial.print("Connecting to MQTT on ");
   Serial.print(g_settings.mqttIPAddress);
   Serial.print(" on port ");
   Serial.println(g_settings.mqttPort);

   mqtt_client.setBufferSize(512); 
   
   mqtt_client.setServer(g_settings.mqttIPAddress, g_settings.mqttPort);

   mqtt_client.setKeepAlive(MQTT_CONNECT_TIMEOUT_S); 
   mqtt_client.setSocketTimeout(MQTT_CONNECT_TIMEOUT_S); 

   int mqtt_connection_attempts = (MQTT_CONNECT_TIMEOUT_S * 1000) / MQTT_CONNECT_DELAY_MS;
   int total_mqtt_connection_attempts = mqtt_connection_attempts;

   Serial.printf("%s - Attempting to connect to the mqtt broker with %s/%s (%d attempts).\n", g_uniq_id.c_str(), g_settings.mqttUser, g_settings.mqttPassword, mqtt_connection_attempts);
   short old_status = MQTT_DISCONNECTED;
   Serial.print(get_readable_mqtt_connection_status(old_status));
   while ((!mqtt_client.connected()) && (0 < mqtt_connection_attempts)) {
            
      boolean connected = mqtt_client.connect(g_uniq_id.c_str(), g_settings.mqttUser, g_settings.mqttPassword);
      short status_code = mqtt_client.state();
      
      if(old_status == status_code){
        Serial.print(".");
      } else {
         Serial.println("");
         Serial.print(get_readable_mqtt_connection_status(status_code));
      }
      if (MQTT_CONNECTED != status_code){
         delay(MQTT_CONNECT_DELAY_MS); //To prevent looping
         --mqtt_connection_attempts;
      }
      old_status = status_code;
   }
   
   if(mqtt_client.connected()){
      Serial.printf("\nConnected after %d attempts \n", (total_mqtt_connection_attempts - mqtt_connection_attempts));

      bool pub_state = mqtt_client.publish(mqtt_topic.c_str(), JSON.stringify(json_packet).c_str());
      /**************************************************************************************/

      if(pub_state){
         Serial.println("MQTT Published OK");
      } else {
         Serial.printf("MQTT failed to publish (error code=%d)\n", pub_state);    
      }

      Serial.print("MQTT Topic:");
      Serial.println(mqtt_topic);
      Serial.print("MQTT JSON:");
      Serial.println(JSON.stringify(json_packet).c_str());

      Serial.print("JSON Len:");
      Serial.println(JSON.stringify(json_packet).length());

   } else {
      Serial.printf("\nUnable to connect to MQTT after %d attempts \n", (total_mqtt_connection_attempts - mqtt_connection_attempts));    
   }
}

/***************************************************************************
 * This prints the reason the chip woke up. Should usually be timer
 ***************************************************************************/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason){
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}


/*****************************************************************
 * 
 * This is the main loop. This runs once the setup is complete
 * 
 ****************************************************************/
void loop() {
  if(CLIENT_MODE == g_mode){
     doClient();
  } else {
     doServer();    
  }
}


/**
 * Main loop when in server mode - waits for everything, including WiFi details
 * to be set
 */
void doServer() {
//  Serial.print("\nLoopy");
  
 // Check if a client has connected
  WiFiClient client = server.available();

  if (!client) {
    delay(1);
    return;
  }
  client.setTimeout(WIFI_CLIENT_READ_TIMEOUT_MS);
    
  while(!client.available()){
    delay(1);
  }
  Serial.print("\nNew client connected from ");
  Serial.println(client.remoteIP());
  
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

   float vcc = getVCC();
   int time_to_sleep_ms = MIN_TTS_SLEEP_PERIOD_MS;
   bool server_connection_status = false;
   unsigned int seqid = 0;
   bool reset = false;

   WiFiClient wifi_client;  // or WiFiClientSecure for HTTPS

   char *sleep_mode_text;
   
   HTTPClient http;
   String end_point = String(g_settings.instanceId) + "/status";

   char server_url[MAX_SERVER_URL_LEN];
   sprintf(server_url, "http://%s:%d/%s/%s", g_settings.serverIPAddress, g_settings.serverPort, g_settings.serverUrlPrefix, end_point.c_str());

   Serial.printf("Sending request to %s\n", server_url);
   http.begin(wifi_client, server_url);
   Serial.print("Calling GET:");
   int resp_code = http.GET();
   Serial.println(resp_code);

   if(FORCE_RESET){
      factory_reset();
   }

   JSONVar json_packet;
//   json_packet[MQTT_JSON_LSC_TAG] = CONN_MQTT_STATUS_N;    
   json_packet[MQTT_JSON_LSC_TAG] = 0;    
   json_packet[MQTT_JSON_VCC_TAG]  = String(vcc, 2); 

   if(resp_code == 200) {
//      g_has_been_successful_at_least_once = true;
//      g_failures_count = 0;
      server_connection_status = true;
      // Print the response
      json_packet[MQTT_JSON_LSC_TAG] = resp_code;    

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

      if (JSON.typeof(resp_json) == "undefined") {
         Serial.println("Parsing input failed!");
      } else {

         if(null != resp_json[MQTT_JSON_LIGHTS_TAG]){
            if (0 == strcmp(resp_json[MQTT_JSON_LIGHTS_TAG], LIGHTS_SERVER_STATUS_ON)){
               Serial.println("Server says lights are on");
               json_packet[MQTT_JSON_LIGHTS_TAG] = LIGHTS_MQTT_STATUS_ON;  
               setLights(true); 
            } else if (0 == strcmp(resp_json[MQTT_JSON_LIGHTS_TAG], LIGHTS_SERVER_STATUS_OFF)){
               Serial.println("Server says lights are off");
               json_packet[MQTT_JSON_LIGHTS_TAG] = LIGHTS_MQTT_STATUS_OFF;  
               setLights(false);
            } 
         } else {
             Serial.println("No Lights instruction from Server - using OFF");          
             setLights(false);
         }
         
         if(null != resp_json[MQTT_JSON_TTS_TAG]){
            time_to_sleep_ms = atoi(resp_json[MQTT_JSON_TTS_TAG]);
            Serial.print("Time To Sleep recevied(ms):");

            if(time_to_sleep_ms > MAX_TTS_SLEEP_PERIOD_MS){
              Serial.print("TTS is larger than max allowed:");
              Serial.println(MAX_TTS_SLEEP_PERIOD_MS);
              time_to_sleep_ms = MAX_TTS_SLEEP_PERIOD_MS;
            } else if (time_to_sleep_ms < MIN_TTS_SLEEP_PERIOD_MS){
              Serial.print("TTS is smaller than min allowed:");
              Serial.println(MIN_TTS_SLEEP_PERIOD_MS);              
              time_to_sleep_ms = MIN_TTS_SLEEP_PERIOD_MS;
            }
            
            Serial.print("Time To Sleep value (ms):");            
            json_packet[MQTT_JSON_TTS_TAG]  = time_to_sleep_ms;
            Serial.println(time_to_sleep_ms);            
            
         } else {
             Serial.println("No TTS instruction from Server - Not changing");          
         }

         if(null != resp_json[JSON_RESET_TAG]){
            if(0 == strcmp(resp_json[JSON_RESET_TAG], RESET_STATUS_TRUE)){
               Serial.println("Reset recevied");
                json_packet[MQTT_JSON_MODE_TAG] = MODE_SERVER;
                reset = true;
            }
         } else {
             Serial.println("No Reset instruction from Server - Not changing");          
         }

         if(null != resp_json[JSON_DISCOVERY_TAG]){
            if(0 == strcmp(resp_json[JSON_DISCOVERY_TAG], DISCOVERY_STATUS_TRUE)){
               Serial.println("Discovery Request recevied");
                discovery_reset(false);
            }
         } else {
             Serial.println("No Discovery instruction from Server - Not changing");          
         }

//         Serial.print("resp_json[JSON_SEQID_TAG]=");
//         Serial.println(resp_json[JSON_SEQID_TAG]);
         if(0 < (int)resp_json[JSON_SEQID_TAG]){
         //   seqid = atoi(resp_json[JSON_SEQID_TAG]);
            seqid = resp_json[JSON_SEQID_TAG];
            Serial.print("Sequence ID recevied(ms):");
            Serial.println(seqid);            
         } else {
             Serial.println("No Sequence ID from Server");          
         }

         if(null != resp_json[MODE_TAG]){
            if(0 == strcmp(resp_json[MODE_TAG], MODE_DEEP)){
               Serial.println("Deep Sleep Mode Found");
                json_packet[MQTT_JSON_MODE_TAG] = MODE_DEEP;
                set_deep_sleep_mode(true);
            } else { //Anything else
                json_packet[MQTT_JSON_MODE_TAG] = MODE_PROG;
                set_deep_sleep_mode(false);              
            }
         } else {
             Serial.println("No Mode instruction from Server - Setting to programattical sleep");          
             set_deep_sleep_mode(false);              
         }
      }
    
   }else {
//      ++g_failures_count;
//      Serial.printf("Failed %d/%d before reset \n", g_failures_count, MAX_FAILURES_ALLOWED_BEFORE_RESET);
      Serial.print("Error Connecting, return code=");    
      Serial.println(resp_code);      
   }
   http.end();


//   if(!isMQTTDiscoverySent()){
//      Serial.println("MQTT Discovery is not sent");        
//   }


   if(!isMQTTDiscoverySent() || (0 == (seqid % SEND_MQTT_DISCOVERY_EVERY_ITERATIONS))){
      sendMQTTDiscoveryLights(wifi_client);
      sendMQTTDiscoveryVcc(wifi_client);
      sendMQTTDiscoveryTts(wifi_client);
      sendMQTTDiscoveryLsc(wifi_client);
      sendMQTTDiscoveryMode(wifi_client);
      discovery_reset(true);
   }

   sendMQTTMessage(json_packet, g_mqtt_state_topic, wifi_client);


//   if(((g_failures_count > MAX_FAILURES_ALLOWED_BEFORE_RESET) && (!g_has_been_successful_at_least_once)) || (true == reset)){
   if(true == reset){
      factory_reset();    
   }


   if(resp_code == 200){
     if(isDeepSleepMode()){
    
        const int deep_sleep_delay_ms = 5000;
        Serial.print("About to deep sleep in ");
        Serial.print(deep_sleep_delay_ms);
        Serial.println("ms (delay for debugging reasons)");      
        delay(deep_sleep_delay_ms);    
    
    
        const int MS_TO_USECONDS = 1000;
        const int time_to_sleep_us = time_to_sleep_ms * MS_TO_USECONDS;
        esp_sleep_enable_timer_wakeup(time_to_sleep_us);
        
        Serial.print("Deep Sleeping for(ms):");
        Serial.print(time_to_sleep_ms);
        Serial.print(" which is (us):");
        Serial.println(time_to_sleep_us);
    
        esp_deep_sleep_start(); 
        //delay(time_to_sleep_ms);    
     } else {
        Serial.print("Programatical Sleeping for(ms):");
        Serial.println(time_to_sleep_ms);
        delay(time_to_sleep_ms);    
     }
   } else {
        long retry_sleep = 60000;
        Serial.print("Sleeping for(ms):");
        Serial.print(retry_sleep);
        Serial.println(" and will retry");
        delay(retry_sleep);    
    
   }

}
