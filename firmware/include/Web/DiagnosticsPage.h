#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "AppState.h"
#include "Settings.h"
#include "WebPageBuilder.h"
#include "EventLog.h"

class DiagnosticsPage
{
public:
    void begin(
        WebServer& server,
        AppState& state,
        Settings& settings);

    void handle();
    void handleClearLog();
private:
    WebServer* server_ = nullptr;
    AppState* state_ = nullptr;
    Settings* settings_ = nullptr;
    String eventLevelName(
    EventLevel level) const;
    String formatUptime(uint32_t seconds) const;
};