#include "RenogyBatteries.h"
#include "RenogyProtocol.h"
#include "RenogyProtocolChecks.h"
#include "BatteryFreshness.h"
#include "BluetoothRecoveryPolicy.h"
#include "WebPageBuilder.h"
#include "Logger.h"
#include <Preferences.h>
#include "RenogyNimbleTransport.h"

RenogyBatteries renogyBatteries;
void RenogyBatteries::begin() {
    if (!renogyProtocolChecks()) { logger.error("Renogy packet self-check failed; Bluetooth not started"); return; }
    logger.info("Renogy packet self-check passed");
    mutex_ = xSemaphoreCreateMutex();
    notifications = xQueueCreate(8, sizeof(Packet));
    scanFinished = xSemaphoreCreateBinary();
    if (!mutex_ || !notifications || !scanFinished) { logger.error("Battery Bluetooth memory allocation failed"); return; }
    logger.info("Bluetooth stack: NimBLE-Arduino 2.3.9; one central connection, no Bluedroid host");
    Preferences prefs;
    if (prefs.begin("renogy", true)) {
        enabled_ = prefs.getBool("enabled", true);
        for (int i = 0; i < 2; ++i) {
            String key = "mac" + String(i);
            readings_[i].address = prefs.getString(key.c_str(), "");
            key = "id" + String(i); deviceIds_[i] = prefs.getUChar(key.c_str(), 255);
            readings_[i].error = !enabled_ ? "Monitoring disabled" : readings_[i].address.isEmpty() ? "Not configured" : "Waiting for first Bluetooth poll";
        }
        prefs.end();
    }
    ready_ = xTaskCreate(task, "renogy", 8192, this, 1, nullptr) == pdPASS;
    if (!ready_) { status_ = "Bluetooth task failed to start"; logger.error(status_); }
}
void RenogyBatteries::task(void* self) { static_cast<RenogyBatteries*>(self)->run(); }
String RenogyBatteries::status() {
    if (!mutex_) return "Bluetooth task unavailable";
    xSemaphoreTake(mutex_, portMAX_DELAY);
    String result = status_;
    xSemaphoreGive(mutex_);
    return result;
}
void RenogyBatteries::setStatus(const String& message) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    status_ = message;
    xSemaphoreGive(mutex_);
    Serial.printf("Bluetooth: %s; heap=%u, largest=%u\n", message.c_str(), ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}
void RenogyBatteries::update(AppState& state) {
    if (!mutex_) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    for (int i = 0; i < 2; ++i) {
        state.lithium[i] = readings_[i];
        if (!batterySampleFresh(state.lithium[i].valid, millis(), state.lithium[i].updatedMs)) {
            state.lithium[i].valid = false;
            state.lithium[i].temperatureValid = false;
        }
    }
    xSemaphoreGive(mutex_);
}
void RenogyBatteries::run() {
    setStatus("NimBLE waiting 30 seconds for network startup");
    vTaskDelay(pdMS_TO_TICKS(30000));
    if (!NimBLEDevice::init("CamperSentinel")) {
        setStatus("NimBLE initialization failed; polling paused until reboot");
        vTaskDelete(nullptr);
        return;
    }
    // Renogy responses are at most 39 bytes and may arrive in fragments.
    // Keep the default ATT MTU; no full attribute cache or MTU exchange needed.
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(160); scan->setWindow(24);
    scan->setMaxResults(32); scan->setScanCallbacks(&scanCallbacks);
    setStatus("NimBLE ready; preparing first scan");
    uint32_t lastCleanupReport = 0;
    uint8_t firstBattery = 1;
    for (;;) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        const bool enabled = enabled_;
        xSemaphoreGive(mutex_);
        if (!enabled) {
            setStatus("Bluetooth monitoring disabled");
            do {
                vTaskDelay(pdMS_TO_TICKS(1000));
                xSemaphoreTake(mutex_, portMAX_DELAY);
                const bool resumed = enabled_;
                xSemaphoreGive(mutex_);
                if (resumed) break;
            } while (true);
            continue;
        }
        const auto current = linkSnapshot();
        if (current.stopped) {
            const bool clean = cleanupComplete();
            if (millis() - lastCleanupReport >= 30000) {
                Serial.printf("Bluetooth NimBLE cleanup: pending=%u, connected=%u, host_connecting=%u, ready=%u, internal_free=%u\n",
                    unsigned(current.pending), unsigned(current.connected), unsigned(ble_gap_conn_active()), unsigned(clean),
                    unsigned(heap_caps_get_free_size(INTERNAL_CAPS)));
                lastCleanupReport = millis();
            }
            if (!bluetoothRetryReady(true, clean, millis(), current.stoppedAt, current.failures) || !connectionMemoryAvailable()) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            portENTER_CRITICAL(&handlesMux);
            LinkState fresh; fresh.failures = batteryLink.failures;
            batteryLink = fresh; discovery.active = false;
            portEXIT_CRITICAL(&handlesMux);
            scan->clearResults(); xQueueReset(notifications);
            setStatus("NimBLE cleanup confirmed; retrying battery poll");
        }
        xSemaphoreTake(scanFinished, 0);
        if (!scan->start(5000, false, true)) {
            stopLink("Scan start failed");
            setStatus("NimBLE scan failed; cleanup and retry pending");
            continue;
        }
        if (xSemaphoreTake(scanFinished, pdMS_TO_TICKS(7000)) != pdTRUE) {
            scan->stop(); stopLink("Scan timed out");
            setStatus("NimBLE scan timed out; cleanup and retry pending");
            continue;
        }
        NimBLEScanResults found = scan->getResults();
        xSemaphoreTake(mutex_, portMAX_DELAY);
        candidateCount_ = 0;
        for (int pass = 0; pass < 2; ++pass)
        for (int n = 0; n < found.getCount() && candidateCount_ < 12; ++n) {
            const auto& device = *found.getDevice(n);
            if (!device.haveName()) continue;
            String name(device.getName().c_str());
            bool renogy = name.startsWith("RNGRBP") || name.startsWith("BT-TH") || name.startsWith("RBT");
            if (renogy != (pass == 0)) continue;
            candidates_[candidateCount_++] = {name, String(device.getAddress().toString().c_str())};
        }
        status_ = "Polling assigned batteries; discovery repeats every 5 minutes";
        xSemaphoreGive(mutex_);
        bool anyConfigured = false, allSucceeded = true;
        unsigned succeeded = 0, configured = 0;
        firstBattery ^= 1; // Alternate priority so a deferred second slot cannot starve.
        for (int slot = 0; slot < 2; ++slot) {
            const int i = (firstBattery + slot) % 2;
            xSemaphoreTake(mutex_, portMAX_DELAY);
            AppState::LithiumBattery reading = readings_[i];
            const uint8_t id = deviceIds_[i];
            const bool stillEnabled = enabled_;
            xSemaphoreGive(mutex_);
            if (!stillEnabled || reading.address.isEmpty()) continue;
            anyConfigured = true; ++configured;
            bool pollSucceeded = false;
            reading.error = linkSnapshot().stopped ? "Bluetooth recovery pending; check Bluetooth status"
                : "Battery not advertising (close the phone app and retry)";
            for (int n = 0; !linkSnapshot().stopped && n < found.getCount(); ++n) {
                const auto& device = *found.getDevice(n);
                String mac(device.getAddress().toString().c_str());
                if (!mac.equalsIgnoreCase(reading.address)) continue;
                if (!connectionMemoryAvailable()) {
                    reading.error = "Insufficient memory for Bluetooth connection; will retry";
                    break;
                }
                reading.error = "Bluetooth connection failed";
                GattHandles handles;
                setStatus("Connecting to battery " + String(i + 1));
                if (!connectBattery(device, handles)) {
                    if (linkSnapshot().stopped) reading.error = "Bluetooth connection failed; recovery pending";
                    break;
                }
                setStatus("Discovering battery " + String(i + 1) + " services");
                reading.error = "Renogy Bluetooth discovery failed or characteristics missing";
                const bool foundHandles = discoverHandles(handles);
                if (foundHandles) reading.error = "Bluetooth notification subscription failed";
                if (foundHandles && subscribe(handles)) {
                    setStatus("Reading battery " + String(i + 1));
                    Packet packet{};
                    reading.error = "Model read failed: check device ID/protocol";
                    if (readRegisters(handles, id, 5122, 8, packet)) {
                        char model[17]{}; memcpy(model, packet.bytes + 3, 16);
                        reading.model = model;
                        reading.error = "Unsupported model: expected Renogy Pro 200Ah";
                        if (reading.model.startsWith("RBT12200LFP-BT")) {
                            RenogyProtocol::Reading value;
                            reading.error = "Battery telemetry read failed";
                            if (readRegisters(handles, id, 5042, 6, packet) &&
                                RenogyProtocol::decode(packet.bytes, packet.length, id, value)) {
                                reading.voltage = value.voltage; reading.current = value.current;
                                reading.remainingAh = value.remainingAh; reading.capacityAh = value.capacityAh;
                                reading.valid = true; reading.updatedMs = millis(); reading.error = "";
                                pollSucceeded = true;
                                reading.temperatureValid = false;
                                if (readRegisters(handles, id, 5017, 17, packet) &&
                                    RenogyProtocol::word(packet.bytes + 3) >= 1 &&
                                    RenogyProtocol::word(packet.bytes + 3) <= 16) {
                                    reading.temperatureC = int16_t(RenogyProtocol::word(packet.bytes + 5)) * 0.1f;
                                    reading.temperatureValid = reading.temperatureC >= -50 && reading.temperatureC <= 100;
                                }
                            }
                        }
                    }
                }
                const bool released = disconnectBattery(handles);
                if (!released) reading.error = "Bluetooth polling paused; cleanup and retry pending";
                if (released && slot == 0) {
                    setStatus("Battery disconnected; allowing networking to settle for 10 seconds");
                    for (unsigned second = 0; second < 10; ++second) {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        xSemaphoreTake(mutex_, portMAX_DELAY);
                        const bool monitoringEnabled = enabled_;
                        xSemaphoreGive(mutex_);
                        if (!monitoringEnabled) break;
                    }
                }
                break;
            }
            allSucceeded = allSucceeded && pollSucceeded;
            if (pollSucceeded) ++succeeded;
            if (!batterySampleFresh(reading.valid, millis(), reading.updatedMs)) {
                reading.valid = false; reading.temperatureValid = false;
            }
            if (!reading.error.isEmpty())
                Serial.printf("Bluetooth battery %d: %s\n", i + 1, reading.error.c_str());
            xSemaphoreTake(mutex_, portMAX_DELAY);
            if (readings_[i].address == reading.address && deviceIds_[i] == id && enabled_) readings_[i] = reading;
            xSemaphoreGive(mutex_);
        }
        scan->clearResults();
        portENTER_CRITICAL(&handlesMux);
        if (anyConfigured && allSucceeded && !batteryLink.stopped) {
            if (batteryLink.goodCycles < 3) ++batteryLink.goodCycles;
            if (batteryLink.goodCycles >= 3) batteryLink.failures = 0;
        } else batteryLink.goodCycles = 0;
        const bool stoppedAfterPoll = batteryLink.stopped;
        portEXIT_CRITICAL(&handlesMux);
        setStatus(stoppedAfterPoll ? "NimBLE polling paused; cleanup and retry pending"
            : !anyConfigured ? "NimBLE ready; no batteries assigned"
            : "NimBLE poll: " + String(succeeded) + "/" + String(configured) + " batteries updated; next poll in 5 minutes");
        if (stoppedAfterPoll) vTaskDelay(pdMS_TO_TICKS(1000));
        else {
            const uint32_t waitStarted = millis();
            while (millis() - waitStarted < BATTERY_POLL_INTERVAL_MS) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                xSemaphoreTake(mutex_, portMAX_DELAY);
                const bool monitoringEnabled = enabled_;
                xSemaphoreGive(mutex_);
                if (!monitoringEnabled) break;
            }
        }
    }
}
void RenogyBatteries::registerRoutes(WebServer& server) {
    server.on("/settings/batteries", HTTP_GET, [this, &server]() { page(server); });
    server.on("/settings/batteries", HTTP_POST, [this, &server]() { save(server); });
}
void RenogyBatteries::page(WebServer& server) {
    if (!mutex_) { server.send(503, "text/plain", "Bluetooth unavailable"); return; }
    Candidate candidates[12]; AppState::LithiumBattery readings[2]; uint8_t ids[2];
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const int candidateCount = candidateCount_;
    const bool enabled = enabled_;
    const String status = status_;
    for (int i = 0; i < candidateCount; ++i) candidates[i] = candidates_[i];
    for (int i = 0; i < 2; ++i) { readings[i] = readings_[i]; ids[i] = deviceIds_[i]; }
    xSemaphoreGive(mutex_);
    WebPageBuilder page(server); page.begin("Camper Sentinel", "Bluetooth Batteries", "Assign each Renogy Pro battery to a stable slot.", 0);
    page.addNavigation("settings");
    page.beginCard("Discovery"); page.addValue("Status", status);
    page.addValue("Help", "Close the Renogy app to free the Bluetooth connection. Refresh this page for scan results.");
    for (int i = 0; i < candidateCount; ++i) page.addValue(candidates[i].name, candidates[i].address);
    page.endCard();
    page.beginCard("Assignments"); page.beginForm("/settings/batteries", "POST");
    page.addCheckbox("Enable Bluetooth monitoring", "enabled", enabled);
    for (int i = 0; i < 2; ++i) {
        page.addTextInput("Battery " + String(i + 1) + " address", "mac" + String(i), readings[i].address, 17, "Copy its Bluetooth address above; blank leaves this slot unused.");
        page.addNumberInput("Battery " + String(i + 1) + " device ID", "id" + String(i), ids[i], 1, 255, 1, "", "Normally 255 for a directly connected battery.", 0);
        page.addValue("Battery " + String(i + 1) + " status", readings[i].error.isEmpty() ? "Reading received" : readings[i].error);
    }
    page.addSubmitButton(); page.endForm(); page.endCard();
    page.send(server);
}
void RenogyBatteries::save(WebServer& server) {
    if (!mutex_ || !ready_) { server.send(503, "text/plain", "Bluetooth unavailable"); return; }
    String addresses[2]; uint8_t ids[2];
    for (int i = 0; i < 2; ++i) {
        String key = "mac" + String(i); addresses[i] = server.arg(key); addresses[i].trim(); addresses[i].toLowerCase();
        key = "id" + String(i); String text = server.arg(key); int id = text.toInt();
        if (!server.hasArg("mac" + String(i)) || !addressValid(addresses[i]) || id < 1 || id > 255 || String(id) != text) {
            server.send(400, "text/plain", "Invalid battery address or device ID"); return;
        }
        ids[i] = id;
    }
    if (!addresses[0].isEmpty() && addresses[0] == addresses[1]) { server.send(400, "text/plain", "Assign a different address to each battery"); return; }
    bool enabled = server.hasArg("enabled");
    Preferences prefs;
    if (!prefs.begin("renogy", false)) { server.send(500, "text/plain", "Battery settings storage unavailable"); return; }
    bool ok = prefs.putBool("enabled", enabled) > 0;
    for (int i = 0; i < 2; ++i) {
        String key = "mac" + String(i);
        if (addresses[i].isEmpty()) prefs.remove(key.c_str());
        else ok &= prefs.putString(key.c_str(), addresses[i]) > 0;
        key = "id" + String(i); ok &= prefs.putUChar(key.c_str(), ids[i]) > 0;
    }
    prefs.end();
    if (!ok) { server.send(500, "text/plain", "Battery settings save failed"); return; }
    xSemaphoreTake(mutex_, portMAX_DELAY); enabled_ = enabled;
    for (int i = 0; i < 2; ++i) {
        readings_[i] = AppState::LithiumBattery{}; readings_[i].address = addresses[i]; deviceIds_[i] = ids[i];
        readings_[i].error = !enabled ? "Monitoring disabled" : addresses[i].isEmpty() ? "Not configured" : "Waiting for next Bluetooth poll";
    }
    xSemaphoreGive(mutex_);
    server.sendHeader("Location", "/settings/batteries"); server.send(303, "text/plain", "");
}
