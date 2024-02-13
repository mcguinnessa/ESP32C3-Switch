# ESP32C3-Switch
Sketches for ESP32C3 wifi switch


# ESP32C3-pull-rest
This sketch is designed to to drive a latch relay based on the instruction given by a REST interface on the same network

This sketch will do the following:

Attempt to connect to wifi given saved credentials.

  Connect to the configured REST API for status information. This is of the form 
  <hostname>/<prefix>/status

  return JSON
    {lights: "on|off", tts: integer_in_ms, discovery: "true|false", reset: "true|false"}

    lights: This will defined the state of the switch, if on it will set the configured PINs to the 'on' state, one high and one low. If off, it will do the reverse.

    tts: time to sleep, this will determine how long the ESP32 will enter sleep mode for. When in deep sleep this will be limited by the max deep sleep period

    discover: if this is set to true, then the ESP will send the MQTT discovery messages again.

    reset: if set to true, the ESP will go into server mode to allow the user to reset the configuration variables.

  It will perform any actions instructed via the JSON package. Including changing switch state, re-sending MQTT discovery message, or going into reset mode.

  Send an MQTT packet to the configured server giving state of the switch, the time it will sleep for and the success or failure state of the last connect to the server

  Sleep for the instructed time.

If the initial connection to wifi is unsuccessful, after a given time the ESP will enter server mode. This allows the user to connect to the ESP on
192.168.1.1/settings

and configure the system


#Configuration
  

TODO:


The server code for the REST interface can be found here:
https://github.com/mcguinnessa/wifi-led-control-server
