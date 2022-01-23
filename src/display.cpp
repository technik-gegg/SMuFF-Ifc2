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
#include <U8g2lib.h>

#define BASE_FONT               u8g2_font_chikita_tr
#define SMALL_FONT              u8g2_font_4x6_tf 

#if defined(OLED_SSD1306)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C    display(U8G2_R0, /* reset= */ U8X8_PIN_NONE);
#else
U8G2_SH1106_128X64_NONAME_F_HW_I2C     display(U8G2_R0, /* reset= */ U8X8_PIN_NONE);
#endif

void resetDisplay() {
    display.clearDisplay();
    display.setFont(BASE_FONT);
    display.setFontMode(0);
    display.setDrawColor(1);
}

void initDisplay() {
    display.begin();
    display.enableUTF8Print();
    resetDisplay();
}

void drawIPAddress(const char* buf) {
    display.setDrawColor(2);
    display.drawUTF8(0, display.getMaxCharHeight(), buf);
    display.setDrawColor(1);
}

void drawAP(const char* buf) {
    display.setDrawColor(2);
    display.drawUTF8(display.getDisplayWidth()-display.getUTF8Width(buf)-2, display.getMaxCharHeight(), buf);
    display.sendBuffer();
    display.setDrawColor(1);
}

void drawScreen() {
    char tmp[128];
    int i = 0;
    int lh = display.getMaxCharHeight();
    display.clearBuffer();
    sprintf(tmp, "%s",  deviceName);
    display.drawUTF8(0, i * lh + lh, tmp);
    i++;
    if(WiFi.isConnected()) {
        sprintf(tmp, "%s  (%s)", WiFi.localIP().toString().c_str(), WiFi.SSID().c_str());
    }
    else {
        sprintf(tmp, "%s", WiFi.softAPIP().toString().c_str());
    }
    display.drawUTF8(0, i * lh + lh, tmp);
    i++;
    sprintf(tmp, "%7ld", duetSent);
    display.drawUTF8(0, i * lh + lh, "Duet:");
    display.drawUTF8(70, i * lh + lh, tmp);
    i++;
    sprintf(tmp, "%7ld", smuffSent);
    display.drawUTF8(0, i * lh + lh, "SMuFF:");
    display.drawUTF8(70, i * lh + lh, tmp);
    i++;
    sprintf(tmp, "BT (%d):", btConnections);
    display.drawUTF8(0, i * lh + lh, tmp);
    sprintf(tmp, "%7ld", btSent);
    display.drawUTF8(70, i * lh + lh, tmp);
    i++;
    sprintf(tmp, "%7ld", dueSent);
    display.drawUTF8(0, i * lh + lh, "PanelDue:");
    display.drawUTF8(70, i * lh + lh, tmp);
    i++;
    sprintf(tmp, "%7s",  feederState ? "HIGH" : "LOW");
    display.drawUTF8(0, i * lh + lh, "Feeder:");
    display.drawUTF8(70, i * lh + lh, tmp);
    i++;

    display.sendBuffer();
}
#endif
