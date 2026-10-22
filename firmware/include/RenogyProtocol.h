#pragma once
#include <stdint.h>
#include <stddef.h>

namespace RenogyProtocol {
inline uint16_t crc(const uint8_t* bytes, size_t length) {
    uint16_t result = 0xffff;
    for (size_t i = 0; i < length; ++i) {
        result ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit)
            result = (result >> 1) ^ ((result & 1) ? 0xa001 : 0);
    }
    return result;
}
inline uint16_t word(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
inline uint32_t dword(const uint8_t* p) { return (uint32_t(word(p)) << 16) | word(p + 2); }
inline void request(uint8_t id, uint16_t reg, uint16_t words, uint8_t* out) {
    out[0] = id; out[1] = 3; out[2] = reg >> 8; out[3] = reg;
    out[4] = words >> 8; out[5] = words;
    uint16_t check = crc(out, 6); out[6] = check; out[7] = check >> 8;
}
inline bool valid(const uint8_t* frame, size_t length, uint8_t id, uint16_t words) {
    return length == size_t(words * 2 + 5) && frame[0] == id && frame[1] == 3 &&
        frame[2] == words * 2 && crc(frame, length - 2) ==
        uint16_t(frame[length - 2] | (uint16_t(frame[length - 1]) << 8));
}
struct Reading { float voltage, current, remainingAh, capacityAh; };
inline bool decode(const uint8_t* frame, size_t length, uint8_t id, Reading& result) {
    if (!valid(frame, length, id, 6)) return false;
    Reading next{word(frame + 5) * 0.1f, int16_t(word(frame + 3)) * 0.01f,
        dword(frame + 7) * 0.001f, dword(frame + 11) * 0.001f};
    if (next.voltage < 5 || next.voltage > 20 || next.capacityAh <= 0 ||
        next.capacityAh > 1000 || next.remainingAh > next.capacityAh * 1.05f) return false;
    result = next;
    return true;
}
}
