#include "Settings.h"
#include "Secrets.h"

Settings::~Settings()
{
    end();
}

bool Settings::begin()
{
    if (ready_)
    {
        return true;
    }

    ready_ = preferences_.begin(NAMESPACE_NAME, false);

    if (!ready_)
    {
        return false;
    }

    return load();
}

bool Settings::load()
{
    if (!ready_)
    {
        return false;
    }

    const uint16_t storedVersion =
        preferences_.getUShort("version", 0);

    //
    // Brand-new device
    //
    if (storedVersion == 0)
    {
        data_ = DeviceSettings{};

        data_.wifiSSID =
            DEFAULT_WIFI_SSID;

        data_.wifiPassword =
            DEFAULT_WIFI_PASSWORD;

        //
        // Development/default MQTT settings.
        //
        data_.mqttHost =
            MQTT_HOST;

        data_.mqttPort =
            MQTT_PORT;

        data_.mqttUsername =
            MQTT_USER;

        data_.mqttPassword =
            MQTT_PASSWORD;

        data_.mqttEnabled = true;
        data_.mqttTlsEnabled = true;

        return save();
    }

    //
    // Load settings shared by v1 and v2.
    //
    data_.version = storedVersion;

    data_.deviceName =
        preferences_.getString(
            "deviceName",
            "Camper Sentinel");

    data_.hostname =
        preferences_.getString(
            "hostname",
            "camper-sentinel");

    data_.wifiEnabled =
        preferences_.getBool(
            "wifiEnabled",
            true);

    if (preferences_.isKey("wifiSSID"))
    {
        data_.wifiSSID =
            preferences_.getString("wifiSSID");
    }
    else
    {
        data_.wifiSSID =
            DEFAULT_WIFI_SSID;
    }

    if (preferences_.isKey("wifiPass"))
    {
        data_.wifiPassword =
            preferences_.getString("wifiPass");
    }
    else
    {
        data_.wifiPassword =
            DEFAULT_WIFI_PASSWORD;
    }

    data_.smsEnabled =
        preferences_.getBool(
            "smsEnabled",
            true);

    data_.tempAlarmHighF =
        preferences_.getFloat(
            "tempHighF",
            DEFAULT_TEMP_ALARM_HIGH_F);

    data_.tempAlarmResetF =
        preferences_.getFloat(
            "tempResetF",
            DEFAULT_TEMP_ALARM_RESET_F);

    data_.heartbeatIntervalSeconds =
        preferences_.getULong(
            "heartbeatSec",
            DEFAULT_HEARTBEAT_INTERVAL_SECONDS);

    data_.temperaturePublishIntervalSeconds =
        preferences_.getULong(
            "tempPubSec",
            DEFAULT_TEMPERATURE_PUBLISH_SECONDS);

    bool needsSave = false;

    //
    // MQTT settings introduced in version 2.
    //
    if (storedVersion < 2)
    {
        data_.mqttEnabled = true;

        data_.mqttHost =
            MQTT_HOST;

        data_.mqttPort =
            MQTT_PORT;

        data_.mqttUsername =
            MQTT_USER;

        data_.mqttPassword =
            MQTT_PASSWORD;

        data_.mqttTlsEnabled = true;

        needsSave = true;
    }
    else
    {
        data_.mqttEnabled =
            preferences_.getBool(
                "mqttEnabled",
                true);

        if (preferences_.isKey("mqttHost"))
        {
            data_.mqttHost =
                preferences_.getString("mqttHost");
        }
        else
        {
            data_.mqttHost =
                MQTT_HOST;

            needsSave = true;
        }

        data_.mqttPort =
            preferences_.getUShort(
                "mqttPort",
                MQTT_PORT);

        if (preferences_.isKey("mqttUser"))
        {
            data_.mqttUsername =
                preferences_.getString("mqttUser");
        }
        else
        {
            data_.mqttUsername =
                MQTT_USER;

            needsSave = true;
        }

        if (preferences_.isKey("mqttPass"))
        {
            data_.mqttPassword =
                preferences_.getString("mqttPass");
        }
        else
        {
            data_.mqttPassword =
                MQTT_PASSWORD;

            needsSave = true;
        }

        data_.mqttTlsEnabled =
            preferences_.getBool(
                "mqttTls",
                true);
    }

    //
    // Missing Wi-Fi keys should also be persisted.
    //
    if (!preferences_.isKey("wifiSSID") ||
        !preferences_.isKey("wifiPass"))
    {
        needsSave = true;
    }

    //
    // Correct invalid values.
    //
    if (!validate())
    {
        needsSave = true;
    }

    //
    // Upgrade persisted version.
    //
    if (storedVersion !=
        DeviceSettings::CURRENT_VERSION)
    {
        data_.version =
            DeviceSettings::CURRENT_VERSION;

        needsSave = true;
    }

    if (needsSave)
    {
        return save();
    }

    return true;
}

bool Settings::save()
{
    if (!ready_)
    {
        return false;
    }

    validate();
    data_.version = DeviceSettings::CURRENT_VERSION;

    bool success = true;

    success &= preferences_.putUShort(
        "version",
        data_.version) > 0;

    success &= preferences_.putString(
        "deviceName",
        data_.deviceName) > 0;

    success &= preferences_.putString(
        "hostname",
        data_.hostname) > 0;

    //
    // Wi-Fi
    //
    success &= preferences_.putBool(
        "wifiEnabled",
        data_.wifiEnabled) > 0;

    if (data_.wifiSSID.isEmpty())
    {
        preferences_.remove("wifiSSID");
    }
    else
    {
        success &= preferences_.putString(
            "wifiSSID",
            data_.wifiSSID) > 0;
    }

    if (data_.wifiPassword.isEmpty())
    {
        preferences_.remove("wifiPass");
    }
    else
    {
        success &= preferences_.putString(
            "wifiPass",
            data_.wifiPassword) > 0;
    }

    //
    // MQTT
    //
    success &= preferences_.putBool(
        "mqttEnabled",
        data_.mqttEnabled) > 0;

    if (data_.mqttHost.isEmpty())
    {
        preferences_.remove("mqttHost");
    }
    else
    {
        success &= preferences_.putString(
            "mqttHost",
            data_.mqttHost) > 0;
    }

    success &= preferences_.putUShort(
        "mqttPort",
        data_.mqttPort) > 0;

    if (data_.mqttUsername.isEmpty())
    {
        preferences_.remove("mqttUser");
    }
    else
    {
        success &= preferences_.putString(
            "mqttUser",
            data_.mqttUsername) > 0;
    }

    if (data_.mqttPassword.isEmpty())
    {
        preferences_.remove("mqttPass");
    }
    else
    {
        success &= preferences_.putString(
            "mqttPass",
            data_.mqttPassword) > 0;
    }

    success &= preferences_.putBool(
        "mqttTls",
        data_.mqttTlsEnabled) > 0;

    //
    // Alerts
    //
    success &= preferences_.putBool(
        "smsEnabled",
        data_.smsEnabled) > 0;

    success &= preferences_.putFloat(
        "tempHighF",
        data_.tempAlarmHighF) > 0;

    success &= preferences_.putFloat(
        "tempResetF",
        data_.tempAlarmResetF) > 0;

    //
    // Reporting
    //
    success &= preferences_.putULong(
        "heartbeatSec",
        data_.heartbeatIntervalSeconds) > 0;

    success &= preferences_.putULong(
        "tempPubSec",
        data_.temperaturePublishIntervalSeconds) > 0;

    return success;
}

bool Settings::resetDefaults(bool saveNow)
{
    data_ = DeviceSettings{};

    data_.wifiSSID = DEFAULT_WIFI_SSID;
    data_.wifiPassword = DEFAULT_WIFI_PASSWORD;

    if (!saveNow)
    {
        return true;
    }

    if (!ready_)
    {
        return false;
    }

    if (!preferences_.clear())
    {
        return false;
    }

    return save();
}

void Settings::end()
{
    if (!ready_)
    {
        return;
    }

    preferences_.end();
    ready_ = false;
}

DeviceSettings& Settings::get()
{
    return data_;
}

const DeviceSettings& Settings::get() const
{
    return data_;
}

bool Settings::isReady() const
{
    return ready_;
}

bool Settings::validate()
{
    bool alreadyValid = true;

    data_.deviceName.trim();
    if (data_.deviceName.isEmpty())
    {
        data_.deviceName = "Camper Sentinel";
        alreadyValid = false;
    }

    data_.hostname.trim();
    data_.hostname.toLowerCase();
    data_.hostname.replace(" ", "-");

    if (data_.hostname.isEmpty())
    {
        data_.hostname = "camper-sentinel";
        alreadyValid = false;
    }

    if (data_.tempAlarmHighF < 40.0f ||
        data_.tempAlarmHighF > 150.0f)
    {
        data_.tempAlarmHighF = 90.0f;
        alreadyValid = false;
    }

    if (data_.tempAlarmResetF < 35.0f ||
        data_.tempAlarmResetF >= data_.tempAlarmHighF)
    {
        data_.tempAlarmResetF = 85.0f;
        alreadyValid = false;
    }

    if (data_.heartbeatIntervalSeconds < 60 ||
        data_.heartbeatIntervalSeconds > 86400)
    {
        data_.heartbeatIntervalSeconds = 300;
        alreadyValid = false;
    }

    if (data_.temperaturePublishIntervalSeconds < 30 ||
        data_.temperaturePublishIntervalSeconds > 86400)
    {
        data_.temperaturePublishIntervalSeconds = 180;
        alreadyValid = false;
    }
    if (data_.mqttPort == 0)
    {
        data_.mqttPort = 8883;
        alreadyValid = false;
    }

    data_.mqttHost.trim();
    data_.mqttUsername.trim();
    return alreadyValid;
}