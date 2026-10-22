#include "WiFiService.h"
#include "WiFiRecoveryPolicy.h"

bool WiFiService::begin(Settings& settings, AppState& appState, Logger& logger)
{
    settings_ = &settings;
    logger_ = &logger;
    appState_ = &appState;

    WiFi.onEvent([this](WiFiEvent_t, WiFiEventInfo_t info) {
        disconnectReason_.store(info.wifi_sta_disconnected.reason);
        disconnectCount_.fetch_add(1);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    lastDriverRecoveryMs_ = millis();

    started_ = true;
    offlineSinceMs_ = millis();

    connect();
    if (!enabled() || !configured()) startAccessPoint();

    return true;
}
void WiFiService::update()
{
    if (!started_)
    {
        return;
    }

    const DeviceSettings& config = settings_->get();
    const uint32_t disconnects = disconnectCount_.load();
    if (disconnects != appState_->wifi.disconnectCount)
    {
        appState_->wifi.disconnectCount = disconnects;
        appState_->wifi.lastDisconnectReason = disconnectReason_.load();
        logger_->warningf("WiFi disconnect count=%u, last reason=%u, heap=%u",
            disconnects, appState_->wifi.lastDisconnectReason, ESP.getFreeHeap());
    }
    if (config.wifiSSID != attemptedSSID_ ||
        config.wifiPassword != attemptedPassword_ ||
        config.wifiEnabled != attemptedEnabled_)
    {
        // Settings are saved by the web server; apply them on the next loop.
        WiFi.disconnect();
        offlineSinceMs_ = millis();
        connect();
    }

    const bool isConnected = connected();

    if (isConnected && !wasConnected_)
    {
        if (logger_ != nullptr)
        {
            logger_->info("WiFi connected");

            logger_->info(
                String("WiFi SSID: ") +
                ssid());

            logger_->info(
                String("WiFi IP: ") +
                ipAddress());

            logger_->info(
                String("WiFi RSSI: ") +
                String(rssi()) +
                " dBm");
        }
        appState_->wifi.connected = true;
        appState_->wifi.ssid = WiFi.SSID();
        appState_->wifi.ipAddress =
        WiFi.localIP().toString();
        appState_->wifi.rssi = WiFi.RSSI();
    }
    else if (!isConnected && wasConnected_)
    {
        offlineSinceMs_ = millis();
        if (logger_ != nullptr)
        {
            logger_->warning("WiFi disconnected");
        }
        appState_->wifi.connected = false;
        appState_->wifi.ipAddress = "No IP";
        appState_->wifi.rssi = 0;
    }

    wasConnected_ = isConnected;

    // A station can remain WL_CONNECTED while DNS/TCP traffic has stalled.
    // Evaluate the requested recovery before the connected fast path returns.
    const uint32_t now = millis();
    const bool recoveryCandidate = enabled() && configured() && (networkRecoveryRequested_ || !isConnected) &&
        now - lastDriverRecoveryMs_ >= WIFI_DRIVER_RECOVERY_MS;
    if (recoveryCandidate && wifiDriverRecoveryDue(enabled(), configured(), isConnected, networkRecoveryRequested_,
        now - offlineSinceMs_, now - lastDriverRecoveryMs_, accessPointActive_ ? WiFi.softAPgetStationNum() : 0))
    {
        restartDriver(networkRecoveryRequested_ ? "repeated network failures while associated"
            : "offline for five minutes");
        return;
    }

    if (isConnected)
    {
        if (appState_ != nullptr)
        {
            appState_->wifi.connected = true;
            appState_->wifi.ssid = WiFi.SSID();
            appState_->wifi.ipAddress = WiFi.localIP().toString();
            appState_->wifi.rssi = WiFi.RSSI();
        }
        return;
    }

    if (!enabled() || !configured() ||
        now - offlineSinceMs_ >= ACCESS_POINT_DELAY_MS)
    {
        startAccessPoint();
    }
    if ((now - lastReconnectAttemptMs_) <
        WIFI_RECONNECT_INTERVAL_MS)
    {
        return;
    }

    lastReconnectAttemptMs_ = now;
    connect();
}

void WiFiService::noteNetworkFailure()
{
    if (!started_ || !enabled() || !configured() || !connected()) return;
    if (networkFailures_ < 255) ++networkFailures_;
    appState_->wifi.networkFailureCount = networkFailures_;
    if (networkFailures_ >= 2) networkRecoveryRequested_ = true;
    appState_->wifi.recoveryPending = networkRecoveryRequested_;
    logger_->warningf("WiFi DNS probe failures while cellular MQTT is live: %u; recovery pending=%d",
        unsigned(networkFailures_), networkRecoveryRequested_);
}
void WiFiService::noteNetworkSuccess()
{
    networkFailures_ = 0;
    networkRecoveryRequested_ = false;
    if (appState_) {
        appState_->wifi.networkFailureCount = 0;
        appState_->wifi.recoveryPending = false;
    }
}
void WiFiService::restartDriver(const char* reason)
{
    lastDriverRecoveryMs_ = millis();
    const bool restoreAccessPoint = accessPointActive_;
    ++appState_->wifi.driverRecoveryCount;
    logger_->warningf("WiFi recovery: %s; restarting driver; status=%d ip=%s gateway=%s dns=%s RSSI=%d",
        reason, int(WiFi.status()), WiFi.localIP().toString().c_str(),
        WiFi.gatewayIP().toString().c_str(), WiFi.dnsIP().toString().c_str(), int(WiFi.RSSI()));
    WiFi.disconnect(false, false); // Keep saved credentials.
    if (!WiFi.mode(WIFI_OFF)) {
        logger_->error("WiFi driver stop failed; recovery rate-limited before retry");
        return;
    }
    accessPointActive_ = false;
    appState_->wifi.accessPointActive = false;
    appState_->wifi.connected = false;
    appState_->wifi.ipAddress = "No IP";
    appState_->wifi.rssi = 0;
    wasConnected_ = false;
    offlineSinceMs_ = millis();
    if (!WiFi.mode(WIFI_STA)) {
        logger_->error("WiFi driver restart failed; will retry");
        return;
    }
    WiFi.setAutoReconnect(true);
    if (restoreAccessPoint) startAccessPoint();
    connect();
}

void WiFiService::connect()
{
    if (settings_ == nullptr)
    {
        return;
    }

    const DeviceSettings& config = settings_->get();
    noteNetworkSuccess();
    attemptedSSID_ = config.wifiSSID;
    attemptedPassword_ = config.wifiPassword;
    attemptedEnabled_ = config.wifiEnabled;
    if (!enabled() || !configured())
    {
        return;
    }

    WiFi.setHostname(config.hostname.c_str());

    if (logger_ != nullptr)
    {
        logger_->info(
            String("WiFi: Connecting to ") +
            config.wifiSSID);
    }

    WiFi.begin(
        config.wifiSSID.c_str(),
        config.wifiPassword.c_str()
    );

    connectionAttempted_ = true;
    lastReconnectAttemptMs_ = millis();
}

void WiFiService::startAccessPoint()
{
    if (accessPointActive_) return;

    String suffix = WiFi.macAddress();
    suffix.replace(":", "");
    suffix = suffix.substring(suffix.length() - 6);
    const String apSSID = "CamperSentinel-" + suffix;
    const String apPassword = "Camper-" + suffix;

    WiFi.mode(WIFI_AP_STA);
    const IPAddress address(192, 168, 4, 1);
    if (!WiFi.softAPConfig(address, address, IPAddress(255, 255, 255, 0)) ||
        !WiFi.softAP(apSSID.c_str(), apPassword.c_str()))
    {
        logger_->error("Setup access point failed to start");
        return;
    }

    accessPointActive_ = true;
    appState_->wifi.accessPointActive = true;
    appState_->wifi.accessPointSSID = apSSID;
    appState_->wifi.accessPointIP = WiFi.softAPIP().toString();
    logger_->info(String("Setup WiFi: ") + apSSID +
        "; password: " + apPassword + "; settings: http://" +
        appState_->wifi.accessPointIP + "/settings/connectivity");
}
bool WiFiService::connected() const
{
    return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress();
}

bool WiFiService::configured() const
{
    if (settings_ == nullptr)
    {
        return false;
    }

    const DeviceSettings& config = settings_->get();

    return !config.wifiSSID.isEmpty();
}

bool WiFiService::enabled() const
{
    if (settings_ == nullptr)
    {
        return false;
    }

    return settings_->get().wifiEnabled;
}
String WiFiService::ssid() const
{
    if (!connected())
    {
        return "";
    }

    return WiFi.SSID();
}

String WiFiService::ipAddress() const
{
    if (!connected())
    {
        return "";
    }

    return WiFi.localIP().toString();
}

int32_t WiFiService::rssi() const
{
    if (!connected())
    {
        return 0;
    }

    return WiFi.RSSI();
}
