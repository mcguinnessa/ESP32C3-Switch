
Sketches for ESP32C3 wifi switch

### Requirements
This was written and tested with the [XIAO_ESP32C3](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/]
with the hardware adjustments made in order to read the VCC status from the A01 pin specified [here](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/#check-the-battery-voltage) 


# ESP32C3-pull-rest
This sketch is designed to to drive a latch relay based on the instruction given by a REST interface on the same network

This sketch will do the following:

### Connect To WiFi
Attempt to connect to wifi given credentials saved in EEPROM.

### Connect to REST Server
Connect to the configured REST API for status information. This is of the form:

#### URL
`<hostname>/<prefix>/status`

#### Response Message
JSON
    `{lights: "on|off", tts: integer_in_ms, discovery: "true|false", reset: "true|false"}`

- *lights*: This will defined the state of the switch, if on it will set the configured PINs to the 'on' state, one high and one low. If off, it will do the reverse.
- *tts*: time to sleep, this will determine how long the ESP32 will enter sleep mode for. When in deep sleep this will be limited by the max deep sleep period
- *discover*: if this is set to true, then the ESP will send the MQTT discovery messages again.
- *reset*: if set to true, the ESP will go into server mode to allow the user to reset the configuration variables.

### Perform Instructed Actions
It will perform any actions instructed via the JSON package. Including changing switch state, re-sending MQTT discovery message, or going into reset mode.

  
### MQTT
Send an MQTT packet to the configured server giving state of the switch, the time it will sleep for and the success or failure state of the last connect to the server

### Sleep
Sleep for the instructed time, cuurrently programmatical sleep, which is required for development


## Server Mode
If the initial connection to wifi is unsuccessful, after a given time the ESP will enter server mode. This allows the user to connect to the ESP on
192.168.1.1/admin/settings

and configure the system

### Configuration
The following parameters can be configured in server mode.
- Wifi SSID
- Wifi Password
- Instance ID (used to identify the instance to the REST server and MQTT messages)
- REST Server IP/Hostname - The server which will determine the state of the switch
- REST Server Port
- REST Server URL prefix - this is prefixed to the REST API in case any routing is needed at the server end (useful for K8s ingress routing)
- MQTT IP/Hostname - the MQTT broker address
- MQTT Port
- MQTT Password
- MQTT Discovery prefix - for instance homeassistant for the Home Assistant broker
- MQTT Display name - the human friendly name to be dsplayed for the instance in the discovery message.


### TODO:
Add in mode server response to determine whether the ESP will go to deep sleep, or normal programatical sleep for development purposes.


The server code for the REST interface can be found here:
[WiFi Switch REST Server ](https://github.com/mcguinnessa/wifi-led-control-server)
