#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SSLClient.h>
#include <TinyGsmClient.h>
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include "Secrets.h"
#include "Logger.h"
#include "Modem.h"
#include "MQTT.h"
#include "Pins.h"
#include "TrustAnchors.h"
#include "Version.h"
#include "BatteryFreshness.h"
#include "MqttNetworkPolicy.h"
#include "WiFiService.h"

// Working SIM7000 TCP transport.
TinyGsmClient cellularClient(gsm);
WiFiClient wifiClient;

// Switch the underlying TCP socket only while TLS is stopped. A single TLS
// engine serves either network instead of keeping two engines in RAM.
class MqttTransport : public Client
{
public:
    Client* socket = &cellularClient;
    int connect(IPAddress ip, uint16_t port) override { return socket->connect(ip, port); }
    int connect(const char* host, uint16_t port) override { return socket->connect(host, port); }
    size_t write(uint8_t value) override { return socket->write(value); }
    size_t write(const uint8_t* data, size_t length) override { return socket->write(data, length); }
    int available() override { return socket->available(); }
    int read() override { return socket->read(); }
    int read(uint8_t* data, size_t length) override { return socket->read(data, length); }
    int peek() override { return socket->peek(); }
    void flush() override { socket->flush(); }
    void stop() override { socket->stop(); }
    uint8_t connected() override { return socket->connected(); }
    operator bool() override { return static_cast<bool>(*socket); }
} mqttTransport;

// TLS layer with server-certificate verification.
class MqttTlsClient : public SSLClient
{
public:
    using SSLClient::SSLClient;
    uint8_t connected() override
    {
        // A failed TCP/DNS attempt leaves a Print error flag. Avoid printing
        // the same TLS error every time the main loop checks connectivity.
        return getWriteError() ? 0 : SSLClient::connected();
    }
    int connect(const char* host, uint16_t port) override
    {
        clearWriteError();
        const int result = SSLClient::connect(host, port);
        // SSLClient can close its handshake engine without setting a Print
        // error. TCP connection failures already set SSL_CLIENT_CONNECT_FAIL.
        if (result != 1 && !getWriteError()) setWriteError(SSL_BR_CONNECT_FAIL);
        return result;
    }
};
MqttTlsClient tlsClient(
    mqttTransport,
    TAs,
    static_cast<size_t>(TAs_NUM),
    PIN_TLS_ENTROPY);

// MQTT protocol layer.
PubSubClient mqttClient(tlsClient);
static bool usingWifi = false;

MQTT mqtt;

namespace
{
// Require a clock at least as recent as this firmware's build day. Never
// silently fall back to SSLClient's cached library compilation timestamp.
time_t minimumTlsTime()
{
    static const time_t minimum = []() {
        struct tm date = {};
        strptime(__DATE__, "%b %d %Y", &date);
        return mktime(&date); // configTime below sets the system timezone to UTC.
    }();
    return minimum;
}

bool prepareTlsTime()
{
    time_t now = time(nullptr);
    if (now < minimumTlsTime() && cellular.isNetworkConnected())
    {
        int year, month, day, hour, minute, second;
        float zone;
        if (gsm.getNetworkTime(&year, &month, &day, &hour, &minute, &second, &zone) &&
            year >= 2026 && year <= 2099 && month >= 1 && month <= 12 &&
            day >= 1 && day <= 31 && hour >= 0 && hour <= 23 &&
            minute >= 0 && minute <= 59 && second >= 0 && second <= 59 &&
            zone >= -12 && zone <= 14)
        {
            struct tm date = {};
            date.tm_year = year - 1900;
            date.tm_mon = month - 1;
            date.tm_mday = day;
            date.tm_hour = hour;
            date.tm_min = minute;
            date.tm_sec = second;
            const time_t utc = mktime(&date) - static_cast<long>(zone * 3600);
            if (utc >= minimumTlsTime())
            {
                struct timeval clock = {utc, 0};
                settimeofday(&clock, nullptr);
                now = time(nullptr);
                logger.info("System clock synchronized from cellular network");
            }
        }
    }
    if (now < minimumTlsTime())
    {
        logger.warning("MQTT TLS deferred: waiting for current time from WiFi NTP or cellular network");
        return false;
    }
    constexpr uint32_t SECONDS_PER_DAY = 86400;
    constexpr uint32_t DAYS_BEFORE_UNIX_EPOCH = 719528;
    tlsClient.setVerificationTime(now / SECONDS_PER_DAY + DAYS_BEFORE_UNIX_EPOCH,
                                  now % SECONDS_PER_DAY);
    logger.infof("MQTT TLS validation time: %lu (UTC Unix seconds)", static_cast<unsigned long>(now));
    return true;
}

constexpr char STATUS_TOPIC[] =
    "rvmonitor/camper01/status";

constexpr char AMBIENT_TOPIC[] =
    "rvmonitor/camper01/temperature/ambient";

constexpr char AMBIENT_DISCOVERY_TOPIC[] =
    "homeassistant/sensor/camper01_ambient/config";

constexpr char BATTERY_TOPIC[] =
    "rvmonitor/camper01/power/battery_voltage";

constexpr char SHORE_POWER_TOPIC[] =
    "rvmonitor/camper01/power/shore";

constexpr char HEARTBEAT_TOPIC[] =
    "rvmonitor/camper01/heartbeat";

constexpr char BATTERY_DISCOVERY_TOPIC[] =
    "homeassistant/sensor/camper01_battery/config";

constexpr char SHORE_DISCOVERY_TOPIC[] =
    "homeassistant/binary_sensor/camper01_shore_power/config";

constexpr char SIGNAL_DISCOVERY_TOPIC[] =
    "homeassistant/sensor/camper01_signal/config";

constexpr char UPTIME_DISCOVERY_TOPIC[] =
    "homeassistant/sensor/camper01_uptime/config";
}

bool MQTT::begin(
    Settings& settings,
    AppState& state)
{
    settings_ = &settings;
    state_ = &state;
    // SNTP runs asynchronously and retries as WiFi becomes available.
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com");

    const DeviceSettings& config =
        settings_->get();

    if (!config.mqttEnabled)
    {
        logger.info(
            "MQTT disabled in settings");

        return true;
    }

    if (config.mqttHost.isEmpty())
    {
        logger.warning(
            "MQTT broker is not configured");

        return false;
    }

    // Existing MQTT initialization continues...
    mqttClient.setServer(config.mqttHost.c_str(),config.mqttPort);
    mqttClient.setKeepAlive(60);
    mqttClient.setSocketTimeout(30);
    mqttClient.setBufferSize(1024);

    return connect();
}

bool MQTT::preferWifi() const
{
    return preferMqttWifi(settings_->get().wifiEnabled && WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(),
        cellular.isDataConnected(), wifiCoolingDown_, millis(), wifiFailedAt_);
}

bool MQTT::connect()
{
    if (settings_ == nullptr)
    {
        logger.error(
            "MQTT connect failed: settings unavailable");

        return false;
    }

    const DeviceSettings& config =
        settings_->get();

    String clientId =
        config.hostname + "-mqtt";

    if (!config.mqttEnabled)
    {
        return false;
    }

    if (config.mqttHost.isEmpty())
    {
        logger.warning(
            "MQTT connect skipped: broker not configured");

        return false;
    }

    const bool wifiAvailable = preferWifi();
    // Synchronize the selected network even when both networks are offline,
    // so a network change cannot bypass the reconnect interval every loop.
    usingWifi = wifiAvailable;
    if (!wifiAvailable && !cellular.isDataConnected())
    {
        logger.warning(
            "MQTT skipped: WiFi and cellular data are offline");

        return false;
    }

    if (config.mqttTlsEnabled && !prepareTlsTime())
    {
        if (state_) state_->mqtt.connected = false;
        return false;
    }

    tlsClient.stop();
    wifiClient.stop();
    cellularClient.stop();
    mqttTransport.socket = usingWifi ? static_cast<Client*>(&wifiClient) : static_cast<Client*>(&cellularClient);
    Client* transport = config.mqttTlsEnabled ? static_cast<Client*>(&tlsClient) : static_cast<Client*>(&mqttTransport);
    mqttClient.setClient(*transport);
    mqttClient.setServer(config.mqttHost.c_str(), config.mqttPort);
    if (state_ != nullptr)
    {
        state_->mqtt.connected = false;
        state_->mqtt.transport = usingWifi ? "WiFi" : "Cellular";
    }

    logger.infof(
        "Connecting to MQTT broker %s:%u via %s (%s)",
        config.mqttHost.c_str(),
        static_cast<unsigned int>(
            config.mqttPort),
        usingWifi ? "WiFi" : "Cellular",
        config.mqttTlsEnabled ? "TLS" : "plain TCP");

    const bool connected =
        mqttClient.connect(
            clientId.c_str(),
            config.mqttUsername.c_str(),
            config.mqttPassword.c_str(),
            STATUS_TOPIC,
            1,
            true,
            "offline");

    if (!connected)
    {
        const int mqttState =
        mqttClient.state();
        // Capture before stop() changes the TLS engine state. Zero for plain TCP.
        const int tlsError = config.mqttTlsEnabled ? tlsClient.getWriteError() : 0;

        if (state_ != nullptr)
        {
        state_->mqtt.connected = false;
        state_->mqtt.lastError = mqttState;
        state_->mqtt.lastTlsError = tlsError;
        state_->mqtt.connectionFailureCount++;
        }
        logger.errorf(
            "MQTT connection failed, state=%d, TLS error=%d (2=TCP/DNS connect, 3=TLS handshake, 7=out of memory)",
            mqttState, tlsError);

        tlsClient.stop();
        transport->stop();
        if (usingWifi)
        {
            if (wifiFailures_ < 3) ++wifiFailures_;
            if (wifiFailures_ >= 3)
            {
                wifiCoolingDown_ = true;
                wifiFailedAt_ = millis();
                logger.warning("MQTT WiFi failed repeatedly: prefer cellular for five minutes when available");
            }
        }
        else
        {
            // If cellular also fails, try WiFi on the next scheduled retry.
            wifiCoolingDown_ = false;
        }

        return false;
    }

    mqttClient.publish(
        STATUS_TOPIC,
        "online",
        true);

    logger.infof("MQTT connected via %s (%s)",
        usingWifi ? "WiFi" : "Cellular",
        config.mqttTlsEnabled ? "TLS" : "plain TCP");
    if (usingWifi) { wifiFailures_ = 0; wifiCoolingDown_ = false; wifi.noteNetworkSuccess(); }
    lithiumDiscoveryPublished_ = false;
    smartShuntDiscoveryPublished_ = false;
    
    if (state_ != nullptr)
    {
    state_->mqtt.connected = true;
    state_->mqtt.lastError = 0;
    state_->mqtt.lastTlsError = 0;

    state_->mqtt.lastConnectedTimeSeconds =
        millis() / 1000UL;
    }

    if (!discoveryPublished)
    {
        if (publishDiscovery())
        {
            discoveryPublished = true;
        }
    }

    return true;
}

void MQTT::loop()
{
    if (settings_ == nullptr)
    {
        return;
    }

    const DeviceSettings& config =
        settings_->get();
        
    
    if (!config.mqttEnabled)
    {
        mqttClient.disconnect();
        if (state_ != nullptr) state_->mqtt.connected = false;
        return;
    }
    bool wifiAvailable = preferWifi();
    if (wifiAvailable && !usingWifi && mqttClient.connected())
    {
        // A WiFi retry must not tear down working cellular MQTT merely to
        // discover that the broker name still cannot be resolved. Probe via
        // the WiFi stack while the existing TLS transport remains cellular.
        IPAddress brokerAddress;
        if (!WiFi.hostByName(config.mqttHost.c_str(), brokerAddress))
        {
            wifiCoolingDown_ = true;
            wifiFailedAt_ = millis();
            wifiAvailable = false;
            wifi.noteNetworkFailure();
            logger.warning("WiFi broker DNS probe failed; keeping cellular MQTT connected and retrying WiFi in five minutes");
        }
    }
    const bool transportChanged = wifiAvailable != usingWifi;
    const bool switchLiveConnection = transportChanged && mqttClient.connected();
    if (switchLiveConnection)
    {
        mqttClient.disconnect();
        tlsClient.stop();
        wifiClient.stop();
        cellularClient.stop();
    }
    if (mqttClient.connected())
    {
        mqttClient.loop();
        if (state_ != nullptr) state_->mqtt.connected = mqttClient.connected();
        return;
    }
    if (state_ != nullptr) state_->mqtt.connected = false;

    const unsigned long now = millis();

    if (
        !switchLiveConnection && now - lastReconnectAttemptMs <
        RECONNECT_INTERVAL_MS)
    {
        return;
    }

    lastReconnectAttemptMs = now;

    connect();
}

bool MQTT::isConnected() const
{
    return mqttClient.connected();
}

bool MQTT::publishAmbient(float temperatureF)
{
    if (!mqttClient.connected())
    {
        logger.warning(
            "Ambient publish skipped: MQTT offline");

        return false;
    }

    char payload[16];

    snprintf(
        payload,
        sizeof(payload),
        "%.1f",
        temperatureF);

    const bool published = mqttClient.publish(
        AMBIENT_TOPIC,
        payload,
        true);

    if (published)
    {
        logger.infof(
            "Published ambient temperature: %s F",
            payload);
    }
    else
    {
        logger.errorf(
            "Ambient publish failed, MQTT state=%d, connected=%s",
            mqttClient.state(),
            mqttClient.connected() ? "yes" : "no");
    }

    return published;
}
bool MQTT::publishDiscovery()
{
    bool success = true;

    success &= publishAmbientDiscovery();
    success &= publishBatteryDiscovery();
    success &= publishShorePowerDiscovery();
    success &= publishSignalDiscovery();
    success &= publishUptimeDiscovery();

    if (success)
    {
        logger.info("Published all MQTT discovery entities");
    }
    else
    {
        logger.warning("One or more discovery publishes failed");
    }

    return success;
}
bool MQTT::publishAmbientDiscovery()
{
    if (!mqttClient.connected())
    {
        return false;
    }

    JsonDocument document;

    document["name"] = "Ambient Temperature";
    document["unique_id"] = "camper01_ambient_temperature";
    document["default_entity_id"] =
        "sensor.camper_ambient_temperature";

    document["state_topic"] = AMBIENT_TOPIC;
    document["availability_topic"] = STATUS_TOPIC;

    document["payload_available"] = "online";
    document["payload_not_available"] = "offline";

    document["device_class"] = "temperature";
    document["state_class"] = "measurement";
    document["unit_of_measurement"] = "\xC2\xB0"
                                      "F";

    JsonObject device =
        document["device"].to<JsonObject>();

    JsonArray identifiers =
        device["identifiers"].to<JsonArray>();

    identifiers.add("camper_monitor_01");

    device["name"] = "Camper Monitor";
    device["manufacturer"] = "DIY";
    device["model"] = "LILYGO T-SIM7000G";
    device["sw_version"] = FW_VERSION;

    char payload[768];

    const size_t payloadLength =
        serializeJson(
            document,
            payload,
            sizeof(payload));

    if (
        payloadLength == 0 ||
        payloadLength >= sizeof(payload))
    {
        logger.error(
            "Discovery JSON buffer is too small");

        return false;
    }

    const bool published =
        mqttClient.publish(
            AMBIENT_DISCOVERY_TOPIC,
            payload,
            true);

    if (published)
    {
        logger.info(
            "Published ambient MQTT discovery");
    }
    else
    {
        logger.error(
            "Failed to publish ambient MQTT discovery");
    }

    return published;
}

bool MQTT::publishBatteryDiscovery()
{
    JsonDocument document;

    document["name"] = "Battery Voltage";
    document["unique_id"] = "camper01_battery_voltage";
    document["state_topic"] = BATTERY_TOPIC;
    document["availability_topic"] = STATUS_TOPIC;

    document["payload_available"] = "online";
    document["payload_not_available"] = "offline";

    document["device_class"] = "voltage";
    document["state_class"] = "measurement";
    document["unit_of_measurement"] = "V";
    document["suggested_display_precision"] = 2;

    JsonObject device =
        document["device"].to<JsonObject>();

    JsonArray identifiers =
        device["identifiers"].to<JsonArray>();

    identifiers.add("camper_monitor_01");

    device["name"] = "Camper Monitor";
    device["manufacturer"] = "DIY";
    device["model"] = "LILYGO T-SIM7000G";
    device["sw_version"] = FW_VERSION;

    char payload[700];

    const size_t length =
        serializeJson(document, payload, sizeof(payload));

    return length > 0 &&
           length < sizeof(payload) &&
           mqttClient.publish(
               BATTERY_DISCOVERY_TOPIC,
               payload,
               true);
}
bool MQTT::publishShorePowerDiscovery()
{
    JsonDocument document;

    document["name"] = "Shore Power";
    document["unique_id"] = "camper01_shore_power";
    document["state_topic"] = SHORE_POWER_TOPIC;
    document["availability_topic"] = STATUS_TOPIC;

    document["payload_on"] = "ON";
    document["payload_off"] = "OFF";

    document["payload_available"] = "online";
    document["payload_not_available"] = "offline";

    document["device_class"] = "plug";

    JsonObject device =
        document["device"].to<JsonObject>();

    JsonArray identifiers =
        device["identifiers"].to<JsonArray>();

    identifiers.add("camper_monitor_01");

    device["name"] = "Camper Monitor";
    device["manufacturer"] = "DIY";
    device["model"] = "LILYGO T-SIM7000G";
    device["sw_version"] = FW_VERSION;

    char payload[700];

    const size_t length =
        serializeJson(document, payload, sizeof(payload));

    return length > 0 &&
           length < sizeof(payload) &&
           mqttClient.publish(
               SHORE_DISCOVERY_TOPIC,
               payload,
               true);
}
bool MQTT::publishSignalDiscovery()
{
    JsonDocument document;

    document["name"] = "LTE Signal Quality";
    document["unique_id"] = "camper01_signal_quality";

    document["state_topic"] = HEARTBEAT_TOPIC;
    document["value_template"] =
        "{{ value_json.signal_quality }}";

    document["availability_topic"] = STATUS_TOPIC;
    document["payload_available"] = "online";
    document["payload_not_available"] = "offline";

    document["state_class"] = "measurement";
    document["unit_of_measurement"] = "CSQ";
    document["icon"] = "mdi:signal-cellular-3";

    JsonObject device =
        document["device"].to<JsonObject>();

    JsonArray identifiers =
        device["identifiers"].to<JsonArray>();

    identifiers.add("camper_monitor_01");

    device["name"] = "Camper Monitor";
    device["manufacturer"] = "DIY";
    device["model"] = "LILYGO T-SIM7000G";
    device["sw_version"] = FW_VERSION;

    char payload[700];

    const size_t length =
        serializeJson(document, payload, sizeof(payload));

    return length > 0 &&
           length < sizeof(payload) &&
           mqttClient.publish(
               SIGNAL_DISCOVERY_TOPIC,
               payload,
               true);
}
bool MQTT::publishUptimeDiscovery()
{
    JsonDocument document;

    document["name"] = "Uptime";
    document["unique_id"] = "camper01_uptime";

    document["state_topic"] = HEARTBEAT_TOPIC;
    document["value_template"] =
        "{{ value_json.uptime_seconds }}";

    document["availability_topic"] = STATUS_TOPIC;
    document["payload_available"] = "online";
    document["payload_not_available"] = "offline";

    document["device_class"] = "duration";
    document["state_class"] = "total_increasing";
    document["unit_of_measurement"] = "s";
    document["icon"] = "mdi:timer-outline";

    JsonObject device =
        document["device"].to<JsonObject>();

    JsonArray identifiers =
        device["identifiers"].to<JsonArray>();

    identifiers.add("camper_monitor_01");

    device["name"] = "Camper Monitor";
    device["manufacturer"] = "DIY";
    device["model"] = "LILYGO T-SIM7000G";
    device["sw_version"] = FW_VERSION;

    char payload[700];

    const size_t length =
        serializeJson(document, payload, sizeof(payload));

    return length > 0 &&
           length < sizeof(payload) &&
           mqttClient.publish(
               UPTIME_DISCOVERY_TOPIC,
               payload,
               true);
}

bool MQTT::publishShorePower(bool present)
{
    if (!mqttClient.connected())
    {
        logger.warning(
            "Shore power publish skipped: MQTT offline");

        return false;
    }

    const char *payload =
        present ? "ON" : "OFF";

    const bool published =
        mqttClient.publish(
            SHORE_POWER_TOPIC,
            payload,
            true);

    if (published)
    {
        logger.infof(
            "Published shore power: %s",
            payload);
    }
    else
    {
        logger.error(
            "Failed to publish shore power");
    }

    return published;
}
bool MQTT::publishHeartbeat(const AppState &state)
{
    if (!mqttClient.connected())
    {
        logger.warning(
            "Heartbeat publish skipped: MQTT offline");

        return false;
    }

    JsonDocument document;

    document["status"] = "online";
    document["uptime_seconds"] = state.system.uptimeSeconds;
    document["free_heap"] = state.system.freeHeap;
    document["signal_quality"] = state.cellular.signalQuality;

    document["network_connected"] =
        state.cellular.networkConnected;

    document["data_connected"] =
        state.cellular.dataConnected;

    document["mqtt_connected"] =
        state.mqtt.connected;
    document["mqtt_transport"] = state.mqtt.transport;
    document["wifi_connected"] = WiFi.status() == WL_CONNECTED;

    if (state.temperature.valid)
    {
        document["ambient_temperature_f"] =
            state.temperature.ambientF;
    }

    if (state.power.batteryVoltageValid)
    {
        document["battery_voltage"] =
            state.power.batteryVoltage;
    }

    document["shore_power"] =
        state.power.shorePowerPresent;

    document["operator"] =
        state.cellular.operatorName;

    document["ip_address"] =
        usingWifi ? WiFi.localIP().toString() : state.cellular.ipAddress;

    document["firmware"] =
        FW_VERSION;

    char payload[512];

    const size_t length =
        serializeJson(
            document,
            payload,
            sizeof(payload));

    if (
        length == 0 ||
        length >= sizeof(payload))
    {
        logger.error(
            "Heartbeat JSON buffer is too small");

        return false;
    }

    const bool published =
        mqttClient.publish(
            HEARTBEAT_TOPIC,
            payload,
            true);

    if (published)
    {
        logger.infof(
            "Published heartbeat: uptime=%lus, heap=%lu",
            state.system.uptimeSeconds,
            state.system.freeHeap);
    }
    else
    {
        logger.errorf(
            "Heartbeat publish failed, MQTT state=%d",
            mqttClient.state());
    }

    return published;
}
bool MQTT::publishBattery(float voltage)
{
    if (!mqttClient.connected())
    {
        return false;
    }

    char payload[16];

    snprintf(
        payload,
        sizeof(payload),
        "%.2f",
        voltage);

    return mqttClient.publish(
        BATTERY_TOPIC,
        payload,
        true);
}

bool MQTT::publishLithiumBatteries(const AppState& state)
{
    if (!mqttClient.connected()) return false;
    bool success = true;
    const bool discoveryNeeded = !lithiumDiscoveryPublished_ ||
        lithiumDiscoveryAddresses_[0] != state.lithium[0].address ||
        lithiumDiscoveryAddresses_[1] != state.lithium[1].address;
    const char* fields[] = {"charge_percent", "voltage", "current", "remaining_ah", "temperature_c"};
    const char* names[] = {"Charge", "Voltage", "Current", "Remaining Charge", "Temperature"};
    const char* classes[] = {"battery", "voltage", "current", "", "temperature"};
    const char* units[] = {"%", "V", "A", "Ah", "\xC2\xB0" "C"};
    for (int i = 0; i < 2; ++i)
    {
        const auto& battery = state.lithium[i];
        String base = "rvmonitor/camper01/battery/" + String(i + 1);
        const bool valid = batterySampleFresh(battery.valid, millis(), battery.updatedMs);
        success &= mqttClient.publish((base + "/availability").c_str(), valid ? "online" : "offline", true);
        JsonDocument telemetry;
        telemetry["address"] = battery.address;
        telemetry["valid"] = valid;
        telemetry["model"] = battery.model;
        telemetry["last_poll_error"] = battery.error;
        if (valid)
        {
            telemetry["voltage"] = battery.voltage;
            telemetry["current"] = battery.current;
            telemetry["charge_percent"] = battery.remainingAh / battery.capacityAh * 100.0f;
            telemetry["remaining_ah"] = battery.remainingAh;
            telemetry["capacity_ah"] = battery.capacityAh;
            telemetry["reading_age_seconds"] = (millis() - battery.updatedMs) / 1000UL;
            if (battery.temperatureValid) telemetry["temperature_c"] = battery.temperatureC;
        }
        else telemetry["error"] = battery.error.isEmpty() ? "Reading is stale" : battery.error;
        char payload[768];
        size_t length = serializeJson(telemetry, payload, sizeof(payload));
        success &= length > 0 && length < sizeof(payload) && mqttClient.publish((base + "/state").c_str(), payload, true);
        // Retained discovery is resent after reconnects and assignment changes.
        for (int f = 0; discoveryNeeded && f < 5; ++f)
        {
            JsonDocument config;
            String unique = "camper01_battery_" + String(i + 1) + "_" + fields[f];
            config["name"] = "Battery " + String(i + 1) + " " + names[f];
            config["unique_id"] = unique;
            config["state_topic"] = base + "/state";
            config["value_template"] = "{{ value_json." + String(fields[f]) + " | default(none) }}";
            config["unit_of_measurement"] = units[f];
            if (classes[f][0]) config["device_class"] = classes[f];
            config["state_class"] = "measurement";
            config["expire_after"] = BATTERY_MAX_AGE_MS / 1000UL;
            JsonArray availability = config["availability"].to<JsonArray>();
            availability.add<JsonObject>()["topic"] = STATUS_TOPIC;
            availability.add<JsonObject>()["topic"] = base + "/availability";
            config["availability_mode"] = "all";
            config["device"]["identifiers"][0] = "camper_monitor_01";
            config["device"]["name"] = "Camper Monitor";
            length = serializeJson(config, payload, sizeof(payload));
            success &= length > 0 && length < sizeof(payload) && mqttClient.publish(
                ("homeassistant/sensor/" + unique + "/config").c_str(), payload, true);
        }
    }
    if (success && discoveryNeeded)
    {
        lithiumDiscoveryPublished_ = true;
        for (int i = 0; i < 2; ++i) lithiumDiscoveryAddresses_[i] = state.lithium[i].address;
    }
    return success;
}

bool MQTT::publishSmartShunt(const AppState& state)
{
    if (!mqttClient.connected())
    {
        return false;
    }

    const auto& shunt = state.power.smartShunt;

    const bool valid =
        shunt.valid &&
        (millis() - shunt.updatedMs < 5000UL);

    constexpr char STATE_TOPIC[] =
        "rvmonitor/camper01/smartshunt/state";

    constexpr char AVAILABILITY_TOPIC[] =
        "rvmonitor/camper01/smartshunt/availability";

    bool success = true;

    // Build SmartShunt telemetry.
    JsonDocument telemetry;

    telemetry["valid"] = valid;

    if (valid)
    {
        telemetry["voltage"] = shunt.voltage;
        telemetry["current"] = shunt.current;
        telemetry["soc"] = shunt.soc;
        telemetry["power"] = shunt.power;

        telemetry["reading_age_seconds"] =
            (millis() - shunt.updatedMs) / 1000UL;
    }

    char payload[768];

    size_t length =
        serializeJson(telemetry, payload, sizeof(payload));

    // Mark invalid readings unavailable before publishing.
    if (!valid)
    {
        success &= mqttClient.publish(
            AVAILABILITY_TOPIC,
            "offline",
            true);
    }

    const bool telemetryPublished =
        length > 0 &&
        length < sizeof(payload) &&
        mqttClient.publish(
            STATE_TOPIC,
            payload,
            true);

    success &= telemetryPublished;

    // Only mark measurements available after a successful publish.
    if (valid)
    {
        success &= mqttClient.publish(
            AVAILABILITY_TOPIC,
            telemetryPublished ? "online" : "offline",
            true);
    }

    // Publish Home Assistant discovery.
    if (!smartShuntDiscoveryPublished_)
    {
        const char* fields[] = {
            "voltage",
            "current",
            "soc",
            "power"
        };

        const char* names[] = {
            "SmartShunt Voltage",
            "SmartShunt Current",
            "SmartShunt State of Charge",
            "SmartShunt Power"
        };

        const char* classes[] = {
            "voltage",
            "current",
            "battery",
            "power"
        };

        const char* units[] = {
            "V",
            "A",
            "%",
            "W"
        };

        const int precision[] = {
            3, 2, 1, 0
        };

        for (int i = 0; i < 4; ++i)
        {
            JsonDocument config;

            String unique =
                "camper01_smartshunt_" +
                String(fields[i]);

            config["name"] = names[i];
            config["unique_id"] = unique;
            config["state_topic"] = STATE_TOPIC;

            config["value_template"] =
                "{{ value_json." +
                String(fields[i]) +
                " | default(none) }}";

            config["device_class"] = classes[i];
            config["unit_of_measurement"] = units[i];
            config["state_class"] = "measurement";
            config["suggested_display_precision"] =
                precision[i];

            config["expire_after"] = 90;

            JsonArray availability =
                config["availability"].to<JsonArray>();

            availability.add<JsonObject>()["topic"] =
                STATUS_TOPIC;

            availability.add<JsonObject>()["topic"] =
                AVAILABILITY_TOPIC;

            config["availability_mode"] = "all";

            config["device"]["identifiers"][0] =
                "camper_monitor_01";

            config["device"]["name"] =
                "Camper Monitor";

            length = serializeJson(
                config,
                payload,
                sizeof(payload));

            success &= length > 0 &&
                length < sizeof(payload) &&
                mqttClient.publish(
                    ("homeassistant/sensor/" +
                     unique + "/config").c_str(),
                    payload,
                    true);
        }

        // SmartShunt connection binary sensor.
        JsonDocument config;

        config["name"] = "SmartShunt Connection";
        config["unique_id"] =
            "camper01_smartshunt_connection";

        config["state_topic"] = AVAILABILITY_TOPIC;
        config["payload_on"] = "online";
        config["payload_off"] = "offline";
        config["device_class"] = "connectivity";

        config["availability_topic"] = STATUS_TOPIC;
        config["payload_available"] = "online";
        config["payload_not_available"] = "offline";

        config["expire_after"] = 90;

        config["device"]["identifiers"][0] =
            "camper_monitor_01";

        config["device"]["name"] =
            "Camper Monitor";

        length = serializeJson(
            config,
            payload,
            sizeof(payload));

        success &= length > 0 &&
            length < sizeof(payload) &&
            mqttClient.publish(
                "homeassistant/binary_sensor/"
                "camper01_smartshunt_connection/config",
                payload,
                true);

        if (success)
        {
            smartShuntDiscoveryPublished_ = true;

            logger.info(
                "Published SmartShunt MQTT discovery");
        }
    }

    return success;
}

