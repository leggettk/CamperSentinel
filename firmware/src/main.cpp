#include <Arduino.h>
#include <esp_system.h>

#include "Button.h"
#include "Display.h"
#include "Modem.h"
#include "MQTT.h"
#include "Temperature.h"
#include "AppState.h"
#include "Logger.h"
#include "Battery.h"
#include "ShorePower.h"
#include "Config.h"
#include "SMS.h"
#include "settings.h"
#include "WiFiService.h"
#include "WebServerService.h"
#include "OTAService.h"
#include "EventLog.h"
#include "RenogyBatteries.h"

Settings settings;
AppState state;
Temperature temp;
WiFiService wifi;
WebServerService webServerService;
OTAService otaService;
//constexpr unsigned long TEMPERATURE_INTERVAL_MS = 5000;
constexpr unsigned long BATTERY_INTERVAL_MS = 60000;
//constexpr unsigned long MQTT_PUBLISH_INTERVAL_MS = 60000;

void updateTemperature(unsigned long now)
{
    static unsigned long lastUpdate = 0;

    if (now - lastUpdate < TEMPERATURE_INTERVAL_MS)
    {
        return;
    }

    lastUpdate = now;

    const float reading = temp.getFahrenheit();

    state.temperature.valid =
        reading > -100.0f &&
        reading < 200.0f;

    if (state.temperature.valid)
    {
        state.temperature.ambientF = reading;

        logger.telemetryf(
            "Ambient temperature: %.1f F",
            state.temperature.ambientF);
    }
    else
    {
        logger.warning("Invalid temperature reading");
    }
}

void updateBattery(unsigned long now)
{
    static unsigned long lastUpdate = 0;

    if (now - lastUpdate < BATTERY_INTERVAL_MS)
    {
        return;
    }

    lastUpdate = now;

    battery.update();

    state.power.batteryVoltage =
        battery.voltage();

    state.power.batteryVoltageValid =
        battery.isValid();

    if (state.power.batteryVoltageValid)
    {
        logger.telemetryf(
            "Camper battery: %.2f V",
            state.power.batteryVoltage);
    }
}

void updateCommunicationsState(unsigned long now)
{
    state.cellular.networkConnected =
        cellular.isNetworkConnected();

    state.cellular.dataConnected =
        cellular.isDataConnected();

    state.mqtt.connected =
        mqtt.isConnected();

    state.cellular.signalQuality =
        cellular.getSignalQuality();

    state.cellular.operatorName =
        cellular.getOperatorName();

    state.cellular.ipAddress =
        cellular.getIpAddress();

    state.system.uptimeSeconds =
        now / 1000UL;

    state.system.freeHeap =
        ESP.getFreeHeap();
}

void handleButton()
{
    userButton.update();

    if (userButton.wasPressed())
    {
        if (oled.isAwake())
        {
            oled.nextPage();
        }
        else
        {
            oled.wake();
        }

        logger.info("Display button pressed");
    }

    if (userButton.wasLongPressed())
    {
        logger.info("Long press detected");

        oled.status(
            "Camper Monitor",
            "MQTT reconnect",
            "Coming soon");
    }
}

void publishTelemetry(unsigned long now)
{
    static unsigned long lastPublish = 0;
    const DeviceSettings& config = settings.get();
    if (
        !state.temperature.valid ||
        now - lastPublish <
            (config.temperaturePublishIntervalSeconds * 1000UL))
    {
        return;
    }

    lastPublish = now;

    if (state.mqtt.connected)
    {
        mqtt.publishAmbient(
            state.temperature.ambientF);
    }
}

void updateShorePower()
{
    static bool initialized = false;
    const DeviceSettings& config = settings.get();
    shorePower.update();

    state.power.shorePowerPresent =
        shorePower.isPresent();

    // Prevent a false "power lost" SMS during startup.
    if (!initialized)
    {
        initialized = true;

        state.alarms.shorePower =
            !state.power.shorePowerPresent;

        return;
    }

    if (!shorePower.changed())
    {
        return;
    }

    mqtt.publishShorePower(
        state.power.shorePowerPresent);

    if (!state.power.shorePowerPresent)
    {
        logger.warning("Shore power lost");

        state.alarms.shorePower = true;

        if (config.smsEnabled)
        {
            state.sms.lastSuccessful =
                sms.sendShorePowerLost(
                    state.power.batteryVoltage,
                    state.power.batteryVoltageValid);

            if (state.sms.lastSuccessful)
            {
                state.sms.lastSentTimeSeconds =
                    millis() / 1000UL;
            }
        }
        else
        {
            state.sms.lastSuccessful = false;

            logger.info(
                "Shore power loss SMS skipped: SMS disabled");
        }
    }
    else
    {
        logger.info("Shore power restored");

        state.alarms.shorePower = false;

        if (config.smsEnabled)
        {
            state.sms.lastSuccessful =
                sms.sendShorePowerRestored();

            if (state.sms.lastSuccessful)
            {
                state.sms.lastSentTimeSeconds =
                    millis() / 1000UL;
            }
        }
        else
        {
            state.sms.lastSuccessful = false;

            logger.info(
                "Shore power restored SMS skipped: SMS disabled");
        }
    }
}

void updateTemperatureAlarm(unsigned long now)
{
    static unsigned long highTemperatureSince = 0;
    const DeviceSettings& config = settings.get();
    if (!state.temperature.valid)
    {
        highTemperatureSince = 0;
        return;
    }

    //
    // NO ACTIVE HIGH-TEMPERATURE ALARM
    //
    if (!state.alarms.highTemperature)
    {
        if (
            state.temperature.ambientF >=
            config.tempAlarmHighF)
        {
            if (highTemperatureSince == 0)
            {
                highTemperatureSince = now;

                logger.warning(
                    "High-temperature delay started");
            }

            if (
                now - highTemperatureSince >=
                TEMP_ALARM_DELAY_MS)
            {
                state.alarms.highTemperature = true;
                highTemperatureSince = 0;

                logger.errorf(
                    "High-temperature alarm: %.1f F",
                    state.temperature.ambientF);

                if (config.smsEnabled)
                {
                    state.sms.lastSuccessful =
                        sms.sendHighTemperature(
                            state.temperature.ambientF,
                            config.tempAlarmHighF);

                    if (state.sms.lastSuccessful)
                    {
                        state.sms.lastSentTimeSeconds =
                            now / 1000UL;
                    }
                }
                else
                {
                    state.sms.lastSuccessful = false;

                    logger.info(
                        "High-temperature SMS skipped: SMS disabled");
                }
            }
        }
        else
        {
            // Temperature dropped below the alarm
            // threshold before the delay expired.
            highTemperatureSince = 0;
        }

        //
        // IMPORTANT:
        // There is no active alarm to clear.
        //
        return;
    }

    //
    // HIGH-TEMPERATURE ALARM IS ACTIVE
    //
    // Only clear it after temperature falls below
    // the configured reset threshold.
    //
    if (
        state.temperature.ambientF <=
        config.tempAlarmResetF)
    {
        state.alarms.highTemperature = false;

        logger.infof(
            "Temperature alarm cleared: %.1f F",
            state.temperature.ambientF);

        if (config.smsEnabled)
        {
            state.sms.lastSuccessful =
                sms.sendTemperatureNormal(
                    state.temperature.ambientF);

            if (state.sms.lastSuccessful)
            {
                state.sms.lastSentTimeSeconds =
                    now / 1000UL;
            }
        }
        else
        {
            state.sms.lastSuccessful = false;

            logger.info(
                "Temperature alarm clear SMS skipped: SMS disabled");
        }
    }
}

void publishHeartbeat(unsigned long now)
{
    static unsigned long lastHeartbeat = 0;
    const DeviceSettings& config = settings.get();
    if (
        now - lastHeartbeat <
        (config.heartbeatIntervalSeconds * 1000UL))
    {
        return;
    }

    lastHeartbeat = now;

    if (state.mqtt.connected)
    {
        mqtt.publishHeartbeat(state);
    }
    else
    {
        logger.warning(
            "Heartbeat skipped: MQTT offline");
    }
}

void setup()
{   

    logger.begin(115200, LogLevel::Info);
    logger.infof("ESP32 reset reason: %d (1=power-on, 3=software, 4=panic, 5/6/7=watchdog, 9=brownout)",
        static_cast<int>(esp_reset_reason()));
    delay(500);

    battery.begin();
    shorePower.begin();

if (settings.begin())
{
    logger.info("Settings loaded");

    wifi.begin(settings, state, logger);
    webServerService.begin(settings, state, logger, otaService);

  const DeviceSettings& config = settings.get();

    logger.info(
        String("Device: ") + config.deviceName
    );

    logger.info(
        String("Hostname: ") + config.hostname
    );

    logger.info(
        String("WiFi configured: ") +
        (config.wifiSSID.isEmpty() ? "No" : "Yes")
    );
}
else
{
    logger.error("Settings initialization failed");
}

    if (!oled.begin())
    {
        logger.error("OLED initialization failed!");
    }

    userButton.begin();
    renogyBatteries.begin();

    if (!temp.begin())
    {
        logger.error("Temperature sensor not found!");
    }

    oled.bootScreen();
    delay(1500);

    if (cellular.begin())
    {
        logger.info("Modem OK");
    }
    else
    {
        logger.error("Modem FAILED");
    }

    if (mqtt.begin(settings, state))
    {
        logger.info("MQTT service initialized");
    }
    else
    {
        logger.warning("MQTT service initialization failed");
    }
}



void loop()
{
    const unsigned long now = millis();

    wifi.update();
    webServerService.update();
    mqtt.loop();
    renogyBatteries.update(state);
    static unsigned long lastLithiumPublish = 0;
    if (mqtt.isConnected() && now - lastLithiumPublish >= 30000UL)
    {
        lastLithiumPublish = now;
        mqtt.publishLithiumBatteries(state);
    }
    webServerService.update();
    handleButton();
    updateTemperature(now);
    updateBattery(now);
    updateCommunicationsState(now);
    updateShorePower();
    updateTemperatureAlarm(now);

    oled.update(state);

    publishTelemetry(now);
    publishHeartbeat(now);
}
