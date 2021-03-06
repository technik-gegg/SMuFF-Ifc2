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
#if defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <HTTPClient.h>
#include <NetBIOS.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266NetBIOS.h>
#endif
#include <WebSocketsServer.h>

WiFiManager             wifiMgr((Stream&)debugOut);
#if defined(ESP32)
WebServer               webServer(80);
HTTPUpdateServer        httpUpdateServer;
#else
ESP8266WebServer        webServer(80);
ESP8266HTTPUpdateServer httpUpdateServer;
#endif
WebSocketsServer        webSocketServer = WebSocketsServer(8080);
char                    deviceName[50];
int                     wsClientsConnected = 0;
int                     curClient = -1;

extern unsigned long wiSent;

#define WS_CHUNK_SIZE   256

void sendResponse(int status, const char* mime, String value) {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("X-Content-Type-Options", "no-sniff");
    webServer.send(status, mime, value);
}

void sendOkResponse() {
    sendResponse(200, MIME_TEXT, String("ok"));
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
    if(!webServer.uri().startsWith("/wi/assets"))   // don't print any debug message for WI 404
        __debugS("404: '%s' not found", webServer.uri().c_str());
}

void initWebserver() {
    
    uint8_t mac[6];
    WiFi.macAddress(mac);
    sprintf(deviceName, "SMuFF-Ifc2_%02X%02X%02X", mac[3], mac[4], mac[5]);

    #if !defined(ESP32)
    wifi_set_sleep_type(NONE_SLEEP_T);
    #else
    esp_wifi_set_ps(WIFI_PS_NONE);
    #endif
    
    wifiMgr.setConfigPortalBlocking(false);
    wifiMgr.setClass("invert");
    if(wifiMgr.autoConnect(deviceName)){
        __debugS("WiFi connected");
    }

    if(MDNS.begin(deviceName)) {
        MDNS.addService("SMuFF-Ifc2", "tcp", 80);
        __debugS("MDNS set to: %s", deviceName);
    }
    if(NBNS.begin(deviceName)) {
        __debugS("NetBios set to: %s", deviceName);
    }

    httpUpdateServer.setup(&webServer);
    webServer.on("/status", HTTP_GET, []() {
        char status[255];
        sprintf(status, "{ \"Device\": \"%s\", \"Version\": \"%s\",\"Variant\": \"%s\", \"IPAddress\":\"%s\", \"Feeder\": %d }", deviceName, VERSION, MCUTYPE, WiFi.localIP().toString().c_str(), digitalRead(FEEDER_OUT));
        sendResponse(200, MIME_JSON, String(status));
    });
    webServer.on("/fstatus", HTTP_GET, []() {
        feederState = digitalRead(FEEDER_OUT);
        sendResponse(200, MIME_TEXT, String(feederState));
    });
    webServer.on("/tstatus", HTTP_GET, []() {
        char status[255];
        sprintf(status, "{ \"Duet\": %ld, \"SMuFF\": %ld,\"Bluetooth\": %ld, \"PanelDue\": %ld }", duetSent, smuffSent, btSent, dueSent);
        sendResponse(200, MIME_JSON, String(status));
    });
    webServer.on("/bstatus", HTTP_GET, []() {
        #if defined(ESP32)
        buttonState = digitalRead(BTN_PIN);
        sendResponse(200, MIME_TEXT, String(buttonState));
        #endif
    });
    webServer.on("/debug", HTTP_GET, []() {
        sendResponse(200, MIME_TEXT, String(debugOut.toString()));
        debugOut.clear();
    });
    webServer.on("/clear", HTTP_GET, []() {
        debugOut.clear();
        sendOkResponse();
    });
    webServer.on("/tosmuff", HTTP_GET, []() {
        String value = webServer.arg("v");
        SerialSmuff.println(value.c_str());
        __debugS("Injecting to SMuFF: %s", value.c_str());
        sendOkResponse();
    });
    webServer.on("/toduet", HTTP_GET, []() {
        String value = webServer.arg("v");
        SerialDuet.println(value.c_str());
        __debugS("Injecting to Duet: %s", value.c_str());
        sendOkResponse();
    });
    #if defined(ESP32)
    webServer.on("/tobt", HTTP_GET, []() {
        String value = webServer.arg("v");
        SerialBT.println(value.c_str());
        __debugS("Injecting to SMuFF-WI: %s", value.c_str());
        sendOkResponse();
    });
    webServer.on("/todue", HTTP_GET, []() {
        String value = webServer.arg("v");
        SerialDue.println(value.c_str());
        __debugS("Injecting to PanelDue: %s", value.c_str());
        sendOkResponse();
    });
    #endif
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
        sendOkResponse();
    });
    webServer.on("/locate", HTTP_GET, []() {
        #if defined(ESP32)
            flashIntLED(10);
        #endif
    });
    webServer.on("/reset", HTTP_GET, []() {
        String value = webServer.arg("v");
        if(value == "wifi") {
            webServer.sendHeader("Location","http://192.168.4.1");
            webServer.send(303);
            wifiMgr.resetSettings();                // wipe credentials
        }
        if(value == "me") {
            ESP.restart();
        }
    });
    webServer.on("/wi", HTTP_GET, []() {
        #if defined(ESP32)
            webServer.sendHeader("Location","/wi/index.html");
            webServer.send(303);
        #endif
    });

    #if defined(USE_FS)
    webServer.serveStatic("/", LittleFS, "/");
    #if defined(ESP32)
    // On ESP32 serveStatic() isn't doing what it's supposed to, when using LittleFS (see issue #6172)!
    // 
    // Constructor of class "StaticRequestHandler" in file RequestHandlersImpl.h needs a modification:
    //
    //      _isFile = (strrchr(path, '.') != nullptr);
    //
    // instead of: 
    //
    //      _isFile = fs.exists(path);
    // 
    // Otherwise one needs to explicitly define each and every file that needs to be served, as shown below 
    // in the commented section.
    /*
    webServer.serveStatic("/assets/bootstrap/css/bootstrap.min.css",    LittleFS, "/assets/bootstrap/css/bootstrap.min.css");
    webServer.serveStatic("/assets/css/styles.min.css",                 LittleFS, "/assets/css/styles.min.css");
    webServer.serveStatic("/assets/img/Logo.png",                       LittleFS, "/assets/img/Logo.png");
    webServer.serveStatic("/assets/img/fav16.png",                      LittleFS, "/assets/img/fav16.png");
    webServer.serveStatic("/assets/img/fav32.png",                      LittleFS, "/assets/img/fav32.png");
    webServer.serveStatic("/assets/img/fav180.png",                     LittleFS, "/assets/img/fav180.png");
    webServer.serveStatic("/assets/img/fav196.png",                     LittleFS, "/assets/img/fav196.png");
    webServer.serveStatic("/assets/img/fav512.png",                     LittleFS, "/assets/img/fav512.png");
    webServer.serveStatic("/assets/js/script.min.js",                   LittleFS, "/assets/js/script.min.js");
    */
    #endif
    #endif

    webServer.onNotFound(handle404);
    #if !defined(ESP32)
    webServer.keepAlive(true);
    #else
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
        // disconnected... seems to be a bug in the ESP library
        // try to reconnect using WiFiManager
        if(wifiMgr.autoConnect(deviceName)){
            __debugS("WiFi reconnected");
        }
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    #endif
    webServer.begin();
    __debugS("Webserver running");
}

void hexDump(const void *mem, uint32_t len, uint8_t cols = 16) {
    char line[30+cols*7];
    char dump [7];

	const uint8_t* src = (const uint8_t*) mem;
	__debugS("[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
	for(uint32_t i = 0; i < len; i++) {
		if(i % cols == 0) {
            sprintf(line, "[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
		}
		sprintf(dump, "%02X ", *src);
        strcat(line, dump);
		src++;
	}
	__debugS(line);
}

const char* wsCliPrefix = "WS client #";

void wsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    curClient = (int)num;
    switch(type) {
        case WStype_DISCONNECTED:
            __debugS("%s%u has disconnected!", wsCliPrefix, num);
            curClient = -1;
            wsClientsConnected--;
            if(wsClientsConnected < 0)
                wsClientsConnected = 0;
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocketServer.remoteIP(num);
                __debugS("%s%u has connected from %d.%d.%d.%d url: %s", wsCliPrefix, num, ip[0], ip[1], ip[2], ip[3], payload);
                wsClientsConnected++;
            }
            break;
        case WStype_TEXT:
            __debugS("%s%u sent: %s", wsCliPrefix, num, payload);
            SerialSmuff.write((char *)payload);
            wiSent++;
            break;
        case WStype_BIN:
            __debugS("%s%u sent: %u bytes", wsCliPrefix, num, length);
            hexDump(payload, length);
            break;
		case WStype_ERROR:			
            __debugS("%s%u has caused an error", wsCliPrefix, num);
            break;
		case WStype_PING:			
            __debugS("%s%u has pinged", wsCliPrefix, num);
            break;
		case WStype_PONG:			
            __debugS("%s%u has ponged", wsCliPrefix, num);
            break;
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
            __debugS("%s%u has sent an unsupported WStype (0x%04x / %d)", wsCliPrefix, num, type, type);
			break;
    }
}

void initWebsockets() {
    webSocketServer.begin();
    webSocketServer.onEvent(wsEvent);
    __debugS("WebSocketsServer running");
}

void sendToWebsocket(String& data) {
    if(wsClientsConnected == 0)
        return;
    if(curClient != -1) {
        if(data.length() > WS_CHUNK_SIZE) {
            // split data to send into fragments, each WS_CHUNK_SIZE long
            uint32_t dataLen = data.length();
            uint32_t ndx = 0;
            int fragment = 1;
            do {
                uint32_t len = ndx+WS_CHUNK_SIZE > dataLen ? dataLen-ndx : WS_CHUNK_SIZE;
                webSocketServer.sendTXT((uint8_t)curClient, data.substring(ndx, ndx+len).c_str(), len);
                // __debugS("Sent to client #%d fragment %d (ndx: %d, len: %d): [%s]", curClient, fragment, ndx, len, data.substring(ndx, ndx+len).c_str());
                ndx += WS_CHUNK_SIZE;
                fragment++;
            } while(ndx < dataLen);
        }
        else {
            webSocketServer.sendTXT((uint8_t)curClient, data);
        }
        webSocketServer.sendTXT((uint8_t)curClient, "\n");
    }
    else {
        __debugS("Invalid WS Client ID!");
    }
}

void loopWebserver() {
    wifiMgr.process();
    webServer.handleClient();
    webSocketServer.loop();
    #if !defined(ESP32)
        MDNS.update();      // ESP32 doesn't have this method
    #endif
}