#pragma once
#include "RenogyProtocol.h"
#include <string.h>
#include <math.h>

// Hardware-independent packet checks also run before enabling the BLE task.
inline bool renogyProtocolChecks() {
    using namespace RenogyProtocol;
    const uint8_t expected[] = {0xff, 3, 1, 0, 0, 34, 0xd1, 0xf1};
    uint8_t query[8]; request(255, 256, 34, query);
    if (memcmp(query, expected, 8)) return false;
    uint8_t fixture[] = {0xff, 3, 12, 0xfb, 0x2e, 0, 0x80, 0, 1, 0x86, 0xa0,
        0, 3, 0x0d, 0x40, 0xcf, 0x1b};
    Reading reading{};
    if (!decode(fixture, sizeof(fixture), 255, reading) ||
        fabsf(reading.current + 12.34f) > 0.001f ||
        fabsf(reading.voltage - 12.8f) > 0.001f ||
        fabsf(reading.remainingAh - 100.0f) > 0.001f ||
        fabsf(reading.capacityAh - 200.0f) > 0.001f) return false;
    for (size_t length = 0; length < sizeof(fixture); ++length)
        if (decode(fixture, length, 255, reading)) return false;
    if (decode(fixture, sizeof(fixture), 1, reading)) return false;
    fixture[4] ^= 1;
    if (decode(fixture, sizeof(fixture), 255, reading)) return false;
    fixture[4] ^= 1;
    // Reject zero capacity even when the packet checksum is correct.
    memset(fixture + 11, 0, 4);
    uint16_t check = crc(fixture, 15); fixture[15] = check; fixture[16] = check >> 8;
    if (decode(fixture, sizeof(fixture), 255, reading)) return false;
    return true;
}
