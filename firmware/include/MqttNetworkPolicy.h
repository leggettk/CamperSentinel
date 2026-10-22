#pragma once
#include <stdint.h>
constexpr uint32_t MQTT_WIFI_BACKOFF_MS = 300000UL;
constexpr bool preferMqttWifi(bool wifiReady, bool cellularReady, bool coolingDown,
                              uint32_t now, uint32_t failedAt) {
    return wifiReady && (!cellularReady || !coolingDown || uint32_t(now - failedAt) >= MQTT_WIFI_BACKOFF_MS);
}
static_assert(preferMqttWifi(true, true, false, 0, 0), "Normally prefer WiFi");
static_assert(!preferMqttWifi(true, true, true, 299999, 0), "Use cellular during WiFi backoff");
static_assert(preferMqttWifi(true, true, true, 300000, 0), "Probe WiFi after backoff");
static_assert(preferMqttWifi(true, false, true, 1, 0), "Keep trying the only available network");
static_assert(!preferMqttWifi(false, true, false, 0, 0), "Use cellular when WiFi has no IP");
static_assert(!preferMqttWifi(true, true, true, 100, UINT32_MAX - 100), "Handle timer rollover");
