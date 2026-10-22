#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "ResponsiveWebServer.h"
#include "Logger.h"

// Fixed per-request storage: HTML size no longer determines heap allocation.
class HtmlStream {
public:
    explicit HtmlStream(WebServer& server) : server_(static_cast<ResponsiveWebServer&>(server)) {}
    void start() {
        used_ = 0; failed_ = false; started_ = millis();
        server_.sendHeader("Cache-Control", "no-store");
        server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server_.send(200, "text/html; charset=utf-8", "");
    }
    HtmlStream& operator+=(const String& value) { append(value.c_str(), value.length()); return *this; }
    HtmlStream& operator+=(const char* value) { append(value, strlen(value)); return *this; }
    HtmlStream& operator+=(const __FlashStringHelper* value) {
        const char* bytes = reinterpret_cast<const char*>(value);
        append(bytes, strlen_P(bytes)); return *this;
    }
    HtmlStream& operator+=(char value) { append(&value, 1); return *this; }
    void finish() {
        flush();
        server_.finishHttpChunks(!failed_);
    }
private:
    ResponsiveWebServer& server_;
    static constexpr size_t HEADER_SPACE = 16;
    static constexpr size_t PAYLOAD_SIZE = 1024;
    char buffer_[HEADER_SPACE + PAYLOAD_SIZE + 2];
    size_t used_ = 0;
    bool failed_ = false;
    uint32_t started_ = 0;
    void append(const char* bytes, size_t length) {
        while (length && !failed_) {
            size_t count = PAYLOAD_SIZE - used_;
            if (count > length) count = length;
            memcpy(buffer_ + HEADER_SPACE + used_, bytes, count);
            used_ += count; bytes += count; length -= count;
            if (used_ == PAYLOAD_SIZE) flush();
        }
    }
    void flush() {
        if (failed_ || !used_) return;
        if (!server_.client().connected() || millis() - started_ > 5000) {
            failed_ = true; server_.client().stop();
            logger.warning("HTTP HTML stream stopped: disconnected or response deadline exceeded");
            return;
        }
        // Send framing and payload together to avoid separate small TCP writes.
        char header[16];
        const int size = snprintf(header, sizeof(header), "%x\r\n", static_cast<unsigned>(used_));
        memcpy(buffer_ + HEADER_SPACE - size, header, size);
        memcpy(buffer_ + HEADER_SPACE + used_, "\r\n", 2);
        const size_t framedSize = size + used_ + 2;
        if (server_.writeHttp(buffer_ + HEADER_SPACE - size, framedSize) != framedSize)
            failed_ = true;
        used_ = 0;
        if (!server_.client().connected()) failed_ = true;
    }
};
