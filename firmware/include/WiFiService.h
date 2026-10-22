#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include "Logger.h"
#include "Settings.h"
#include "AppState.h"

class WiFiService
{
public:
    // Initializes Wi-Fi station mode and begins a connection attempt
    // using the credentials stored in Settings.
    bool begin(Settings& settings, AppState& appState, Logger& logger);

    // Handles non-blocking reconnect attempts.
    void update();

    // Stops Wi-Fi and clears the current connection.
    void disconnect();

    // Starts a new connection attempt using the current settings.
    void reconnect();

    bool connected() const;
    bool configured() const;
    bool enabled() const;

    String ipAddress() const;
    String ssid() const;
    String hostname() const;

    int32_t rssi() const;
    // Called on the main loop after a WiFi broker DNS probe, while cellular
    // MQTT remains live. Repeated failures can request station recovery.
    void noteNetworkFailure();
    void noteNetworkSuccess();

private:

    Logger* logger_ = nullptr;
    Settings* settings_ = nullptr;
    AppState* appState_ = nullptr;
    String attemptedSSID_;
    String attemptedPassword_;
    bool attemptedEnabled_ = false;
    bool accessPointActive_ = false;
    uint32_t offlineSinceMs_ = 0;
    static constexpr uint32_t ACCESS_POINT_DELAY_MS = 30000UL;
    uint32_t lastDriverRecoveryMs_ = 0;
    uint8_t networkFailures_ = 0;
    bool networkRecoveryRequested_ = false;
    std::atomic<uint32_t> disconnectCount_{0};
    std::atomic<uint32_t> disconnectReason_{0};
    bool wasConnected_ = false;
    bool started_ = false;
    bool connectionAttempted_ = false;

    uint32_t lastReconnectAttemptMs_ = 0;

    void connect();
    void startAccessPoint();
    void restartDriver(const char* reason);
};
extern WiFiService wifi;
