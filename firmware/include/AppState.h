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
    bool accessPointActive = false;
    uint32_t disconnectCount = 0;
    uint32_t lastDisconnectReason = 0;
    uint32_t driverRecoveryCount = 0;
    uint32_t networkFailureCount = 0;
    bool recoveryPending = false;
    String accessPointSSID;
    String accessPointIP;

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
    String transport = "None";

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
    struct LithiumBattery
    {
        String address;
        String model;
        String error = "Not configured";
        bool valid = false;
        bool temperatureValid = false;
        float voltage = 0, current = 0, remainingAh = 0, capacityAh = 0;
        float temperatureC = 0;
        uint32_t updatedMs = 0;
    };
    LithiumBattery lithium[2];
    TemperatureState temperature;
    PowerState power;

    WiFiState wifi;
    CellularState cellular;
    MQTTState mqtt;

    AlarmState alarms;
    SmsState sms;
    SystemState system;
};
