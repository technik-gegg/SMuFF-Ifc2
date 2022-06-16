#pragma once

#include <stdlib.h>
#include <Arduino.h>

#define EOL     "\n"

class StringStream : Stream {

private:
    String buffer;
    unsigned int maxLen = 32768;
    unsigned int cutOff = 2048;

public:
    void setMaxLen(unsigned int len) {
        maxLen = len;
    }
    unsigned int getMaxLen() {
        return maxLen;
    }
    void setCutOff(unsigned int len) {
        cutOff = len;
    }
    unsigned int getCutOff() {
        return cutOff;
    }
    void clear() {
        buffer = "";
    }

    virtual int available() {
        return buffer.length();
    }
    virtual int read() {
        if(buffer.length()) {
            char ch = buffer[0];
            buffer.remove(0,1);
            return ch;
        }
        else 
            return -1;
    }
    virtual int peek() {
        if(buffer.length()) {
            char ch = buffer[0];
            return ch;
        }
        else 
            return -1;
    }
    virtual size_t write(uint8_t ch) {
        buffer += (char)ch;
        return 1;
    };
    virtual size_t write(const char *str) {
        buffer += str;
        return strlen(str);
    };
    virtual size_t write(const void *buf, uint32_t len) {
        char tmp[len+1];
        memcpy(tmp, buf, len);
        tmp[len] = 0;
        buffer += String(tmp);
        return len;
    };
    size_t print(const char *buf) {
        buffer += String(buf);
        return strlen(buf)+1;
    };
    size_t print(char c) {
        buffer += String(c);
        return 1;
    };
    size_t print(String& buf) {
        buffer += buf;
        return buf.length();
    };
    size_t print(unsigned char c, int fmt = DEC) {
        String buf = String(c, fmt);
        buffer += buf;
        return buf.length();
    }
    size_t print(int v, int fmt = DEC) {
        String buf = String(v, fmt);
        buffer += buf;
        return buf.length();
    }
    size_t print(unsigned int v, int fmt = DEC) {
        String buf = String(v, fmt);
        buffer += buf;
        return buf.length();
    }
    size_t print(long v, int fmt = DEC) {
        String buf = String(v, fmt);
        buffer += buf;
        return buf.length();
    }
    size_t print(unsigned long v, int fmt = DEC) {
        String buf = String(v, fmt);
        buffer += buf;
        return buf.length();
    }
    size_t print(double v, int fmt = 2) {
        String buf = String(v, fmt);
        buffer += buf;
        return buf.length();
    }


    size_t println(const char *buf) {
        size_t l = print((const char*)buf);
        buffer += EOL;
        return l+strlen(EOL);
    };
    size_t println(char c) {
        size_t l = print(c);
        buffer += EOL;
        return l+strlen(EOL);
    };
    size_t println(String& buf) {
        size_t l = print(buf);
        buffer += EOL;
        return l+strlen(EOL);
    };
    size_t println(unsigned char c, int fmt = DEC) {
        size_t l = print(c, fmt);
        buffer += EOL;
        return l+strlen(EOL);
    }
    size_t println(int v, int fmt = DEC) {
        size_t l = print(v, fmt);
        buffer += EOL;
        return l+strlen(EOL);
    }
    size_t println(unsigned int v, int fmt = DEC) {
        size_t l = print(v, fmt);
        buffer += EOL;
        return l+strlen(EOL);
    }
    size_t println(long v, int fmt = DEC) {
        size_t l = print(v, fmt);
        buffer += EOL;
        return l+strlen(EOL);
    }
    size_t println(unsigned long v, int fmt = DEC) {
        size_t l = print(v, fmt);
        buffer += EOL;
        return l+strlen(EOL);
    }
    size_t println(double v, int fmt = 2) {
        size_t l = print(v, fmt);
        buffer += EOL;
        return l+strlen(EOL);
    }

    size_t printf(const char * format, ...) {
        char s[1024];
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(s, (sizeof(s) / sizeof(s[0])) - 1, format, arguments);
        va_end(arguments);
        buffer += s;
        return strlen(s);
    } 

    virtual void flush() {
        if(buffer.length() > maxLen) { // trim off first 2048 characters to avoid memory overruns
          int ofs = buffer.indexOf("\n", cutOff);
          buffer.remove(0, ofs);
        }
    }

    StringStream() {
        buffer.reserve(512);
    }

    const char* toString() {
        return buffer.c_str();
    }

    const String& get() {
        return buffer;
    }
};