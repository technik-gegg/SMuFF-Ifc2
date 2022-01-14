#include "Config.h"
#include <WiFiManager.h>

extern WiFiManager  wifiMgr;
extern int          wifiBtn;

#define MIME_JSON       "text/json"
#define MIME_HTML       "text/html"
#define MIME_TEXT       "text/plain"

void initWebserver();
void loopWebserver();
