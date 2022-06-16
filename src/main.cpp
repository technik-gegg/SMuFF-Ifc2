/**
 * SMuFF IFC2 Firmware
 * Copyright (C) 2022 Technik Gegg
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "Config.h"
#include <RingBuf.h>

StringStream    debugOut;

#define WM_DEBUG_PORT   (Stream&)debugOut         // route WiFiManager console messages to String
#define DEBUG_ESP_PORT  (Stream&)debugOut         // route ESP console messages to String

String          fromDuet, fromSMuFF, fromPanelDue, fromWI;
RingBuf<byte, 3072> bufFromDuet;
RingBuf<byte, 2048> bufFromSMuFF;
#if defined(ESP32)
RingBuf<byte, 1024> bufFromPanelDue;
RingBuf<byte, 512>  bufFromWI;
#endif
unsigned        millisCurrent;
unsigned        millisLast;
int             isJson = 0;
unsigned long   jsonData = 0, duetSent = 0, smuffSent = 0, dueSent = 0, btSent = 0, wiSent = 0;
bool            msgSent = false;
bool            smuffMode = false;
int             feederState = 0, buttonState = 0, btConnections = 0;
HardwareSerial  SerialDuet(0);
HardwareSerial  SerialSmuff(1);
#if defined(ESP32)
HardwareSerial  SerialDue(2);
BluetoothSerial SerialBT;
#endif

void IRAM_ATTR feederTrigger() {
  feederState = digitalRead(FEEDER_IN);
  digitalWrite(FEEDER_OUT, feederState);
  #if defined (ESP32)
  digitalWrite(LED_PIN, feederState);
  #endif
}

void sendIPAddress() {
  if(WiFi.isConnected()) {
    // Send a message to Duet
    SerialDuet.printf("M118 P0 L2 S\"SMuFF-Ifc2 has connected to network '%s'. IP-Address: %s\"\r\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  }
  else {
    SerialDuet.printf("M118 P0 L2 S\"SMuFF-Ifc2 running in AP-Mode. IP-Address: %s\"\r\n", WiFi.softAPIP().toString().c_str());
  }
}

#if defined (ESP32)
void btStatus(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    __debugS("Bluetooth serial connected");
    btConnections++;
  }
  else if (event == ESP_SPP_CLOSE_EVT) {
    __debugS("Bluetooth serial disconnected");
    btConnections--;
  }
}

void IRAM_ATTR buttonTrigger() {
  buttonState = digitalRead(BTN_PIN);
  // TODO: assign a function to the button trigger
}

// only available on ESP32 since on WEMOS-D1 the built in LED doubles as TX1!
void toggleIntLED() {
  digitalWrite(INTLED_PIN, !digitalRead(INTLED_PIN));
}

void flashIntLED(int repeat, int _delay) {
  for(int i=0; i<(repeat*2); i++) {
    toggleIntLED();
    delay(_delay);
  }
}

#endif

void setup(){
  #if defined(ESP32)
    esp_log_set_vprintf(__debugESP);
    initDisplay();
    __debugS("Display initialized...");
    drawScreen();
  #endif
  __debugS("SMuFF-Ifc2 Version %s (%s)\n", VERSION, MCUTYPE);
  __debugS("Starting...");


  // initialize serial ports
  #if !defined(ESP32)
    SerialDuet.begin(BAUDRATE);       // RXD0, TXD0
    SerialSmuff.begin(BAUDRATE);      // TXD1 only
    __debugS("Serial 1 & 2 initialized at %ld Baud", BAUDRATE);
  #else
    SerialDuet.begin (BAUDRATE, SERIAL_8N1, RXD0_PIN, TXD0_PIN);
    SerialSmuff.begin(BAUDRATE, SERIAL_8N1, RXD1_PIN, TXD1_PIN);
    SerialDue.begin  (BAUDRATE, SERIAL_8N1, RXD2_PIN, TXD2_PIN);
    __debugS("Serial 1, 2 & 3 initialized at %ld Baud", BAUDRATE);
  #endif

  #if defined(USE_FS)
    __debugS("Filesystem init...");
    if(LittleFS.begin()) {
      __debugS("ok");
    }
    else {
      __debugS("failed");
    }  
  #endif
  __debugS("Webserver init...");
  initWebserver();
  initWebsockets();
  drawScreen();
  
  #if defined(ESP32)
    // setup Bluetooth serial for SMuFF WebInterface connection (ESP32 only)
    SerialBT.begin(deviceName);
    SerialBT.register_callback(btStatus); 
    __debugS("Bluetooth Serial initialized");

    pinMode(BTN_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    pinMode(INTLED_PIN, OUTPUT);
    attachInterrupt(digitalPinToInterrupt(BTN_PIN), buttonTrigger, CHANGE);
    __debugS("External I/O initialized");
  #endif

  pinMode(FEEDER_IN, INPUT);
  pinMode(FEEDER_OUT, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(FEEDER_IN), feederTrigger, CHANGE);
  __debugS("Feeder I/O initialized");
  drawScreen();
  
  millisLast = millis();
  #if defined(ESP32)
    flashIntLED(5);
  #endif
}

void serialDuetEvent() {
  while (SerialDuet.available()) {
    int in = SerialDuet.read();
    if(in == -1)
      break;
    if(in == '\\') {                  // is it a escape sequence?
      while(!SerialDuet.available())  
        ;                             // wait for next character
      int nxt = SerialDuet.read();
      switch(nxt) {
        case '\\': in = '\\'; break;
        case 'n': in = '\n'; break;
        case 'r': in = '\r'; break;
        case 'a': in = '\a'; break;
        case 'b': in = '\b'; break;
        case 't': in = '\t'; break;
        case '@': in = 0;
                  sendIPAddress();    // special case for "\@"
                  continue;
        case 's': in = 0;             // smuffMode routes data to SMuFF instead to the PanelDue
                  smuffMode = true;
                  break;
        
        default:  
                  #if defined(ESP32)
                  SerialDue.write(in);
                  SerialDue.write(nxt);
                  #endif
                  continue;
      }
    }
    if(in != 0) {
      #if defined(ESP32)
      if(smuffMode) {
        SerialSmuff.write(in);
      }
      else {
        SerialDue.write(in);
      }
      #else
        // filter any JSON sequences coming from Duet since SMuFF won't handle it
        if(in == '{') {
          isJson++;
        }
        if(in == '}') {
          isJson--;
          fromDuet += in;
          continue;
        }
        if(isJson <= 0) {
          if(in != 0)
            SerialSmuff.write(in);
        }
      #endif
      bufFromDuet.lockedPush(in);
    }
  }
}

void serialSmuffEvent() {
  while (SerialSmuff.available()) {
    int in = SerialSmuff.read();
    if(in == -1)
      break;
    #if defined(ESP32)
      if(btConnections > 0)
        SerialBT.write(in);
    #endif
    bufFromSMuFF.lockedPush(in);
  }
}

#if defined(ESP32)
void SerialDueEvent() {
  while (SerialDue.available()) {
    int in = SerialDue.read();
    if(in == -1)
      break;
    SerialDuet.write(in);
    bufFromPanelDue.lockedPush(in);
  }
}

void serialBTEvent() {
  while (SerialBT.available()) {
    int in = SerialBT.read();
    if(in == -1)
      break;
    SerialSmuff.write(in);
    bufFromWI.lockedPush(in);
  }
}

// just for the laughs...
void goToHall() {
  int hall;
  if((hall = hallRead()) > 150) {
    long r = random(6);
    switch(r) {
      case 0: __debugS("What the Hall?"); break;
      case 1: __debugS("What are you up to?"); break;
      case 2: __debugS("Magneto, is that you?"); break;
      case 3: __debugS("Stop tickling me!"); break;
      case 4: __debugS("You bore me..."); break;
      case 5: __debugS("Dude, let go!"); break;
      // feel free to add more stupid stuff in here (and increment the number in random above)...
    }
  }
}
#endif

template<class T>
void dumpBuffer(const T& buffer, String& ref, const char* dbg, unsigned long* cntRef, bool sendWS = false) {
    if(buffer->isEmpty())
      return;
    bool stat = true;
    bool lineComplete = false;
    do {
      byte b;
      if((stat = (bool)buffer->lockedPop(b))) {
        if(b=='\n') {
          lineComplete = true;
          break;
        }
        ref += (char)b;
      }
    } while (stat);
    if(lineComplete) {
      if(sendWS)
        sendToWebsocket(ref);
      if(dbg != nullptr)
        __debugS("%s sent: %s", dbg, ref.c_str());
      ref = "";
      *cntRef += 1;
    }
}

void loop() { 

  if(SerialDuet.available()) {
    serialDuetEvent();
  }
  if(SerialSmuff.available()) {
    serialSmuffEvent();
  }
  #if defined(ESP32)
    if(SerialDue.available()) {
      SerialDueEvent();
    }
    if(SerialBT.available()) {
      serialBTEvent();
    }
  #endif

  dumpBuffer(&bufFromSMuFF, fromSMuFF, "SMuFF", &smuffSent, true);
  dumpBuffer(&bufFromDuet, fromDuet, "Duet", &duetSent, false);
  #if defined(ESP32)
    dumpBuffer(&bufFromPanelDue, fromPanelDue, "PanelDue", &dueSent, false);
    dumpBuffer(&bufFromWI, fromWI, "SMuFF-WI", &btSent, false);
  #endif

  loopWebserver();
  
  if(millis()-millisLast > 5000) {
    // send message with IP address to Duet Log
    if(!msgSent) {
      sendIPAddress();
      msgSent = true;
    }
    millisLast = millis();
  }

  if(millis() % 500 == 0) {
    #if defined(ESP32)
      goToHall();
      drawScreen();
    #endif
  }

}

int __debugESP(const char* fmt, va_list arguments) {
  char _dbg[1024];
  int res = vsnprintf(_dbg, ArraySize(_dbg) - 1, fmt, arguments);
  debugOut.println(_dbg);
  debugOut.flush();
  return res;
}

void __debugS(const char* fmt, ...)
{
    char _dbg[2048];
    va_list arguments;
    va_start(arguments, fmt);
    vsnprintf_P(_dbg, ArraySize(_dbg) - 1, fmt, arguments);
    va_end(arguments);
    debugOut.println(_dbg);
    debugOut.flush();
    // #if defined(ESP32)
    //   drawScreen();
    // #endif
}
