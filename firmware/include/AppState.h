#pragma once

#include <Arduino.h>

struct TemperatureState
{
    float ambientF = 0.0f;
    bool valid = false;
};

struct PowerState
{
    float batteryVoltage = 0.0f;
    bool batteryVoltageValid = false;

    bool shorePowerPresent = false;
};

struct WiFiState
{
    bool connected = false;

    String ssid;
    String ipAddress = "No IP";

    int32_t rssi = 0;
};

struct CellularState
{
    bool networkConnected = false;
    bool dataConnected = false;

    int signalQuality = 99;

    String operatorName = "Unknown";
    String ipAddress = "No IP";
};

struct MQTTState
{
    bool connected = false;

    int lastError = 0;

    uint32_t connectionFailureCount = 0;
    uint32_t lastConnectedTimeSeconds = 0;
};

struct AlarmState
{
    bool highTemperature = false;
    bool shorePower = false;
};

struct SmsState
{
    bool lastSuccessful = false;
    uint32_t lastSentTimeSeconds = 0;
};

struct SystemState
{
    uint32_t uptimeSeconds = 0;
    uint32_t freeHeap = 0;
};

struct AppState
{
    TemperatureState temperature;
    PowerState power;

    WiFiState wifi;
    CellularState cellular;
    MQTTState mqtt;

    AlarmState alarms;
    SmsState sms;
    SystemState system;
};