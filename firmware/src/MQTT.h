#pragma once
#include "AppState.h"
#include "settings.h"
class MQTT
{
public:
    bool begin(
        Settings& settings,
        AppState& state);
    void loop();

    bool isConnected() const;
    bool publishAmbient(float temperatureF);
    bool publishBattery(float voltage);
    bool publishShorePower(bool present);
    bool publishHeartbeat(const AppState &state);
    bool publishLithiumBatteries(const AppState& state);

private:
    AppState* state_ = nullptr;
    Settings* settings_ = nullptr;
    bool connect();
    bool preferWifi() const;
    uint8_t wifiFailures_ = 0;
    bool wifiCoolingDown_ = false;
    uint32_t wifiFailedAt_ = 0;
    bool publishDiscovery();
    bool discoveryPublished = false;
    bool lithiumDiscoveryPublished_ = false;
    String lithiumDiscoveryAddresses_[2];
    unsigned long lastReconnectAttemptMs = 0;
    bool publishAmbientDiscovery();
    bool publishBatteryDiscovery();
    bool publishShorePowerDiscovery();
    bool publishSignalDiscovery();
    bool publishUptimeDiscovery();
    

    static constexpr unsigned long RECONNECT_INTERVAL_MS = 30000UL;
};

extern MQTT mqtt;
