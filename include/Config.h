#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include "StringStream.h"
#if defined(ESP32)
#include <BluetoothSerial.h>
#endif

#define VERSION         "1.1.1"
#if defined(ESP32)
#define MCUTYPE         "ESP32"
#else
#define MCUTYPE         "ESP8266"
#endif
#define USE_FS          1
#define DEBUG           1
#if defined (ESP32)
#define INTLED_PIN      2   // GPIO2 - built in LED
#define FEEDER_IN       4   // GPIO4
#define FEEDER_OUT      5   // GPIO5
#define RXD0_PIN        3   // RX0
#define TXD0_PIN        1   // TX0
#define RXD1_PIN        16  // RX2
#define TXD1_PIN        17  // TX2
#define RXD2_PIN        18  // GPIO18
#define TXD2_PIN        19  // GPIO19
#define BTN_PIN         14  // GPIO14
#define LED_PIN         12  // GPIO12
#else
#define FEEDER_IN       D5
#define FEEDER_OUT      D1
#endif
#define BAUDRATE        115200

#define ArraySize(arr) (sizeof(arr) / sizeof(arr[0]))

#define MIME_JSON       "text/json"
#define MIME_HTML       "text/html"
#define MIME_TEXT       "text/plain"

extern WiFiManager      wifiMgr;
extern int              wifiBtn;
extern char             deviceName[];
extern HardwareSerial   SerialDuet;
extern HardwareSerial   SerialSmuff;
#if defined(ESP32)
extern HardwareSerial   SerialDue;
extern BluetoothSerial  SerialBT;
#endif
extern StringStream     debugOut;
extern unsigned long    jsonData, duetSent, smuffSent, dueSent, btSent;
extern int              feederState, buttonState, btConnections;


extern void initWebserver();
extern void loopWebserver();
extern void initDisplay();
extern void resetDisplay();
extern void drawIPAddress(const char* buf);
extern void drawAP(const char* buf);
extern void drawScreen();
extern void flashIntLED(int repeat, int _delay = 300);
extern void __debugS(const char *fmt, ...);
extern int  __debugESP(const char* fmt, va_list arguments);
