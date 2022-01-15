#include "Config.h"
#include "webserver.h"

String          debugOut;
String          fromDuet, fromSMuFF;
unsigned long   ser1cnt = 0, ser2cnt = 0, lastSer1cnt = 0, lastSer2cnt = 0;
unsigned        millisCurrent;
unsigned        millisLast;
int             isJson = 0;
unsigned long   jsonData = 0;
bool            msgSent = false;

void IRAM_ATTR feederTrigger() {
  int state = digitalRead(FEEDER_IN);
  digitalWrite(FEEDER_OUT, state);
  __debugS("Feeder signal: %s", state ? "HIGH" : "LOW");
}

void setup(){
  
  __debugS("SMuFF-Ifc2 Version %s\n", VERSION);
  Serial.begin(BAUDRATE);       // RX0, TX0
  Serial1.begin(BAUDRATE);      // TX1 only

  __debugS("Starting...");
  #if defined(USE_FS)
  if(LittleFS.begin()) {
    __debugS("FS init... ok");
  }
  else {
    __debugS("FS init... failed");
  }  
  #endif
  __debugS("WebSvr init...");
  initWebserver();
  pinMode(FEEDER_IN, INPUT);
  pinMode(FEEDER_OUT, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(FEEDER_IN), feederTrigger, CHANGE);
  millisLast = millis();
}

void serialEvent() {
  while (Serial.available()) {
    char in = Serial.read();
    if(in == '\n') {
      __debugS("Duet sent: %s", fromDuet.c_str());
      fromDuet = "";
      if(jsonData > 0) {
        __debugS("Skipped %ld bytes JSON data.", jsonData);
        jsonData = 0;
      }
    }
    else {
      fromDuet += in;
    }
    // filter any JSON sequences coming from Duet since SMuFF won't handle it
    if(in == '{') {
      isJson++;
      continue;
    }
    if(in == '}') {
      isJson--;
      continue;
    }
    if(isJson <= 0) {
      Serial1.write(in);
      ser1cnt++;
    }
    else {
      jsonData++;
    }
  }
}

void serial1Event() {
  while (Serial1.available()) {
    char in = Serial1.read();
    if(in == '\n') {
      __debugS("SMuFF sent: %s", fromSMuFF.c_str());
      fromSMuFF = "";
    }
    else {
      fromSMuFF += in;
    }
    Serial.write(in);
    ser2cnt++;
  }
}


void loop() { 

  loopWebserver();
  
  if(millis()-millisLast > 5000) {
    if(WiFi.isConnected() && !msgSent) {
      // Send a message to Duet
      Serial.printf("M118 P0 L2 S\"SMuFF-Ifc2 has connected to network '%s'. IP-Address: %s\"\r\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      msgSent = true;
    }
    millisLast = millis();
  }
}

void __debugS(const char *fmt, ...)
{
#ifdef DEBUG
    char _dbg[1024];
    va_list arguments;
    va_start(arguments, fmt);
    vsnprintf_P(_dbg, ArraySize(_dbg) - 1, fmt, arguments);
    va_end(arguments);
    debugOut += _dbg;
    debugOut += "\n";
    if(debugOut.length() > 32000) { // trim off first 2000 characters to avoid overruns
      debugOut.remove(0,2000);
      debugOut = "..." + debugOut;
    }
#endif
}
