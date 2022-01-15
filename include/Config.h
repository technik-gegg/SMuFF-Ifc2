#pragma once

#include <Arduino.h>
#include <LittleFS.h>

#define VERSION         "1.0.0"
#define USE_FS          1
#define DEBUG           1
#define FEEDER_IN       D5
#define FEEDER_OUT      D1
#define BAUDRATE        115200

#define ArraySize(arr) (sizeof(arr) / sizeof(arr[0]))

extern void __debugS(const char *fmt, ...);