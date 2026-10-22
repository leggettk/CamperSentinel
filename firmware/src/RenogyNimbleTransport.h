#pragma once
#include <NimBLEDevice.h>
#include <esp_heap_caps.h>
#include <freertos/queue.h>

// Private to RenogyBatteries.cpp. Native asynchronous GATT avoids retaining
// wrapper service trees or passing pointers to expired stack contexts.
namespace {
constexpr uint32_t INTERNAL_CAPS = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t DISCOVERY_TIMEOUT_MS = 15000;
constexpr char WRITE_SERVICE[] = "0000ffd0-0000-1000-8000-00805f9b34fb";
constexpr char WRITE_CHAR[] = "0000ffd1-0000-1000-8000-00805f9b34fb";
constexpr char NOTIFY_CHAR[] = "0000fff1-0000-1000-8000-00805f9b34fb";
const NimBLEUUID writerServiceUuid(WRITE_SERVICE), writerUuid(WRITE_CHAR), notifyUuid(NOTIFY_CHAR);
const NimBLEUUID cccdUuid("00002902-0000-1000-8000-00805f9b34fb");
struct Packet { uint8_t bytes[128]; size_t length = 0; };
struct GattHandles {
    uint16_t connection = BLE_HS_CONN_HANDLE_NONE;
    uint16_t writer = 0, notifier = 0, descriptor = 0, notifierEnd = 0;
    uint8_t properties = 0;
};
struct ServiceRange { uint16_t start, end; bool writerService; };
struct LinkState {
    bool pending = false, connected = false, stopped = false;
    uint16_t connection = BLE_HS_CONN_HANDLE_NONE;
    uint8_t failures = 0, goodCycles = 0;
    uint32_t stoppedAt = 0, idleAt = 0;
    uint32_t attempt = 0;
    ble_addr_t peer{};
};
struct DiscoveryState {
    uint32_t token = 0;
    bool active = false, done = false, writerService = false;
    int status = 0;
    ServiceRange services[16]{};
    uint8_t count = 0;
    uint16_t serviceEnd = 0;
    GattHandles handles;
};
LinkState batteryLink;
DiscoveryState discovery;
portMUX_TYPE handlesMux = portMUX_INITIALIZER_UNLOCKED;
QueueHandle_t notifications;
SemaphoreHandle_t scanFinished;
bool notificationOverflow = false;
uint32_t nextAttempt = 0; // Never reset when replacing a quarantined LinkState.

LinkState linkSnapshot() {
    portENTER_CRITICAL(&handlesMux); const auto copy = batteryLink; portEXIT_CRITICAL(&handlesMux);
    return copy;
}
bool connectionMemoryAvailable() {
    return bluetoothConnectionMemoryReady(heap_caps_get_free_size(INTERNAL_CAPS),
        heap_caps_get_largest_free_block(INTERNAL_CAPS));
}
void stopLink(const char* reason) {
    portENTER_CRITICAL(&handlesMux);
    if (batteryLink.stopped) { portEXIT_CRITICAL(&handlesMux); return; }
    batteryLink.stopped = true; batteryLink.goodCycles = 0;
    batteryLink.stoppedAt = millis();
    if (batteryLink.failures < 4) ++batteryLink.failures;
    discovery.active = false;
    const LinkState state = batteryLink;
    portEXIT_CRITICAL(&handlesMux);
    Serial.printf("Bluetooth NimBLE: %s; cancelling connection, retry in at least %u seconds\n",
        reason, unsigned(bluetoothRetryDelay(state.failures) / 1000));
    if (state.pending) {
        const int rc = ble_gap_conn_cancel();
        Serial.printf("Bluetooth NimBLE cancel: rc=%d, host_connecting=%u\n", rc, unsigned(ble_gap_conn_active()));
    }
    if (state.connected) ble_gap_terminate(state.connection, BLE_ERR_REM_USER_CONN_TERM);
}
bool operationMemoryAvailable() {
    if (linkSnapshot().stopped) return false;
    const auto freeBytes = heap_caps_get_free_size(INTERNAL_CAPS);
    const auto largest = heap_caps_get_largest_free_block(INTERNAL_CAPS);
    if (bluetoothOperationMemoryReady(freeBytes, largest)) return true;
    Serial.printf("Bluetooth memory guard: internal_free=%u, internal_largest=%u\n", unsigned(freeBytes), unsigned(largest));
    stopLink("Internal memory low; yielding to networking");
    return false;
}
int gapEvent(ble_gap_event* event, void* context) {
    const uint32_t attempt = uintptr_t(context);
    const auto before = linkSnapshot();
    if (attempt != before.attempt) {
        // A late success from a retired attempt must not replace the current
        // connection. All other retired events are harmless to the new link.
        if (event->type == BLE_GAP_EVENT_CONNECT && event->connect.status == 0)
            ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }
    if (event->type == BLE_GAP_EVENT_CONNECT) {
        Serial.printf("Bluetooth NimBLE GAP connect: status=%d, handle=%u, attempt=%u\n",
            event->connect.status, unsigned(event->connect.conn_handle), unsigned(attempt));
        portENTER_CRITICAL(&handlesMux);
        batteryLink.pending = false;
        batteryLink.connected = event->connect.status == 0;
        batteryLink.connection = batteryLink.connected ? event->connect.conn_handle : BLE_HS_CONN_HANDLE_NONE;
        batteryLink.idleAt = millis();
        const bool reject = batteryLink.stopped && batteryLink.connected;
        portEXIT_CRITICAL(&handlesMux);
        if (reject) ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        const auto& disconnected = event->disconnect.conn;
        const bool peerMatches = !memcmp(before.peer.val, disconnected.peer_ota_addr.val, sizeof(before.peer.val)) ||
            !memcmp(before.peer.val, disconnected.peer_id_addr.val, sizeof(before.peer.val));
        Serial.printf("Bluetooth NimBLE GAP disconnect: reason=%d, handle=%u, opening=%u, peer_match=%u, attempt=%u\n",
            event->disconnect.reason, unsigned(disconnected.conn_handle), unsigned(before.pending),
            unsigned(peerMatches), unsigned(attempt));
        portENTER_CRITICAL(&handlesMux);
        // NimBLE may report a failed establishment as DISCONNECT before the
        // CONNECT callback. There is then no established local handle to match.
        if (bluetoothDisconnectCompletesAttempt(attempt == batteryLink.attempt, peerMatches,
            batteryLink.pending, disconnected.conn_handle == batteryLink.connection)) {
            batteryLink.pending = false;
            batteryLink.connected = false;
            batteryLink.connection = BLE_HS_CONN_HANDLE_NONE;
            discovery.active = false;
            batteryLink.idleAt = millis();
        }
        portEXIT_CRITICAL(&handlesMux);
    } else if (event->type == BLE_GAP_EVENT_NOTIFY_RX) {
        portENTER_CRITICAL(&handlesMux);
        const bool expected = batteryLink.connected && !batteryLink.stopped &&
            event->notify_rx.conn_handle == batteryLink.connection &&
            event->notify_rx.attr_handle == discovery.handles.notifier;
        portEXIT_CRITICAL(&handlesMux);
        if (!expected) return 0;
        Packet packet;
        packet.length = OS_MBUF_PKTLEN(event->notify_rx.om);
        const bool copied = packet.length <= sizeof(packet.bytes) &&
            os_mbuf_copydata(event->notify_rx.om, 0, packet.length, packet.bytes) == 0;
        if (!copied || xQueueSend(notifications, &packet, 0) != pdTRUE) {
            portENTER_CRITICAL(&handlesMux); notificationOverflow = true; portEXIT_CRITICAL(&handlesMux);
        }
    }
    return 0;
}
class ScanCallbacks : public NimBLEScanCallbacks {
    void onScanEnd(const NimBLEScanResults&, int) override { xSemaphoreGive(scanFinished); }
} scanCallbacks;

bool connectBattery(const NimBLEAdvertisedDevice& device, GattHandles& handles) {
    const auto state = linkSnapshot();
    if (state.stopped || state.pending || state.connected || ble_gap_conn_active()) return false;
    uint8_t ownType;
    if (ble_hs_id_infer_auto(0, &ownType) != 0) return false;
    const auto address = device.getAddress();
    portENTER_CRITICAL(&handlesMux);
    if (++nextAttempt == 0) ++nextAttempt;
    batteryLink.attempt = nextAttempt;
    batteryLink.peer = *address.getBase();
    batteryLink.pending = true;
    discovery.handles = GattHandles{};
    portEXIT_CRITICAL(&handlesMux);
    const int rc = ble_gap_connect(ownType, address.getBase(), CONNECT_TIMEOUT_MS, nullptr, gapEvent,
        reinterpret_cast<void*>(uintptr_t(nextAttempt)));
    if (rc != 0) {
        portENTER_CRITICAL(&handlesMux); batteryLink.pending = false; portEXIT_CRITICAL(&handlesMux);
        Serial.printf("Bluetooth NimBLE connect rejected: rc=%d\n", rc);
        return false;
    }
    const uint32_t started = millis();
    while (linkSnapshot().pending && millis() - started < CONNECT_TIMEOUT_MS + 1000) {
        if (!operationMemoryAvailable()) return false;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    const auto connected = linkSnapshot();
    if (connected.pending) { stopLink("Connection timed out"); return false; }
    if (!connected.connected || !operationMemoryAvailable()) return false;
    handles.connection = connected.connection;
    portENTER_CRITICAL(&handlesMux); discovery.handles.connection = handles.connection; portEXIT_CRITICAL(&handlesMux);
    return true;
}
bool disconnectBattery(const GattHandles&) {
    const auto state = linkSnapshot();
    if (state.stopped) return false;
    if (!state.connected) return true;
    const int rc = ble_gap_terminate(state.connection, BLE_ERR_REM_USER_CONN_TERM);
    if (rc != 0 && rc != BLE_HS_ENOTCONN) { stopLink("Disconnect request failed"); return false; }
    const uint32_t start = millis();
    while (linkSnapshot().connected && millis() - start < 3000) vTaskDelay(pdMS_TO_TICKS(20));
    if (linkSnapshot().connected) { stopLink("Disconnect timed out"); return false; }
    return true;
}
bool uuidMatches(const ble_uuid_any_t& uuid, uint16_t shortId, const char* expanded) {
    // No allocation inside callbacks/critical sections. Match short UUIDs and
    // the expanded Bluetooth-base representation used by some Renogy firmware.
    (void)expanded;
    if (uuid.u.type == BLE_UUID_TYPE_16) return ble_uuid_u16(&uuid.u) == shortId;
    const auto& expected = shortId == 0xffd0 ? writerServiceUuid : shortId == 0xffd1 ? writerUuid :
        shortId == 0xfff1 ? notifyUuid : cccdUuid;
    return ble_uuid_cmp(&uuid.u, expected.getBase()) == 0;
}
// Tokens are opaque integers, not pointers to temporary operation contexts.
void* beginOperation() {
    portENTER_CRITICAL(&handlesMux);
    if (++discovery.token == 0) ++discovery.token;
    discovery.active = true; discovery.done = false; discovery.status = 0;
    const uint32_t token = discovery.token;
    portEXIT_CRITICAL(&handlesMux);
    return reinterpret_cast<void*>(uintptr_t(token));
}
bool accepts(uint16_t connection, void* token) { // Caller holds handlesMux.
    return bluetoothGattCallbackReady(discovery.active, batteryLink.stopped, batteryLink.connected,
        connection, batteryLink.connection, uintptr_t(token), discovery.token);
}
void completeOperation(int status) { // Caller holds handlesMux.
    discovery.done = true; discovery.active = false;
    discovery.status = status == BLE_HS_EDONE ? 0 : status;
}
int serviceCallback(uint16_t connection, const ble_gatt_error* error, const ble_gatt_svc* service, void* token) {
    portENTER_CRITICAL(&handlesMux);
    if (!accepts(connection, token)) { portEXIT_CRITICAL(&handlesMux); return BLE_HS_EAPP; }
    if (error->status) completeOperation(error->status);
    else if (service) {
        if (discovery.count >= 16) {
            completeOperation(BLE_HS_ENOMEM); portEXIT_CRITICAL(&handlesMux); return BLE_HS_ENOMEM;
        }
        auto& range = discovery.services[discovery.count++];
        range = {service->start_handle, service->end_handle, uuidMatches(service->uuid, 0xffd0, WRITE_SERVICE)};
    }
    portEXIT_CRITICAL(&handlesMux);
    return 0;
}
int characteristicCallback(uint16_t connection, const ble_gatt_error* error, const ble_gatt_chr* chr, void* token) {
    portENTER_CRITICAL(&handlesMux);
    if (!accepts(connection, token)) { portEXIT_CRITICAL(&handlesMux); return BLE_HS_EAPP; }
    if (error->status) completeOperation(error->status);
    else if (chr) {
        auto& h = discovery.handles;
        if (discovery.writerService && uuidMatches(chr->uuid, 0xffd1, WRITE_CHAR)) {
            h.writer = chr->val_handle; h.properties = chr->properties;
        }
        h.notifierEnd = bluetoothDescriptorEnd(h.notifier, h.notifierEnd, chr->def_handle);
        if (uuidMatches(chr->uuid, 0xfff1, NOTIFY_CHAR) && (chr->properties & BLE_GATT_CHR_PROP_NOTIFY)) {
            h.notifier = chr->val_handle; h.notifierEnd = discovery.serviceEnd;
        }
    }
    portEXIT_CRITICAL(&handlesMux);
    return 0;
}
int descriptorCallback(uint16_t connection, const ble_gatt_error* error, uint16_t,
                       const ble_gatt_dsc* descriptor, void* token) {
    portENTER_CRITICAL(&handlesMux);
    if (!accepts(connection, token)) { portEXIT_CRITICAL(&handlesMux); return BLE_HS_EAPP; }
    if (error->status) completeOperation(error->status);
    else if (descriptor && uuidMatches(descriptor->uuid, 0x2902, nullptr)) discovery.handles.descriptor = descriptor->handle;
    portEXIT_CRITICAL(&handlesMux);
    return 0;
}
int writeCallback(uint16_t connection, const ble_gatt_error* error, ble_gatt_attr*, void* token) {
    portENTER_CRITICAL(&handlesMux);
    if (accepts(connection, token)) completeOperation(error->status);
    portEXIT_CRITICAL(&handlesMux);
    return 0;
}
bool waitOperation(int rc, uint32_t limit) {
    if (rc != 0) {
        portENTER_CRITICAL(&handlesMux); discovery.active = false; portEXIT_CRITICAL(&handlesMux);
        Serial.printf("Bluetooth NimBLE GATT request rejected: rc=%d\n", rc);
        return false;
    }
    const uint32_t started = millis();
    while (millis() - started < limit) {
        if (!operationMemoryAvailable() || !linkSnapshot().connected) return false;
        portENTER_CRITICAL(&handlesMux);
        const bool done = discovery.done; const int status = discovery.status;
        portEXIT_CRITICAL(&handlesMux);
        if (done) {
            if (status) Serial.printf("Bluetooth NimBLE GATT failed: rc=%d\n", status);
            return status == 0;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    stopLink("GATT operation timed out");
    return false;
}
bool discoverHandles(GattHandles& handles) {
    const uint32_t started = millis();
    portENTER_CRITICAL(&handlesMux); discovery.count = 0; portEXIT_CRITICAL(&handlesMux);
    void* token = beginOperation();
    if (!waitOperation(ble_gattc_disc_all_svcs(handles.connection, serviceCallback, token), DISCOVERY_TIMEOUT_MS)) return false;
    portENTER_CRITICAL(&handlesMux); const unsigned count = discovery.count; portEXIT_CRITICAL(&handlesMux);
    for (unsigned i = 0; i < count; ++i) {
        portENTER_CRITICAL(&handlesMux);
        const auto service = discovery.services[i];
        discovery.writerService = service.writerService; discovery.serviceEnd = service.end;
        portEXIT_CRITICAL(&handlesMux);
        const uint32_t elapsed = millis() - started;
        if (elapsed >= DISCOVERY_TIMEOUT_MS) { stopLink("Discovery deadline exceeded"); return false; }
        token = beginOperation();
        if (!waitOperation(ble_gattc_disc_all_chrs(handles.connection, service.start, service.end,
            characteristicCallback, token), DISCOVERY_TIMEOUT_MS - elapsed)) return false;
        portENTER_CRITICAL(&handlesMux); handles = discovery.handles; portEXIT_CRITICAL(&handlesMux);
        if (handles.writer && handles.notifier) break;
    }
    if (!handles.writer || !handles.notifier || handles.notifierEnd <= handles.notifier ||
        !(handles.properties & (BLE_GATT_CHR_PROP_WRITE | BLE_GATT_CHR_PROP_WRITE_NO_RSP))) return false;
    const uint32_t elapsed = millis() - started;
    if (elapsed >= DISCOVERY_TIMEOUT_MS) { stopLink("Discovery deadline exceeded"); return false; }
    token = beginOperation();
    if (!waitOperation(ble_gattc_disc_all_dscs(handles.connection, handles.notifier, handles.notifierEnd,
        descriptorCallback, token), DISCOVERY_TIMEOUT_MS - elapsed)) return false;
    portENTER_CRITICAL(&handlesMux); handles = discovery.handles; portEXIT_CRITICAL(&handlesMux);
    Serial.printf("Bluetooth NimBLE discovery: elapsed=%u ms, services=%u, internal_free=%u, internal_largest=%u\n",
        unsigned(millis() - started), count, unsigned(heap_caps_get_free_size(INTERNAL_CAPS)),
        unsigned(heap_caps_get_largest_free_block(INTERNAL_CAPS)));
    return handles.descriptor != 0;
}
bool subscribe(const GattHandles& handles) {
    if (!operationMemoryAvailable()) return false;
    const uint8_t enable[] = {1, 0};
    void* token = beginOperation();
    return waitOperation(ble_gattc_write_flat(handles.connection, handles.descriptor, enable, sizeof(enable),
        writeCallback, token), 5000);
}
bool readRegisters(const GattHandles& handles, uint8_t id, uint16_t reg, uint16_t words, Packet& response) {
    if (!operationMemoryAvailable()) return false;
    xQueueReset(notifications);
    portENTER_CRITICAL(&handlesMux); notificationOverflow = false; portEXIT_CRITICAL(&handlesMux);
    uint8_t request[8]; RenogyProtocol::request(id, reg, words, request);
    if (handles.properties & BLE_GATT_CHR_PROP_WRITE_NO_RSP) {
        if (ble_gattc_write_no_rsp_flat(handles.connection, handles.writer, request, sizeof(request)) != 0) return false;
    } else {
        void* token = beginOperation();
        if (!waitOperation(ble_gattc_write_flat(handles.connection, handles.writer, request, sizeof(request),
            writeCallback, token), 5000)) return false;
    }
    response.length = 0;
    const uint32_t started = millis();
    while (millis() - started < 5000) {
        if (!operationMemoryAvailable() || !linkSnapshot().connected) return false;
        portENTER_CRITICAL(&handlesMux); const bool overflow = notificationOverflow; portEXIT_CRITICAL(&handlesMux);
        if (overflow) return false;
        Packet chunk;
        if (xQueueReceive(notifications, &chunk, pdMS_TO_TICKS(50)) != pdTRUE) continue;
        if (response.length + chunk.length > sizeof(response.bytes)) return false;
        memcpy(response.bytes + response.length, chunk.bytes, chunk.length); response.length += chunk.length;
        if (response.length >= 2 && response.bytes[1] == 0x83) return false;
        if (response.length >= size_t(words * 2 + 5)) return RenogyProtocol::valid(response.bytes, response.length, id, words);
    }
    return false;
}
bool cleanupComplete() {
    const auto state = linkSnapshot();
    return bluetoothNimbleCleanupReady(state.pending, state.connected, ble_gap_conn_active(),
        NimBLEDevice::getScan()->isScanning(), millis(), state.idleAt);
}
bool addressValid(const String& address) {
    if (address.isEmpty()) return true;
    if (address.length() != 17) return false;
    for (unsigned i = 0; i < 17; ++i)
        if (i % 3 == 2 ? address[i] != ':' : !isxdigit(static_cast<unsigned char>(address[i]))) return false;
    return true;
}
}
