#include "WebServerService.h"
#include "WebPageBuilder.h"
#include "RenogyBatteries.h"
#include "WebAssets.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_heap_caps.h>


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
    state_ = &appState;
    
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
    server_.on("/health/batteries", HTTP_GET, [&appState, this]()
    {
        JsonDocument health;
        for (int i = 0; i < 2; ++i)
        {
            const auto& battery = appState.lithium[i];
            JsonObject item = health["batteries"].add<JsonObject>();
            item["address"] = battery.address;
            item["model"] = battery.model;
            item["valid"] = battery.valid;
            item["error"] = battery.error;
        }
        char payload[768];
        const size_t length = serializeJson(health, payload, sizeof(payload));
        if (length >= sizeof(payload)) { server_.send(500, "text/plain", "Battery diagnostics too large"); return; }
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "application/json", payload);
    });
    renogyBatteries.registerRoutes(server_);

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
    server_.on("/assets/style.css", HTTP_GET, [this]()
    {
        server_.sendHeader("Cache-Control", "public, max-age=3600");
        server_.send_P(200, "text/css; charset=utf-8", WEB_STYLES);
    });
    server_.on("/health", HTTP_GET, [this]()
    {
        JsonDocument health;
        health["free_heap"] = ESP.getFreeHeap();
        health["largest_free_block"] = ESP.getMaxAllocHeap();
        health["internal_free_heap"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        health["internal_largest_block"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        health["internal_minimum_heap"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        health["bluetooth_stack"] = "NimBLE-Arduino 2.3.9";
        health["wifi_connected"] = WiFi.status() == WL_CONNECTED;
        health["wifi_ip"] = WiFi.localIP().toString();
        health["wifi_dns"] = WiFi.dnsIP().toString();
        if (state_)
        {
            health["wifi_disconnects"] = state_->wifi.disconnectCount;
            health["wifi_last_disconnect_reason"] = state_->wifi.lastDisconnectReason;
            health["wifi_driver_recoveries"] = state_->wifi.driverRecoveryCount;
            health["wifi_network_failures"] = state_->wifi.networkFailureCount;
            health["wifi_recovery_pending"] = state_->wifi.recoveryPending;
            health["mqtt_connected"] = state_->mqtt.connected;
            health["mqtt_transport"] = state_->mqtt.transport;
            health["mqtt_last_error"] = state_->mqtt.lastError;
        }
        health["uptime_seconds"] = millis() / 1000UL;
        health["reset_reason"] = static_cast<int>(esp_reset_reason());
        health["bluetooth_status"] = renogyBatteries.status();
        char payload[1024];
        const size_t length = serializeJson(health, payload, sizeof(payload));
        if (length >= sizeof(payload)) { server_.send(500, "text/plain", "Health diagnostics too large"); return; }
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "application/json", payload);
    });
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





