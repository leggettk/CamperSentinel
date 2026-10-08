#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "Constants.h"

struct DeviceSettings
{
    static constexpr uint16_t CURRENT_VERSION = 2;

    uint16_t version = CURRENT_VERSION;

    // Device identity
    String deviceName = PROJECT_NAME;
    String hostname = "camper-sentinel";

    // Wi-Fi
    bool wifiEnabled = true;
    String wifiSSID = "";
    String wifiPassword = "";

    // MQTT
    bool mqttEnabled = true;
    String mqttHost = "";
    uint16_t mqttPort = 8883;
    String mqttUsername = "";
    String mqttPassword = "";
    bool mqttTlsEnabled = true;

    // Alerts
    bool smsEnabled = true;
    float tempAlarmHighF = DEFAULT_TEMP_ALARM_HIGH_F;
    float tempAlarmResetF = DEFAULT_TEMP_ALARM_RESET_F;

    // Publish timing, in seconds
    uint32_t heartbeatIntervalSeconds =
        DEFAULT_HEARTBEAT_INTERVAL_SECONDS;

    uint32_t temperaturePublishIntervalSeconds =
        DEFAULT_TEMPERATURE_PUBLISH_SECONDS;
};

class Settings
{
public:
    Settings() = default;
    ~Settings();

    // Opens Preferences and loads saved settings.
    bool begin();

    // Loads settings from nonvolatile storage.
    bool load();

    // Saves the current settings to nonvolatile storage.
    bool save();

    // Restores defaults in memory. Set saveNow=true to persist them.
    bool resetDefaults(bool saveNow = true);

    // Closes the Preferences namespace.
    void end();

    DeviceSettings& get();
    const DeviceSettings& get() const;

    bool isReady() const;

private:
    static constexpr const char* NAMESPACE_NAME = "campercfg";

    Preferences preferences_;
    DeviceSettings data_;
    bool ready_ = false;

    bool validate();
};