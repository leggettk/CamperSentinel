#include "Web/SettingsPage.h"

#include "Version.h"

void SettingsPage::begin(
    WebServer& server,
    Settings& settings,
    AppState& state,
    Logger& logger)
{
    server_ = &server;
    settings_ = &settings;
    state_ = &state;
    logger_ = &logger;
}
String SettingsPage::formatUptime(
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

    if (days > 0)
    {
        snprintf(
            buffer,
            sizeof(buffer),
            "%lu d %02u:%02u:%02u",
            static_cast<unsigned long>(days),
            hours,
            minutes,
            seconds);
    }
    else
    {
        snprintf(
            buffer,
            sizeof(buffer),
            "%02u:%02u:%02u",
            hours,
            minutes,
            seconds);
    }

    return String(buffer);
}
void SettingsPage::handleLanding()
{
    if (server_ == nullptr ||
        settings_ == nullptr ||
        state_ == nullptr)
    {
        return;
    }

    WebPageBuilder page(*server_);

    page.begin(
        FW_NAME,
        "Settings",
        "Configure Camper Sentinel.",
        0);

    page.addNavigation("settings");

    page.addHealthBanner(
        "Camper Sentinel settings",
        "Changes are stored on the device and remain available after a restart.",
        StatusLevel::Neutral);

    page.addSettingsCard(
        "Alerts",
        "Temperature thresholds, shore-power alerts, and SMS notifications.",
        "/settings/alerts");

    page.addSettingsCard(
        "Reporting",
        "Control telemetry publishing and heartbeat timing.",
        "/settings/reporting");

    page.addSettingsCard(
        "Device",
        "Change the device name, hostname, and general behavior.",
        "/settings/device");

    page.addSettingsCard(
        "Connectivity",
        "Manage Wi-Fi, MQTT, and cellular preferences.",
        "/settings/connectivity");

    page.addSettingsCard(
        "Firmware",
        "View firmware details and install an OTA update.",
        "/ota");
    page.addSettingsCard("Bluetooth Batteries", "Discover and assign two Renogy Pro batteries.", "/settings/batteries");

    page.addFooter(
        FW_VERSION,
        formatUptime(
            state_->system.uptimeSeconds));

    page.send(*server_);
}
void SettingsPage::handleAlerts()
{
    if (settings_ == nullptr)
    {
        server_->send(
            500,
            "text/plain; charset=utf-8",
            "Settings service is unavailable.");

        return;
    }

    const DeviceSettings& config =
        settings_->get();

    WebPageBuilder page(*server_);

    page.begin(
        FW_NAME,
        "Alert Settings",
        "Configure temperature and notification behavior.");

    page.addNavigation("settings");

    if (server_->hasArg("saved"))
    {
        page.addToast(
            "Alert settings saved successfully.",
            StatusLevel::Normal);
    }

    page.beginCard("Temperature Alerts");

    page.beginForm(
        "/settings/alerts",
        "POST");

    page.addNumberInput(
    "High temperature alarm",
    "tempHigh",
    config.tempAlarmHighF,
    40.0f,
    150.0f,
    0.5f,
    "°F",
    "An alert is triggered after the configured delay when the camper reaches this temperature.");

    page.addNumberInput(
    "Temperature reset point",
    "tempReset",
    config.tempAlarmResetF,
    35.0f,
    145.0f,
    0.5f,
    "°F",
    "The alarm clears only after the temperature falls to or below this value.");

    page.addCheckbox(
        "SMS alerts",
        "smsEnabled",
        config.smsEnabled,
        "Send text messages for high temperature and shore-power events.");

    page.addSubmitButton("Save Settings");

    page.endForm();
    page.addSettingsFormScript();
    page.endCard();

    page.addLink(
        "Back to Settings",
        "/settings");

    page.addFooter(
        FW_VERSION,
        formatUptime(
            state_ != nullptr
                ? state_->system.uptimeSeconds
                : 0));

    page.send(*server_);
}
void SettingsPage::handleAlertsSave()
{
    if (settings_ == nullptr)
    {
        server_->send(
            500,
            "text/plain; charset=utf-8",
            "Settings service is unavailable.");

        return;
    }

    if (!server_->hasArg("tempHigh") ||
        !server_->hasArg("tempReset"))
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Required settings are missing.");

        return;
    }

    const float highTemperature =
        server_->arg("tempHigh").toFloat();

    const float resetTemperature =
        server_->arg("tempReset").toFloat();

    if (highTemperature < 40.0f ||
        highTemperature > 150.0f)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "High-temperature threshold must be between 40 and 150 F.");

        return;
    }

    if (resetTemperature < 35.0f ||
        resetTemperature >= highTemperature)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Reset temperature must be at least 35 F and below the alarm threshold.");

        return;
    }

    DeviceSettings& config =
        settings_->get();

    config.tempAlarmHighF =
        highTemperature;

    config.tempAlarmResetF =
        resetTemperature;

    config.smsEnabled =
        server_->hasArg("smsEnabled");

    if (!settings_->save())
    {
        logger_->error(
            "Failed to save alert settings");

        server_->send(
            500,
            "text/plain; charset=utf-8",
            "Failed to save settings.");

        return;
    }

    logger_->info(
        String("Alert settings saved: high=") +
        String(config.tempAlarmHighF, 1) +
        " F, reset=" +
        String(config.tempAlarmResetF, 1) +
        " F, SMS=" +
        (config.smsEnabled ? "enabled" : "disabled"));

    server_->sendHeader(
        "Location",
        "/settings/alerts?saved=1");

    server_->send(
        303,
        "text/plain",
        "");
}
void SettingsPage::handleReporting()
{
    if (server_ == nullptr ||
        settings_ == nullptr ||
        state_ == nullptr)
    {
        return;
    }

    const DeviceSettings& config =
        settings_->get();

    WebPageBuilder page(*server_);

    page.begin(
        FW_NAME,
        "Reporting Settings",
        "Configure telemetry and heartbeat timing.");

    page.addNavigation("settings");

    if (server_->hasArg("saved"))
    {
        page.addToast(
            "Reporting settings saved successfully.",
            StatusLevel::Normal);
    }

    page.beginCard("Telemetry");

    page.beginForm(
        "/settings/reporting",
        "POST");

    page.addNumberInput(
        "Temperature publish interval",
        "tempPublishSeconds",
        config.temperaturePublishIntervalSeconds,
        30.0f,
        3600.0f,
        30.0f,
        "sec",
        "How often ambient temperature is published to MQTT.");

    page.addNumberInput(
        "Heartbeat interval",
        "heartbeatSeconds",
        config.heartbeatIntervalSeconds,
        60.0f,
        86400.0f,
        60.0f,
        "sec",
        "How often Camper Sentinel publishes its heartbeat and system status.");

    page.addSubmitButton(
        "Save Settings");

    page.endForm();

    page.addSettingsFormScript();

    page.endCard();

    page.addLink(
        "Back to Settings",
        "/settings");

    page.addFooter(
        FW_VERSION,
        formatUptime(
            state_->system.uptimeSeconds));

    page.send(*server_);
}
void SettingsPage::handleReportingSave()
{
    if (server_ == nullptr ||
        settings_ == nullptr)
    {
        return;
    }

    if (!server_->hasArg("tempPublishSeconds") ||
        !server_->hasArg("heartbeatSeconds"))
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Required settings are missing.");

        return;
    }

    const uint32_t tempPublishSeconds =
        server_->arg("tempPublishSeconds").toInt();

    const uint32_t heartbeatSeconds =
        server_->arg("heartbeatSeconds").toInt();

    if (tempPublishSeconds < 30 ||
        tempPublishSeconds > 3600)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Temperature publish interval must be between 30 and 3600 seconds.");

        return;
    }

    if (heartbeatSeconds < 60 ||
        heartbeatSeconds > 86400)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Heartbeat interval must be between 60 and 86400 seconds.");

        return;
    }

    DeviceSettings& config =
        settings_->get();

    config.temperaturePublishIntervalSeconds =
        tempPublishSeconds;

    config.heartbeatIntervalSeconds =
        heartbeatSeconds;

    if (!settings_->save())
    {
        if (logger_ != nullptr)
        {
            logger_->error(
                "Failed to save reporting settings");
        }

        server_->send(
            500,
            "text/plain; charset=utf-8",
            "Failed to save reporting settings.");

        return;
    }

    if (logger_ != nullptr)
    {
        logger_->info(
            String("Reporting settings saved: temp publish=") +
            String(config.temperaturePublishIntervalSeconds) +
            " sec, heartbeat=" +
            String(config.heartbeatIntervalSeconds) +
            " sec");
    }

    server_->sendHeader(
        "Location",
        "/settings/reporting?saved=1");

    server_->send(
        303,
        "text/plain",
        "");
}
void SettingsPage::handleDevice()
{
    DeviceSettings& config =
        settings_->get();

    WebPageBuilder page(*server_);
    
    page.begin(
        FW_NAME,
        "Device Settings",
        "Identity and general configuration.");

    page.addNavigation("settings");

    if (server_->hasArg("saved"))
{
    if (server_->hasArg("restart"))
    {
        page.addToast(
            "Settings saved. Restart Camper Sentinel to apply the new hostname.",
            StatusLevel::Warning);
    }
    else
    {
        page.addToast(
            "Device settings saved successfully.",
            StatusLevel::Normal);
    }
}

    page.beginCard("Device");

    page.beginForm(
    "/settings/device",
    "POST");

    
    
page.addTextInput(
    "Device Name",
    "deviceName",
    config.deviceName,
    32,
    "Friendly name shown throughout Camper Sentinel.");

page.addTextInput(
    "Hostname",
    "hostname",
    config.hostname,
    32,
    "Local network hostname. Changing this requires a restart.");

page.addCheckbox(
    "Enable Wi-Fi",
    "wifiEnabled",
    config.wifiEnabled,
    "Enable Wi-Fi access, the local dashboard, and OTA updates.");

    page.addSubmitButton(
    "Save Settings");

page.endForm();
page.endCard();
    page.addLink(
        "Back to Settings",
        "/settings");

    page.send(*server_);
}
void SettingsPage::handleDeviceSave()
{
    if (settings_ == nullptr)
    {
        server_->send(
            500,
            "text/plain; charset=utf-8",
            "Settings service is unavailable.");

        return;
    }

    if (!server_->hasArg("deviceName") ||
        !server_->hasArg("hostname"))
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Required settings are missing.");

        return;
    }

    String deviceName =
        server_->arg("deviceName");

    String hostname =
        server_->arg("hostname");

    deviceName.trim();
    hostname.trim();

    //
    // Validate device name
    //
    if (deviceName.length() == 0 ||
        deviceName.length() > 32)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Device name must be between 1 and 32 characters.");

        return;
    }

    //
    // Normalize hostname
    //
    hostname.toLowerCase();
    hostname.replace(" ", "-");

    String cleanHostname;

    for (size_t i = 0;
         i < hostname.length();
         ++i)
    {
        const char c = hostname[i];

        if (
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-')
        {
            cleanHostname += c;
        }
    }

    hostname = cleanHostname;

    if (hostname.length() == 0 ||
        hostname.length() > 32)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Hostname must contain between 1 and 32 valid characters.");

        return;
    }

    //
    // Avoid leading/trailing hyphens
    //
    if (hostname.startsWith("-") ||
        hostname.endsWith("-"))
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Hostname cannot begin or end with a hyphen.");

        return;
    }

    DeviceSettings& config =
        settings_->get();

    const bool hostnameChanged =
        hostname != config.hostname;

    config.deviceName =
        deviceName;

    config.hostname =
        hostname;

    config.wifiEnabled =
        server_->hasArg("wifiEnabled");

    if (!settings_->save())
    {
        logger_->error(
            "Failed to save device settings");

        server_->send(
            500,
            "text/plain; charset=utf-8",
            "Failed to save device settings.");

        return;
    }

    logger_->info(
        String("Device settings saved: name=") +
        config.deviceName +
        ", hostname=" +
        config.hostname +
        ", WiFi=" +
        (config.wifiEnabled
            ? "enabled"
            : "disabled"));

    //
    // Hostname changes require restart.
    //
    if (hostnameChanged)
    {
        server_->sendHeader(
            "Location",
            "/settings/device?saved=1&restart=1");
    }
    else
    {
        server_->sendHeader(
            "Location",
            "/settings/device?saved=1");
    }

    server_->send(
        303,
        "text/plain",
        "");
}

void SettingsPage::handleConnectivity()
{
    if (server_ == nullptr ||
        settings_ == nullptr ||
        state_ == nullptr)
    {
        return;
    }

    const DeviceSettings& config =
        settings_->get();

    WebPageBuilder page(*server_);

    page.begin(
        FW_NAME,
        "Connectivity Settings",
        "Configure Wi-Fi and view network status.");

    page.addNavigation("settings");

    if (server_->hasArg("saved"))
    {
        page.addToast("Wi-Fi settings saved. Connecting automatically; the setup hotspot remains available until restart.");
    }
    const bool restartRequired = server_->hasArg("mqttSaved");

if (restartRequired)
{
    page.beginCard("Apply Changes");

    page.addHealthBanner(
        "Restart required",
        "Restart Camper Sentinel to apply the new connectivity settings.",
        StatusLevel::Warning);

    page.addActionButton(
        "Restart Now",
        "/restart");

    page.endCard();
}
    if (server_->hasArg("mqttSaved"))
{
    page.addToast(
        "MQTT settings saved. Restart Camper Sentinel to apply the changes.",
        StatusLevel::Warning);
}
    page.beginCard("Current Status");
    if (state_->wifi.accessPointActive)
    {
        page.addValue("Setup Wi-Fi", state_->wifi.accessPointSSID);
        page.addValue("Setup Address", "http://" + state_->wifi.accessPointIP);
        page.addValue("Setup", "Available until restart. Saved Wi-Fi credentials are applied automatically.");
    }

    page.addStatus(
        "Wi-Fi",
        state_->wifi.connected
            ? "Connected"
            : "Disconnected",
        state_->wifi.connected
            ? StatusLevel::Normal
            : StatusLevel::Warning);

    if (state_->wifi.connected)
    {
        page.addValue(
            "SSID",
            state_->wifi.ssid);

        page.addValue(
            "IP Address",
            state_->wifi.ipAddress);

        page.addValue(
            "Signal",
            String(state_->wifi.rssi) + " dBm");
    }

    page.endCard();

    page.beginCard("Wi-Fi Configuration");

    page.beginForm(
        "/settings/connectivity",
        "POST");

    page.addCheckbox(
        "Enable Wi-Fi",
        "wifiEnabled",
        config.wifiEnabled,
        "Enable local dashboard access and OTA updates.");

    page.addTextInput(
        "Wi-Fi SSID",
        "wifiSSID",
        config.wifiSSID,
        32,
        "Network name Camper Sentinel should connect to.");

    page.addPasswordInput(
        "Wi-Fi Password",
        "wifiPassword",
        64,
        "Leave blank to keep the currently saved password.");

    page.addSubmitButton(
        "Save Settings");

    page.endForm();

    page.addSettingsFormScript();

    page.endCard();

    page.beginCard("MQTT Configuration");

page.beginForm(
    "/settings/connectivity/mqtt",
    "POST");

page.addCheckbox(
    "Enable MQTT",
    "mqttEnabled",
    config.mqttEnabled,
    "Enable remote telemetry and heartbeat publishing.");

page.addTextInput(
    "Broker",
    "mqttHost",
    config.mqttHost,
    128,
    "Hostname or IP address of the MQTT broker.");

page.addNumberInput(
    "Port",
    "mqttPort",
    config.mqttPort,
    1,
    65535,
    1,
    "",
    "MQTT broker port. TLS commonly uses port 8883.",
    0);

page.addTextInput(
    "Username",
    "mqttUsername",
    config.mqttUsername,
    64,
    "Username used to authenticate with the MQTT broker.");

page.addPasswordInput(
    "Password",
    "mqttPassword",
    128,
    "Leave blank to keep the currently saved password.");

page.addCheckbox(
    "Enable TLS",
    "mqttTlsEnabled",
    config.mqttTlsEnabled,
    "Use an encrypted connection to the MQTT broker.");

page.addSubmitButton(
    "Save MQTT Settings");

page.endForm();

page.addSettingsFormScript();

page.endCard();

    page.addLink(
        "Back to Settings",
        "/settings");

    page.addFooter(
        FW_VERSION,
        formatUptime(
            state_->system.uptimeSeconds));

    page.send(*server_);
}

void SettingsPage::handleConnectivitySave()
{
    if (server_ == nullptr ||
        settings_ == nullptr)
    {
        return;
    }

    if (!server_->hasArg("wifiSSID"))
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Wi-Fi SSID is missing.");

        return;
    }

    String ssid =
        server_->arg("wifiSSID");

    String password =
        server_->arg("wifiPassword");

    ssid.trim();

    if (ssid.length() > 32)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Wi-Fi SSID must be 32 characters or fewer.");

        return;
    }

    if (password.length() > 64)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Wi-Fi password is too long.");

        return;
    }

    DeviceSettings& config =
        settings_->get();

    config.wifiEnabled =
        server_->hasArg("wifiEnabled");

    config.wifiSSID =
        ssid;

    // Blank means keep the existing password.
    if (password.length() > 0)
    {
        config.wifiPassword =
            password;
    }

    if (!settings_->save())
    {
        if (logger_ != nullptr)
        {
            logger_->error(
                "Failed to save connectivity settings");
        }

        server_->send(
            500,
            "text/plain; charset=utf-8",
            "Failed to save connectivity settings.");

        return;
    }

    if (logger_ != nullptr)
    {
        logger_->info(
            String("Wi-Fi settings saved: SSID=") +
            config.wifiSSID +
            ", WiFi=" +
            (config.wifiEnabled
                ? "enabled"
                : "disabled"));
    }

    server_->sendHeader(
        "Location",
        "/settings/connectivity?saved=1");

    server_->send(
        303,
        "text/plain",
        "");
}
void SettingsPage::handleMqttSave()
{
    if (server_ == nullptr ||
        settings_ == nullptr)
    {
        return;
    }

    if (!server_->hasArg("mqttHost") ||
        !server_->hasArg("mqttPort"))
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "Required MQTT settings are missing.");

        return;
    }

    String host =
        server_->arg("mqttHost");

    String username =
        server_->arg("mqttUsername");

    String password =
        server_->arg("mqttPassword");

    host.trim();
    username.trim();

    const long port =
        server_->arg("mqttPort").toInt();

    if (host.length() > 128)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "MQTT broker hostname is too long.");

        return;
    }

    if (port < 1 ||
        port > 65535)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "MQTT port must be between 1 and 65535.");

        return;
    }

    if (username.length() > 64)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "MQTT username is too long.");

        return;
    }

    if (password.length() > 128)
    {
        server_->send(
            400,
            "text/plain; charset=utf-8",
            "MQTT password is too long.");

        return;
    }

    DeviceSettings& config =
        settings_->get();

    config.mqttEnabled =
        server_->hasArg("mqttEnabled");

    config.mqttHost =
        host;

    config.mqttPort =
        static_cast<uint16_t>(port);

    config.mqttUsername =
        username;

    config.mqttTlsEnabled =
        server_->hasArg("mqttTlsEnabled");

    // Blank means preserve the currently stored password.
    if (!password.isEmpty())
    {
        config.mqttPassword =
            password;
    }

    if (!settings_->save())
    {
        if (logger_ != nullptr)
        {
            logger_->error(
                "Failed to save MQTT settings");
        }

        server_->send(
            500,
            "text/plain; charset=utf-8",
            "Failed to save MQTT settings.");

        return;
    }

    if (logger_ != nullptr)
    {
        logger_->info(
            String("MQTT settings saved: host=") +
            config.mqttHost +
            ", port=" +
            String(config.mqttPort) +
            ", TLS=" +
            (config.mqttTlsEnabled
                ? "enabled"
                : "disabled"));
    }

    server_->sendHeader(
        "Location",
        "/settings/connectivity?mqttSaved=1");

    server_->send(
        303,
        "text/plain",
        "");
}
