#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "ResponsiveWebServer.h"

#include "AppState.h"
#include "Logger.h"
#include "OTAService.h"
#include "Settings.h"

#include "Web/DashboardPage.h"
#include "Web/SettingsPage.h"
#include "Web/OTAPage.h"
#include "Web/DiagnosticsPage.h"

class WebServerService
{
public:
    bool begin(
        Settings& settings,
        AppState& appState,
        Logger& logger,
        OTAService& otaService);

    void update();

    bool running() const;

private:
    ResponsiveWebServer server_{80};

    DashboardPage dashboardPage_;
    SettingsPage settingsPage_;
    OTAPage otaPage_;
    DiagnosticsPage diagnosticsPage_;

    bool started_ = false;
    AppState* state_ = nullptr;

    void registerRoutes();
    void handleNotFound();
};
