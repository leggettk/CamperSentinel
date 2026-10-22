#pragma once
#include <stdint.h>

constexpr uint32_t WIFI_DRIVER_RECOVERY_MS = 300000UL;
constexpr bool wifiDriverRecoveryDue(bool enabled, bool configured, bool associated,
                                     bool networkRecoveryRequested, uint32_t offlineElapsed,
                                     uint32_t sinceRecovery, uint32_t accessPointClients) {
    return enabled && configured && accessPointClients == 0 && sinceRecovery >= WIFI_DRIVER_RECOVERY_MS &&
        (networkRecoveryRequested || (!associated && offlineElapsed >= WIFI_DRIVER_RECOVERY_MS));
}
static_assert(wifiDriverRecoveryDue(true, true, true, true, 0, 300000, 0), "Recover a stalled associated station");
static_assert(!wifiDriverRecoveryDue(true, true, true, false, 600000, 600000, 0), "Do not restart healthy WiFi");
static_assert(!wifiDriverRecoveryDue(true, true, true, true, 0, 299999, 0), "Rate-limit driver restarts");
static_assert(!wifiDriverRecoveryDue(true, true, true, true, 0, 600000, 1), "Preserve active setup hotspot clients");
static_assert(!wifiDriverRecoveryDue(false, true, true, true, 0, 600000, 0), "Respect disabled WiFi");
static_assert(!wifiDriverRecoveryDue(true, false, false, true, 600000, 600000, 0), "Require configured credentials");
static_assert(wifiDriverRecoveryDue(true, true, false, false, 300000, 300000, 0), "Retain offline recovery");
static_assert(!wifiDriverRecoveryDue(true, true, false, false, 299999, 600000, 0), "Allow ordinary joins first");
