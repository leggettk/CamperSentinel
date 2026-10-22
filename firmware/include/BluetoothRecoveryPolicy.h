#pragma once
#include <stdint.h>

// Inputs must be internal 8-bit-capable heap, not the general ESP heap.
constexpr bool bluetoothConnectionMemoryReady(uint32_t freeBytes, uint32_t largestBlock) {
    return freeBytes >= 20000 && largestBlock >= 6144;
}
static_assert(!bluetoothConnectionMemoryReady(19999, 16372), "Protect network memory despite a large block");
static_assert(!bluetoothConnectionMemoryReady(30000, 6143), "Fragmentation must defer a connection");
static_assert(bluetoothConnectionMemoryReady(20000, 6144), "Memory gate boundary");

constexpr bool bluetoothOperationMemoryReady(uint32_t freeBytes, uint32_t largestBlock) {
    return freeBytes >= 8192 && largestBlock >= 2048;
}
static_assert(!bluetoothOperationMemoryReady(3396, 1652), "Abort the observed in-flight memory shortage");
static_assert(!bluetoothOperationMemoryReady(8191, 8192), "Preserve network headroom during an operation");
static_assert(!bluetoothOperationMemoryReady(10000, 2047), "Detect in-flight fragmentation");
static_assert(bluetoothOperationMemoryReady(8192, 2048), "In-flight memory boundary");

constexpr uint32_t bluetoothRetryDelay(uint8_t failures) {
    return failures <= 1 ? 60000UL : failures == 2 ? 120000UL : failures == 3 ? 240000UL : 300000UL;
}
constexpr bool bluetoothRetryReady(bool enabled, bool cleanupComplete, uint32_t now,
                                   uint32_t stoppedAt, uint8_t failures) {
    return enabled && cleanupComplete && uint32_t(now - stoppedAt) >= bluetoothRetryDelay(failures);
}
// Compile-time regression checks for the safety gate, cooldown, and timer wrap.
static_assert(!bluetoothRetryReady(true, false, 600000, 0, 1), "Never retry without confirmed cleanup");
static_assert(!bluetoothRetryReady(false, true, 600000, 0, 1), "Disabled monitoring must stay disabled");
static_assert(!bluetoothRetryReady(true, true, 59999, 0, 1), "Respect first cooldown");
static_assert(bluetoothRetryReady(true, true, 60000, 0, 1), "Retry after first cooldown");
static_assert(!bluetoothRetryReady(true, true, 119999, 0, 2), "Repeated failures increase cooldown");
static_assert(bluetoothRetryReady(true, true, 120000, 0, 2), "Second cooldown boundary");
static_assert(bluetoothRetryDelay(3) == 240000 && bluetoothRetryDelay(255) == 300000, "Backoff is capped");
static_assert(!bluetoothRetryReady(true, true, 100, UINT32_MAX - 100, 1), "Rollover must not retry early");
static_assert(bluetoothRetryReady(true, true, 59900, UINT32_MAX - 100, 1), "Cooldown survives rollover");

constexpr bool bluetoothNimbleCleanupReady(bool opening, bool connected, bool hostConnecting,
                                          bool scanning, uint32_t now, uint32_t idleAt) {
    return !opening && !connected && !hostConnecting && !scanning && uint32_t(now - idleAt) >= 5000;
}
constexpr bool bluetoothGattCallbackReady(bool active, bool stopped, bool connected,
                                         uint16_t connection, uint16_t expectedConnection,
                                         uint32_t token, uint32_t expectedToken) {
    return active && !stopped && connected && connection == expectedConnection &&
        token != 0 && token == expectedToken;
}
constexpr bool bluetoothDisconnectCompletesAttempt(bool currentAttempt, bool matchingPeer,
                                                   bool opening, bool matchingHandle) {
    return currentAttempt && matchingPeer && (opening || matchingHandle);
}
static_assert(bluetoothDisconnectCompletesAttempt(true, true, true, false), "Failed opening must clear pending without an established handle");
static_assert(bluetoothDisconnectCompletesAttempt(true, true, false, true), "Established disconnect clears link");
static_assert(!bluetoothDisconnectCompletesAttempt(false, true, true, true), "Old attempt must not clear a new opening");
static_assert(!bluetoothDisconnectCompletesAttempt(true, false, true, true), "Another peer must not clear this opening");
static_assert(!bluetoothDisconnectCompletesAttempt(true, true, false, false), "Unrelated handle cannot clear an established link");
constexpr uint16_t bluetoothDescriptorEnd(uint16_t notifier, uint16_t end, uint16_t nextDefinition) {
    return notifier && nextDefinition > notifier && nextDefinition <= end ? nextDefinition - 1 : end;
}
static_assert(bluetoothNimbleCleanupReady(false, false, false, false, 6000, 1000), "Idle cleanup can settle");
static_assert(!bluetoothNimbleCleanupReady(false, false, false, false, 5999, 1000), "Respect settling boundary");
static_assert(!bluetoothNimbleCleanupReady(true, false, false, false, 6000, 1000), "Await open cancellation callback");
static_assert(!bluetoothNimbleCleanupReady(false, true, false, false, 6000, 1000), "Await physical disconnect");
static_assert(!bluetoothNimbleCleanupReady(false, false, true, false, 6000, 1000), "Host must not be connecting");
static_assert(!bluetoothNimbleCleanupReady(false, false, false, true, 6000, 1000), "Scan must be idle");
static_assert(bluetoothNimbleCleanupReady(false, false, false, false, 4900, UINT32_MAX - 100), "Settling survives rollover");
static_assert(bluetoothGattCallbackReady(true, false, true, 1, 1, 42, 42), "Accept current operation");
static_assert(!bluetoothGattCallbackReady(false, false, true, 1, 1, 42, 42), "Ignore completed operation callbacks");
static_assert(!bluetoothGattCallbackReady(true, true, true, 1, 1, 42, 42), "Quarantined links cannot change handles");
static_assert(!bluetoothGattCallbackReady(true, false, false, 1, 1, 42, 42), "Disconnected links cannot change handles");
static_assert(!bluetoothGattCallbackReady(true, false, true, 2, 1, 42, 42), "Reject old connection callback");
static_assert(!bluetoothGattCallbackReady(true, false, true, 1, 1, 41, 42), "Reject stale token after handle reuse");
static_assert(!bluetoothGattCallbackReady(true, false, true, 1, 1, 0, 0), "Zero token is never an operation");
static_assert(bluetoothDescriptorEnd(20, 40, 30) == 29, "Exclude the next characteristic declaration");
static_assert(bluetoothDescriptorEnd(20, 29, 35) == 29, "Never widen a descriptor range");
static_assert(bluetoothDescriptorEnd(0, 0, 30) == 0, "No notifier means no descriptor range");
static_assert(bluetoothDescriptorEnd(20, 40, 20) == 40, "Ignore earlier declaration");
static_assert(bluetoothDescriptorEnd(20, 40, 50) == 40, "Ignore another service's handles");
