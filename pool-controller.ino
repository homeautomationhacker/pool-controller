/********************************************************************************
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    Home Automation Hacker, hereby disclaims all copyright interest in the program "pool-controller"
    written by Frank W Clements, 08 January 2019

    

    Most of the code below is lifted from various projects found around the web.
 
    Cites and credits:
      The Hookup for the intial idea and opening pandora's box:  
        https://github.com/thehookup & https://www.youtube.com/channel/UC2gyzKcHbYfqoXA5xbyGXtQ
        
      Sinric:  https://github.com/kakopappa/sinric
      
********************************************************************************/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <StreamString.h>
#include "PoolDevice.h"

// Enable debugging?  if not, comment this out and save cycles at runtime
#define DEBUG
#ifdef DEBUG
  #define DEBUG_PRINTLN(x) Serial.println(x);
  #define DEBUG_PRINT(x) Serial.print(x);
  #define DEBUG_PRINTF(x, y) Serial.printf(x, y);
#else
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT(x)
#endif

// Pin mapping between the Arduino IDE and Wemos D1 mini found here
// https://chewett.co.uk/blog/1066/pin-numbering-for-wemos-d1-mini-esp8266/
#define AUX_1      5       // Pin D1 on the Wemos D1 mini
#define AUX_2      4       // Pin D2 on the Wemos D1 mini
#define AUX_3      0       // Pin D3 on the Wemos D1 mini
#define AUX_4      2       // Pin D4 on the Wemos D1 mini
#define STATUS_LED 14      // Pin D5 on the Wemos D1 mini

// Sinric parameters
#define SINRIC_HOST     "iot.sinric.com"                      // The sinric IoT URI
#define MY_API_KEY      "xxxxxxxx-xxx-xxxx-xxxx-xxxxxxxxxxxx" // found on the https://sinric.com site
#define SINRIC_DEVICE1  "<replace with your device id here>"  // each device has a unique ID - rinse repeat for each
#define SINRIC_DEVICE2  "<replace with your device id here>"
#define SINRIC_DEVICE3  "<replace with your device id here>"
#define SINRIC_DEVICE4  "<replace with your device id here>"
#define MAX_POOL_DEVICES  4   // Maximum number of devices to support

// Wifi parameters
#define MY_SSID           "Your SSID Goes Here"     // Your SSID to connect to
#define MY_WIFI_PASSWORD  "<your_supersecret_key>"  // Your WiFi network password

// WebSockets parameters
#define HEARTBEAT_INTERVAL 300000   // 5 minute interval


// Create the variables for the ESP8266, WebSockets and WiFi client
ESP8266WiFiMulti wifiMulti;
WebSocketsClient webSocket;
WiFiClient client;

bool isConnected = false;                           // Is the Wemos connected to the WiFi network?
const int JSON_BUFFER_SIZE = JSON_OBJECT_SIZE(3);   // The size of our JSON responses from Sinric is 3 objects long

// Declare functions used in setup() and loop() - they're implemented at the end so they need to be declared here
void firePin(int triggerPin);
void turnOn(String deviceId);
void turnOff(String deviceId);
void webSocketEventCallback(WStype_t type, uint8_t *payload, size_t length);


// PoolDevice class binds the physical AUX port on the pool panel to the Sinric device
// Instantiate a PoolDevice for each auxillary pin/sinric device
PoolDevice aux1 = PoolDevice(AUX_1, SINRIC_DEVICE1);
PoolDevice aux2 = PoolDevice(AUX_2, SINRIC_DEVICE2);
PoolDevice aux3 = PoolDevice(AUX_3, SINRIC_DEVICE3);
PoolDevice aux4 = PoolDevice(AUX_4, SINRIC_DEVICE4);
PoolDevice *poolDevices[MAX_POOL_DEVICES] = {&aux1, &aux2, &aux3, &aux4};

// Main micro controller seutp 
void setup() {
  Serial.begin(115200);

  // The status LED from the panel would be the only input pin used to determine the current status of the panel
  pinMode(STATUS_LED, INPUT);

  // 
  // Setup the WiFI connection and initiate the connection
  wifiMulti.addAP(MY_SSID, MY_WIFI_PASSWORD);
  
  // Wait while the board connects to the network - foreverever, foreverever, if it never can 
  DEBUG_PRINT("Connecting to WiFi network: ");
  DEBUG_PRINTLN(MY_SSID);
  
  while(wifiMulti.run() != WL_CONNECTED)
  {
    delay(500);
    DEBUG_PRINT(".");
  }
  if(wifiMulti.run() == WL_CONNECTED)
  {
    DEBUG_PRINTLN();
    DEBUG_PRINT("\nWiFi connected!\nIP Address is: ");  
    DEBUG_PRINTLN(WiFi.localIP());
  }

  webSocket.begin(SINRIC_HOST, 80, "/");      // Connect to the Sinric servers - not a fan that this isn't SSL 
  webSocket.onEvent(webSocketEventCallback);  // Set the event handler callback and API key
  webSocket.setAuthorization("apiKey", MY_API_KEY); // Set the API key for use
  webSocket.setReconnectInterval(5000);       // Set the reconnect interval to 5 seconds
  // end wifi setup 

}

// Micro controller loop function
void loop() {
  
  uint64_t heartbeatTimestamp = 0;    // Timestamp of the last heartbeat
  
  webSocket.loop();

  if(isConnected) 
  {
    uint64_t now = millis();
  
    // Send the regular heartbeat to avoid disconnects
    if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) 
    {
      heartbeatTimestamp = now;
      webSocket.sendTXT("H");
    }
  }
}

/* Websockets callback routine that handles initial payload parsing from Sinric
 *
 * What gets sent back from Sinric is a relatively standard size JSON object that we chop
 * up and parse in here for determining which function to call, either turnOn or turnOff
 * along with which device (and then pin) to call it on.
 * Example:
 *    {"deviceId": xxxx, "action": "setPowerState", value: "ON"}
 *    
 * Since the size of the results is relatively known we use a static JSON object.
  */
void webSocketEventCallback(WStype_t type, uint8_t *payload, size_t length) {
  switch(type)
  {
    case WStype_DISCONNECTED: {
      isConnected = false;
      DEBUG_PRINT("[WebSocketCallback] Webservice disconnected from ");
      DEBUG_PRINTLN(SINRIC_HOST);
      break;
    }
    case WStype_CONNECTED: {
      isConnected = true;
      DEBUG_PRINT("[WebSocketCallback] Webservice connected to ");
      DEBUG_PRINTLN(SINRIC_HOST);
      DEBUG_PRINTLN("Waiting for commands from ");
      DEBUG_PRINTLN(SINRIC_HOST);
      break;
    }
    case WStype_TEXT: {
      // Parse the response from the Sinric API to figure out what device to call
      DEBUG_PRINTF("[WebSocketCallback] Got text response of: %s\n", payload);
      DEBUG_PRINTF("[WebSocketCallback] Lenght of message %d bytes\n", length);
      
      StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
      JsonObject &json = jsonBuffer.parseObject((char*)payload);

      String deviceId = json["deviceId"];
      String action = json["action"];
      PoolDevice *poolDevice = NULL;

      // find the matching pool device object
      for(int i = 0; i < MAX_POOL_DEVICES; i++) {
        if(poolDevices[i]->getDeviceId() == deviceId) {
          poolDevice = poolDevices[i];
        }
      }

      if(poolDevice != NULL) {
        // This only reacts to setPowerState events, and tests for logging purpose.
        if(action == "setPowerState") 
        {
          String value = json["value"];
          if(value == "ON")
          {
            poolDevice->turnOn();
          } else {
            poolDevice->turnOff();
          }
        }
        else if(action == "test")
        {
          DEBUG_PRINTLN("[WebSocketCallback] received test command from Sinric");
        }
      }
      break;
    }
    case WStype_ERROR: {
      DEBUG_PRINTLN("[WebSocketCallback] there was some kind of an error or unknown response in the payload type");
      break;
    }
    // Various other unhandled response types
    case WStype_FRAGMENT_BIN_START: {
      DEBUG_PRINT("[WebSocketCallback] received binary data start");
      break;
    }
    case WStype_FRAGMENT_TEXT_START: {
      DEBUG_PRINT("[WebSocketCallback] received fragmented text start");
      break;
    }
    case WStype_BIN: {
      DEBUG_PRINT("[WebSocketCallback] received binary data");
      break;
    }
    case WStype_FRAGMENT: {
      DEBUG_PRINT("[WebSocketCallback] received fragment");
      break;
    }
    case WStype_FRAGMENT_FIN: {
      DEBUG_PRINT("[WebSocketCallback] received fragment text stop");
      break;
    }
  }
}
