#pragma once
#include <stdint.h>
constexpr uint32_t BATTERY_POLL_INTERVAL_MS = 300000UL;
// Keep a sample across one missed five-minute poll, with time for scanning,
// connection attempts and recovery. Failed polls never refresh its timestamp.
constexpr uint32_t BATTERY_MAX_AGE_MS = 720000UL;
constexpr bool batterySampleFresh(bool hasSample, uint32_t now, uint32_t updated) {
    return hasSample && uint32_t(now - updated) < BATTERY_MAX_AGE_MS;
}
static_assert(!batterySampleFresh(false, 100, 0), "An unread battery is unavailable");
static_assert(batterySampleFresh(true, 300000, 0), "Keep readings between five-minute polls");
static_assert(batterySampleFresh(true, 600000, 0), "Allow one missed poll");
static_assert(batterySampleFresh(true, 719999, 0), "Keep recent successful readings");
static_assert(!batterySampleFresh(true, 720000, 0), "Expire stale readings after twelve minutes");
static_assert(batterySampleFresh(true, 100, UINT32_MAX - 100), "Handle millis rollover");
