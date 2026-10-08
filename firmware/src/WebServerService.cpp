#include "WebServerService.h"
#include "WebPageBuilder.h"


bool WebServerService::begin(
    Settings& settings,
    AppState& appState,
    Logger& logger,
    OTAService& otaService)
{
    if (started_)
    {
        return true;
    }
    
    dashboardPage_.begin(
    server_,
    appState);

    settingsPage_.begin(
    server_,
    settings,
    appState,
    logger);

    otaPage_.begin(
    server_,
    appState,
    logger,
    otaService);

    diagnosticsPage_.begin(
        server_,
        appState,
        settings);

    registerRoutes();

    server_.begin();
    started_ = true;

    logger.info(String("Web server started on port 80"));

    return true;
}

void WebServerService::update()
{
    if (!started_)
    {
        return;
    }

    server_.handleClient();
}

bool WebServerService::running() const
{
    return started_;
}

void WebServerService::registerRoutes()
{
    //
    // Dashboard
    //
    server_.on(
        "/",
        HTTP_GET,
        [this]()
        {
            dashboardPage_.handle();
        });

    //
    // OTA
    //
    server_.on(
        "/ota",
        HTTP_GET,
        [this]()
        {
            otaPage_.handlePage();
        });

    server_.on(
        "/ota",
        HTTP_POST,
        [this]()
        {
            otaPage_.handleUploadComplete();
        },
        [this]()
        {
            otaPage_.handleUpload();
        });

    //
    // Settings
    //
    server_.on(
        "/settings",
        HTTP_GET,
        [this]()
        {
            settingsPage_.handleLanding();
        });

    //
    // Alert Settings
    //
    server_.on(
        "/settings/alerts",
        HTTP_GET,
        [this]()
        {
            settingsPage_.handleAlerts();
        });

    server_.on(
        "/settings/alerts",
        HTTP_POST,
        [this]()
        {
            settingsPage_.handleAlertsSave();
        });

    //
    // Reporting Settings
    //
    server_.on(
        "/settings/reporting",
        HTTP_GET,
        [this]()
        {
            settingsPage_.handleReporting();
        });

    server_.on(
        "/settings/reporting",
        HTTP_POST,
        [this]()
        {
            settingsPage_.handleReportingSave();
        });

    //
    // Device Settings
    //
    server_.on(
        "/settings/device",
        HTTP_GET,
        [this]()
        {
            settingsPage_.handleDevice();
        });

    server_.on(
        "/settings/device",
        HTTP_POST,
        [this]()
        {
            settingsPage_.handleDeviceSave();
        });

    //
    // Connectivity Settings
    //
    server_.on(
    "/settings/connectivity",
    HTTP_GET,
    [this]()
    {
        settingsPage_.handleConnectivity();
    });

    server_.on(
    "/settings/connectivity",
    HTTP_POST,
    [this]()
    {
        settingsPage_.handleConnectivitySave();
    });

    //
    // 404 handler must be registered last.
    //
    server_.onNotFound(
        [this]()
        {
            handleNotFound();
        });
    
    server_.on(
    "/settings/connectivity/mqtt",
    HTTP_POST,
    [this]()
    {
        settingsPage_.handleMqttSave();
    });   
    
    server_.on(
    "/diagnostics",
    HTTP_GET,
    [this]()
    {
        diagnosticsPage_.handle();
    });
    
    server_.on(
    "/restart",
    HTTP_POST,
    [this]()
    {
        server_.send(
            200,
            "text/html; charset=utf-8",
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<meta http-equiv=\"refresh\" content=\"8;url=/\">"
            "<title>Restarting</title>"
            "</head>"
            "<body>"
            "<h2>Camper Sentinel is restarting...</h2>"
            "<p>Please wait while the device reconnects.</p>"
            "</body>"
            "</html>");

        delay(500);

        ESP.restart();
    });
}

void WebServerService::handleNotFound()
{
    String message = "Not Found\n\n";
    message += "URI: ";
    message += server_.uri();
    message += "\nMethod: ";
    message += server_.method() == HTTP_GET ? "GET" : "OTHER";
    message += "\n";

    server_.send(
        404,
        "text/plain; charset=utf-8",
        message);
}





