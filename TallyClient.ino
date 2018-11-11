/**
   TallyClient.ino

    Original sample code (StreamClient.ino) created on: 24.05.2015

    Trimmed down to the bare-bones and reworked for the specifics by Ruvim Kondratyev on 16.09.2018 - 25.09.2018 

*/

// The includes came with the sample code: 
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

// Our own include for Wifi password: 
#include "wifi-credentials.h" 

#define CONNECT_URL "http://192.168.1.121:50513/4" 

// The debugs and PIN defines by Ruvim: 
#define DEBUG 1 

#if DEBUG 
#define TRACE(x, y) { Serial.print (x); Serial.println (y); } 
#else 
#define TRACE(x, y) ; 
#endif 

#define PIN_RED D8 
#define PIN_YEL D6 
#define PIN_GRN D4 

#define INFO_CHUNK_SIZE     4 
#define STREAM_TIMEOUT      800 

ESP8266WiFiMulti WiFiMulti;

void enableLED (int led, uint8_t state) { 
  digitalWrite (led, state); 
}

void setup() {
  #if DEBUG 
  Serial.begin (9600); 
  #endif 

  // Sample code called WiFi.mode () and WiFiMulti.addAP (): 
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP (WIFI_NETWORK, WIFI_PASSWORD); // Insert your SSID and passphrase here. 

  // Tally uses red, yellow, and green, and the built-in LED: 
  pinMode (BUILTIN_LED, OUTPUT); 

  pinMode (PIN_RED, OUTPUT);
  pinMode (PIN_YEL, OUTPUT); 
  pinMode (PIN_GRN, OUTPUT); 
}

/* 
 *  
 *  This program connects to an HTTP streaming server that listens on port 50513. 
 *  The server listens for HTTP requests and takes the requested path (for example, 
 *  this would be "/4" if the requested URL is "http://server/4" - so it's the part 
 *  after and including the slash). If the path is a slash followed by an integer, 
 *  then the server keeps sending information about that camera number to the client. 
 *  For example, if we're requesting "/4", then the server sends us information 
 *  about whether camera 4 is on air or not, or if it's on preview or on an aux out. 
 *  
 *  The server sends data chunks in INFO_CHUNK_SIZE bytes at a time, where the bytes have the 
 *  following structure: 
 *  [0] BOOL - nonzero if this camera is on air (PROGRAM) 
 *  [1] BOOL - nonzero if this camera is routed to an aux video output other than program (can be a projector, for example) 
 *  [2] BOOL - nonzero if this camera is on preview 
 *  [3] reserved; always zero for now 
 *  
 *  I might change [3] to be if this camera is fed as a source into any upstream or downstream keys, but we'll see what to do later. 
 *  
 *  The function waits for information chunks from the server, but it exits the 
 *  wait loop and tries reconnecting in a consecutive loop () call if the connection 
 *  is dropped (when http.connected () returns FALSE). However, connected () does 
 *  not always catch on right away when the server disconnects - there may be a few 
 *  seconds of delay. To catch this, and to keep the tally from lagging seconds 
 *  behind in case of a network problem, the loop () also exits if there is no 
 *  data from the server for STREAM_TIMEOUT milliseconds or longer. 
 *  
 *  The color coding for the LEDs is determined by the following logic: 
 *  RED:    If this camera is on PROGRAM or any aux output. 
 *  YELLOW: If this camera is on PREVIEW and is *NOT* on PROGRAM (mutually exclusive). 
 *  GREEN:  If this camera is *NOT* on PROGRAM and *NOT* on PREVIEW. 
 *  ALL 3:  There is a network error; attempting to reconnect ... 
 *  
 */ 

void loop() {
  static uint8_t b_initialized = 0; 
  #if DEBUG 
  static size_t loop_count = 0; 
  TRACE ("Entered loop () function; loop count before this: ", loop_count); 
  loop_count++; 
  #endif 
  // Wait for WiFi to be connected: 
  if ((WiFiMulti.run () == WL_CONNECTED)) {
    HTTPClient http; 

    // Connect to the server: 
    TRACE ("Connecting ... ", 0); 
    TRACE (CONNECT_URL, 0); 
    http.begin (CONNECT_URL); // Change to your server's IP address. 

    int httpCode = http.GET();
    TRACE ("HTTP requested; response code: ", httpCode); 
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        b_initialized = 1; 

        // The getSize () is -1 when the server sends no Content-Length header; this is the case for our tally server. 
        int len = http.getSize();

        TRACE ("HTTP OK; content-length: ", len); 

        uint8_t buff [INFO_CHUNK_SIZE] = { 0 };
        WiFiClient * stream = http.getStreamPtr();

        long lastReceive = millis (); 
        #if DEBUG 
        uint32_t lastState = 0; 
        #endif 
        while (http.connected () && len == -1 && millis () - lastReceive < STREAM_TIMEOUT) {
          size_t avl = stream->available();

          if (avl >= sizeof (buff)) {
            int c = stream->readBytes (buff, sizeof (buff));

            #if DEBUG 
            if (((uint32_t *) buff) [0] != lastState) { 
              TRACE ("State changed to: ", ((uint32_t *) buff) [0]); 
            } 
            lastState = ((uint32_t *) buff) [0]; 
            #endif 

            uint8_t red = buff[0] != 0 || buff[1] != 0; 

            digitalWrite (BUILTIN_LED, !red ? HIGH : LOW); 

            enableLED (PIN_RED, red ? HIGH : LOW); // On program, or on aux (e.g., projector). 
            enableLED (PIN_YEL, !red && buff[2] != 0 ? HIGH : LOW); // On preview. And NOT on program or aux output (for our church, aux goes to the projectors). 
            enableLED (PIN_GRN, !red && buff[2] == 0 ? HIGH : LOW); // Can move. 

            lastReceive = millis (); 
          }
          delay (1); 
        }

        TRACE ("Exited receive loop; time since last receive: ", millis () - lastReceive); 
        

        enableLED (D5, HIGH); 
        enableLED (D6, HIGH); 
        enableLED (D7, HIGH); 
        enableLED (D8, HIGH); 

      }
    } else {
      // Do nothing. 
      TRACE ("Could not get HTTP response", ""); 
    }

    http.end();
  } else if (!b_initialized) { 
    // If not initalized yet, then blink LEDs to let the user know we're loading. Helps with checking pin connections without a WiFi network. 
      enableLED (PIN_RED, 1); 
      enableLED (PIN_YEL, 0); 
      enableLED (PIN_GRN, 0); 
      delay (250); 
      enableLED (PIN_RED, 0); 
      enableLED (PIN_YEL, 1); 
      delay (250); 
      enableLED (PIN_YEL, 0); 
      enableLED (PIN_GRN, 1); 
      delay (250); 
      enableLED (PIN_GRN, 0); 
      delay (250); 
      return; 
  }

  delay(1000);
}
