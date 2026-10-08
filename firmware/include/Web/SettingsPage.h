#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "AppState.h"
#include "Logger.h"
#include "Settings.h"
#include "WebPageBuilder.h"

class SettingsPage
{
public:

    void begin(
        WebServer& server,
        Settings& settings,
        AppState& state,
        Logger& logger);

    void handleLanding();

    void handleAlerts();
    void handleAlertsSave();

    void handleReporting();
    void handleReportingSave();

    void handleDevice();
    void handleDeviceSave();

    void handleConnectivity();
    void handleConnectivitySave();

    void handleMqttSave();

    String formatUptime (uint32_t totalSeconds) const;
private:

    WebServer* server_ = nullptr;
    Settings* settings_ = nullptr;
    AppState* state_ = nullptr;
    Logger* logger_ = nullptr;
};