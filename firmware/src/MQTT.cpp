#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SSLClient.h>
#include <TinyGsmClient.h>
#include "Secrets.h"
#include "Logger.h"
#include "Modem.h"
#include "MQTT.h"
#include "Pins.h"
#include "TrustAnchors.h"
#include "Version.h"

// Working SIM7000 TCP transport.
TinyGsmClient cellularClient(gsm);

// TLS layer with server-certificate verification.
SSLClient tlsClient(
    cellularClient,
    TAs,
    static_cast<size_t>(TAs_NUM),
    PIN_TLS_ENTROPY);

// MQTT protocol layer.
PubSubClient mqttClient(tlsClient);

MQTT mqtt;

namespace
{
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

    if (!cellular.isDataConnected())
    {
        logger.warning(
            "MQTT skipped: packet data is offline");

        return false;
    }

    tlsClient.stop();

    logger.infof(
        "Connecting to MQTT TLS broker %s:%u",
        config.mqttHost.c_str(),
        static_cast<unsigned int>(
            config.mqttPort));

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

        if (state_ != nullptr)
        {
        state_->mqtt.connected = false;
        state_->mqtt.lastError = mqttState;
        state_->mqtt.connectionFailureCount++;
        }
        logger.errorf(
            "MQTT connection failed, state=%d",
            mqttClient.state());

        tlsClient.stop();

        return false;
    }

    mqttClient.publish(
        STATUS_TOPIC,
        "online",
        true);

    logger.info(
        "MQTT TLS connected");
    
    if (state_ != nullptr)
    {
    state_->mqtt.connected = true;
    state_->mqtt.lastError = 0;

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
        return;
    }
    
    if (mqttClient.connected())
    {
        mqttClient.loop();
        return;
    }

    const unsigned long now = millis();

    if (
        now - lastReconnectAttemptMs <
        RECONNECT_INTERVAL_MS)
    {
        return;
    }

    lastReconnectAttemptMs = now;

    tlsClient.stop();

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
        state.cellular.ipAddress;

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