#include "webserver.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266NetBIOS.h>
#include <WiFiUdp.h>


WiFiManager             wifiMgr;
ESP8266WebServer        webServer(80);
ESP8266HTTPUpdateServer httpUpdateServer;
char                    deviceName[25];

extern String debugOut;

void sendResponse(int status, const char* mime, String value) {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("X-Content-Type-Options", "no-sniff");
    webServer.send(status, mime, value);
}

void handle404() {
    String message = "<h1>File Not Found</h1>";
    message += "<p>URI: " + webServer.uri() + "</p>";
    message += "<p>Method: " + String((webServer.method() == HTTP_GET) ? "GET" : "POST") + "</p>";
    if(webServer.args() > 0) {
        message += "<p>Arguments:</p><ul>";
        for (uint8_t i = 0; i < webServer.args(); i++) {
            message += "<li>" + webServer.argName(i) + ": " + webServer.arg(i) + "</li>";
        }
        message += "</ul>";
    }
    sendResponse(404, MIME_HTML, message);
    __debugS("404 error...");
}


void initWebserver() {
    
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    sprintf(deviceName, "SMuFF-Ifc2_%02X%02X%02X", mac[WL_MAC_ADDR_LENGTH - 3], mac[WL_MAC_ADDR_LENGTH - 2], mac[WL_MAC_ADDR_LENGTH - 1]);
    wifiMgr.setConfigPortalBlocking(false);
    
    //wifiMgr.resetSettings();                // wipe credentials
    
    if(wifiMgr.autoConnect(deviceName)){
        __debugS("WiFi connected");
    }

    wifi_set_sleep_type(NONE_SLEEP_T);

    if(MDNS.begin(deviceName)) {
        MDNS.setHostname(deviceName);
        MDNS.addService("SMuFF-Ifc2", "tcp", 80);
        __debugS("MDNS set to: %s", deviceName);
    }
    if(NBNS.begin(deviceName)) {
        __debugS("NetBios set to: %s", deviceName);
    }

    httpUpdateServer.setup(&webServer);
    webServer.on("/status", HTTP_GET, []() {
        char status[255];
        sprintf(status, "{ \"Device\": \"%s\", \"Version\": \"%s\", \"IPAddress\":\"%s\", \"Feeder\": %d }", deviceName, VERSION, WiFi.localIP().toString().c_str(), digitalRead(FEEDER_OUT));
        sendResponse(200, MIME_JSON, String(status));
    });
    webServer.on("/debug", HTTP_GET, []() {
        sendResponse(200, MIME_TEXT, debugOut);
    });
    webServer.on("/tosmuff", HTTP_GET, []() {
        String value = webServer.arg("v");
        Serial1.println(value.c_str());
        __debugS("Injecting to SMuFF: %s", value.c_str());
        sendResponse(200, MIME_TEXT, String("ok"));
    });
    webServer.on("/toduet", HTTP_GET, []() {
        String value = webServer.arg("v");
        Serial.println(value.c_str());
        __debugS("Injecting to Duet: %s", value.c_str());
        sendResponse(200, MIME_TEXT, String("ok"));
    });
    webServer.on("/feedersig", HTTP_GET, []() {
        String value = webServer.arg("v");
        if(value == "1") {
            digitalWrite(FEEDER_OUT, HIGH);
            __debugS("Injecting Feeder High");
        }
        else {
            digitalWrite(FEEDER_OUT, LOW);
            __debugS("Injecting Feeder Low");
        }
        sendResponse(200, MIME_TEXT, String("ok"));
    });

    #if defined(USE_FS)
    webServer.serveStatic("/", LittleFS, "/www/");
    #endif

    webServer.onNotFound(handle404);
    webServer.keepAlive(true);
    webServer.begin();
    __debugS("Webserver started. IP: %s", WiFi.localIP().toString().c_str());
}


void loopWebserver() {
    wifiMgr.process();
    webServer.handleClient();
    MDNS.update();
}