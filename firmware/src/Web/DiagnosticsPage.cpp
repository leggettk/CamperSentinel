#include "Web/DiagnosticsPage.h"
#include "Version.h"
#include "EventLog.h"

void DiagnosticsPage::begin(
    WebServer& server,
    AppState& state,
    Settings& settings)
{
    server_ = &server;
    state_ = &state;
    settings_ = &settings;
}
void DiagnosticsPage::handle()
{
    if (server_ == nullptr ||
        state_ == nullptr ||
        settings_ == nullptr)
    {
        return;
    }

    const AppState& state = *state_;
    const DeviceSettings& config =
        settings_->get();

    WebPageBuilder page;

    page.begin(
        FW_NAME,
        "Diagnostics",
        "Detailed Camper Sentinel system information.",
        5);

    page.addNavigation("diagnostics");

    page.beginCard("System");

    page.addValue(
        "Firmware",
        FW_VERSION);

    page.addValue(
        "Build",
        String(__DATE__) + " " + __TIME__);

    page.addValue(
        "Uptime",
        formatUptime(
            state.system.uptimeSeconds));

    page.addValue(
        "Free Heap",
        String(
            state.system.freeHeap / 1024.0f,
            1) + " KB");

    page.addValue(
        "ESP32",
        ESP.getChipModel());

    page.addValue(
        "CPU",
        String(ESP.getCpuFreqMHz()) + " MHz");

    page.endCard();

    page.beginCard("Wi-Fi");

    page.addStatus(
        "Status",
        state.wifi.connected
            ? "Connected"
            : "Disconnected",
        state.wifi.connected
            ? StatusLevel::Normal
            : StatusLevel::Warning);

    page.addValue(
        "SSID",
        state.wifi.ssid);

    page.addValue(
        "IP Address",
        state.wifi.ipAddress);

    page.addValue(
        "RSSI",
        String(state.wifi.rssi) + " dBm");

    page.endCard();

    page.beginCard("Cellular");

    page.addStatus(
        "Network",
        state.cellular.networkConnected
            ? "Connected"
            : "Disconnected",
        state.cellular.networkConnected
            ? StatusLevel::Normal
            : StatusLevel::Warning);

    page.addStatus(
        "Packet Data",
        state.cellular.dataConnected
            ? "Connected"
            : "Disconnected",
        state.cellular.dataConnected
            ? StatusLevel::Normal
            : StatusLevel::Warning);

    page.addValue(
        "Operator",
        state.cellular.operatorName);

    page.addValue(
        "IP Address",
        state.cellular.ipAddress);

    page.addValue(
        "Signal Quality",
        String(state.cellular.signalQuality));

    page.endCard();

    page.beginCard("MQTT");

    page.addStatus(
        "Status",
        state.mqtt.connected
            ? "Connected"
            : "Disconnected",
        state.mqtt.connected
            ? StatusLevel::Normal
            : StatusLevel::Warning);

    page.addValue(
        "Broker",
        config.mqttHost);

    page.addValue(
        "Port",
        String(config.mqttPort));

    page.addValue(
        "TLS",
        config.mqttTlsEnabled
            ? "Enabled"
            : "Disabled");

    page.addValue(
        "Client ID",
        config.hostname + "-mqtt");

    page.addValue(
        "Last Error",
        String(state.mqtt.lastError));

    page.addValue(
        "Connection Failures",
        String(state.mqtt.connectionFailureCount));

    page.addValue(
        "Connected At Uptime",
        state.mqtt.lastConnectedTimeSeconds > 0
            ? formatUptime(
                state.mqtt.lastConnectedTimeSeconds)
            : "Never");

    page.endCard();

    page.beginCard("Recent Events");

const size_t totalEvents =
    eventLog.count();

if (totalEvents == 0)
{
    page.addValue(
        "Status",
        "No events recorded");
}
else
{
    constexpr size_t MAX_DISPLAY_EVENTS = 25;

    const size_t eventsToShow =
        totalEvents < MAX_DISPLAY_EVENTS
            ? totalEvents
            : MAX_DISPLAY_EVENTS;

    for (size_t i = 0;
         i < eventsToShow;
         ++i)
    {
        //
        // EventLog index 0 is oldest.
        // Display newest first.
        //
        const size_t eventIndex =
            totalEvents - 1 - i;

        const EventEntry& entry =
            eventLog.get(eventIndex);

        String label =
            formatUptime(
                entry.uptimeSeconds);

        String value =
            "[" +
            eventLevelName(entry.level) +
            "] " +
            entry.message;

        page.addValue(
            label,
            value);
    }
}

page.endCard();

    server_->send(
        200,
        "text/html; charset=utf-8",
        page.build());
}

String DiagnosticsPage::formatUptime(
    uint32_t totalSeconds) const
{
    const uint32_t days =
        totalSeconds / 86400UL;

    totalSeconds %= 86400UL;

    const uint8_t hours =
        totalSeconds / 3600UL;

    totalSeconds %= 3600UL;

    const uint8_t minutes =
        totalSeconds / 60UL;

    const uint8_t seconds =
        totalSeconds % 60UL;

    char buffer[40];

    snprintf(
        buffer,
        sizeof(buffer),
        "%lu d %02u:%02u:%02u",
        static_cast<unsigned long>(days),
        hours,
        minutes,
        seconds);

    return String(buffer);
}
String DiagnosticsPage::eventLevelName(
    EventLevel level) const
{
    switch (level)
    {
        case EventLevel::Info:
            return "INFO";

        case EventLevel::Warning:
            return "WARN";

        case EventLevel::Error:
            return "ERROR";
    }

    return "UNKNOWN";
}
void DiagnosticsPage::handleClearLog()
{
    if (server_ == nullptr)
    {
        return;
    }

    eventLog.clear();

    //
    // Don't use logger.info() here.
    // That would immediately put an entry
    // back into the log we just cleared.
    //

    server_->sendHeader(
        "Location",
        "/diagnostics?logCleared=1");

    server_->send(
        303,
        "text/plain",
        "");
}