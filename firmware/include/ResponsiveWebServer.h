#pragma once
#include <WebServer.h>
#include <lwip/sockets.h>
#include <errno.h>
#include "Logger.h"

// WiFiClient::write retries EAGAIN with one-second select waits. Bound only
// HTTP output here; MQTT and incoming OTA upload handling keep their own policy.
class ResponsiveWebServer : public WebServer
{
public:
    explicit ResponsiveWebServer(int port) : WebServer(port) {}
    size_t writeHttp(const char* bytes, size_t length) { return _currentClientWrite(bytes, length); }
    void finishHttpChunks(bool complete)
    {
        // HtmlStream frames its own chunks. Prevent WebServer::_finalizeResponse
        // from appending another terminator via its unbounded footer write.
        _chunked = false;
        if (complete) writeHttp("0\r\n\r\n", 5);
    }
protected:
    size_t _currentClientWrite(const char* bytes, size_t length) override
    {
        const int socket = _currentClient.fd();
        if (socket < 0) return 0;
        size_t sent = 0;
        uint32_t progress = millis();
        const uint32_t started = progress;
        int lastError = 0;
        while (sent < length)
        {
            const size_t remaining = length - sent;
            const size_t count = remaining < 1460 ? remaining : 1460;
            const int result = ::send(socket, bytes + sent, count, MSG_DONTWAIT);
            if (result > 0)
            {
                sent += result;
                progress = millis();
            }
            else
            {
                lastError = result < 0 ? errno : 0;
                if (result == 0 || (lastError != EAGAIN && lastError != EWOULDBLOCK && lastError != EINTR)) break;
                delay(5);
            }
            if (millis() - progress >= 750 || millis() - started >= 3000) break;
        }
        if (sent != length)
        {
            logger.warningf("HTTP socket stalled: errno=%d, sent=%u/%u, heap=%u",
                lastError, static_cast<unsigned>(sent), static_cast<unsigned>(length), ESP.getFreeHeap());
            _chunked = false;
            _currentClient.stop();
        }
        return sent;
    }
    size_t _currentClientWrite_P(PGM_P bytes, size_t length) override
    {
        // ESP32 flash is memory mapped; no temporary stylesheet copy is needed.
        return _currentClientWrite(bytes, length);
    }
};
