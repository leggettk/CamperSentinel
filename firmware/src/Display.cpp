#include "Display.h"

#include <Wire.h>
#include <esp_heap_caps.h>
#include "BatteryFreshness.h"

#include "Pins.h"
#include "Config.h"

Display oled;

void Display::drawHeader(const char *title)
{
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(title);

    display.drawLine(
        0,
        10,
        SCREEN_WIDTH - 1,
        10,
        SSD1306_WHITE);
}

uint8_t Display::signalBarCount(
    int signalQuality) const
{
    if (signalQuality == 99 || signalQuality <= 0)
    {
        return 0;
    }

    if (signalQuality <= 5)
    {
        return 1;
    }

    if (signalQuality <= 10)
    {
        return 2;
    }

    if (signalQuality <= 15)
    {
        return 3;
    }

    if (signalQuality <= 20)
    {
        return 4;
    }

    return 5;
}

void Display::drawSignalBars(
    int signalQuality,
    int16_t x,
    int16_t y)
{
    constexpr int16_t BAR_WIDTH = 3;
    constexpr int16_t BAR_GAP = 2;
    constexpr uint8_t BAR_COUNT = 5;

    const uint8_t activeBars =
        signalBarCount(signalQuality);

    for (uint8_t i = 0; i < BAR_COUNT; i++)
    {
        const int16_t height =
            3 + (i * 2);

        const int16_t barX =
            x + i * (BAR_WIDTH + BAR_GAP);

        const int16_t barY =
            y + 11 - height;

        if (i < activeBars)
        {
            display.fillRect(
                barX,
                barY,
                BAR_WIDTH,
                height,
                SSD1306_WHITE);
        }
        else
        {
            display.drawRect(
                barX,
                barY,
                BAR_WIDTH,
                height,
                SSD1306_WHITE);
        }
    }
}

void Display::drawConnectionIndicator(
    int16_t x,
    int16_t y,
    bool connected,
    const char *label)
{
    if (connected)
    {
        display.fillCircle(
            x + 3,
            y + 3,
            3,
            SSD1306_WHITE);
    }
    else
    {
        display.drawCircle(
            x + 3,
            y + 3,
            3,
            SSD1306_WHITE);
    }

    display.setCursor(x + 10, y);
    display.print(label);
}

bool Display::begin()
{
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

    if (!display.begin(
            SSD1306_SWITCHCAPVCC,
            OLED_ADDRESS,
            true,
            false))
    {
        return false;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    display.display();

    awake = true;
    resetActivityTimer();

    return true;
}

void Display::bootScreen()
{
    wake();

    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Camper");
    display.println("Monitor");

    display.setTextSize(1);
    display.println();
    display.println("Starting...");

    display.display();
}

void Display::update(const AppState &state)
{
    const unsigned long now = millis();

    if (
        awake &&
        now - lastActivityMs >= SCREEN_TIMEOUT_MS)
    {
        sleep();
        return;
    }

    if (!awake)
    {
        return;
    }

    if (now - lastRefreshMs < REFRESH_INTERVAL_MS)
    {
        return;
    }

    lastRefreshMs = now;

    switch (page)
    {
        case DisplayPage::Ambient:
            renderAmbient(state);
            break;

        case DisplayPage::Battery1:
            renderBattery(state, 0);
            break;

        case DisplayPage::Battery2:
            renderBattery(state, 1);
            break;

        case DisplayPage::LTE:
            renderLTE(state);
            break;

        case DisplayPage::Internet:
            renderInternet(state);
            break;

        case DisplayPage::System:
            renderSystem(state);
            break;

        default:
            page = DisplayPage::Ambient;
            renderAmbient(state);
            break;
    }
}

void Display::nextPage()
{
    wake();

    const uint8_t next =
        (static_cast<uint8_t>(page) + 1) % static_cast<uint8_t>(DisplayPage::Count);

    page = static_cast<DisplayPage>(next);
    lastRefreshMs = 0;
}

void Display::previousPage()
{
    wake();

    uint8_t current = static_cast<uint8_t>(page);

    if (current == 0)
    {
        current = static_cast<uint8_t>(DisplayPage::Count) - 1;
    }
    else
    {
        current--;
    }

    page = static_cast<DisplayPage>(current);
    lastRefreshMs = 0;
}

void Display::wake()
{
    if (!awake)
    {
        display.ssd1306_command(SSD1306_DISPLAYON);
        awake = true;
    }

    resetActivityTimer();
    lastRefreshMs = 0;
}

void Display::sleep()
{
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    awake = false;
}

bool Display::isAwake() const
{
    return awake;
}

DisplayPage Display::currentPage() const
{
    return page;
}

void Display::status(
    const String &line1,
    const String &line2,
    const String &line3)
{
    wake();

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);

    display.println(line1);
    display.println();
    display.println(line2);
    display.println(line3);

    display.display();
}

void Display::renderAmbient(
    const AppState &state)
{
    display.clearDisplay();

    drawHeader("Camper Monitor");

    display.setTextSize(1);

    display.setCursor(0, 16);
    display.print("TEMP");

    display.setCursor(47, 16);

    if (state.temperature.valid)
    {
        display.print(
            state.temperature.ambientF,
            1);

        display.print(" F");
    }
    else
    {
        display.print("--.- F");
    }

    display.setCursor(0, 28);
    display.print("BACKUP");

    display.setCursor(47, 28);

    if (state.power.batteryVoltageValid)
    {
        display.print(
            state.power.batteryVoltage,
            2);

        display.print(" V");
    }
    else
    {
        display.print("--.-- V");
    }

    display.setCursor(0, 40);
    display.print("POWER");

    display.setCursor(47, 40);
    display.print(
        state.power.shorePowerPresent
            ? "SHORE"
            : "BATTERY");

    display.setCursor(0, 53);
    display.print("LTE");

    drawSignalBars(
        state.cellular.signalQuality,
        47,
        51);

    display.setCursor(79, 53);

    if (state.cellular.signalQuality == 99)
    {
        display.print("--");
    }
    else
    {
        display.print(state.cellular.signalQuality);
    }

    display.display();
}

void Display::renderBattery(const AppState &state, uint8_t slot)
{
    const auto &battery = state.lithium[slot];
    const uint32_t now = millis();
    const bool fresh = batterySampleFresh(battery.valid, now, battery.updatedMs) && battery.capacityAh > 0;
    display.clearDisplay();
    drawHeader(slot == 0 ? "Renogy Battery 1" : "Renogy Battery 2");

    if (!fresh)
    {
        display.setCursor(0, 18);
        display.print(battery.address.isEmpty() ? "Not assigned" : "Data unavailable");
        display.setCursor(0, 31);
        // Keep diagnostics within the 21-character display width.
        if (battery.address.isEmpty()) display.print("Use battery settings");
        else if (battery.error.indexOf("disabled") >= 0 || battery.error.indexOf("Disabled") >= 0)
            display.print("Monitoring disabled");
        else if (battery.error.startsWith("Waiting")) display.print("Waiting for a poll");
        else display.print("Awaiting fresh data");
        display.setCursor(0, 46);
        display.print("Details on dashboard");
        display.display();
        return;
    }

    float charge = battery.remainingAh / battery.capacityAh * 100.0f;
    if (charge < 0) charge = 0;
    if (charge > 100) charge = 100;
    display.setTextSize(2);
    display.setCursor(0, 15);
    display.print(charge, 0);
    display.print("%");

    display.setTextSize(1);
    display.setCursor(72, 16);
    display.print("Age ");
    const uint32_t ageSeconds = uint32_t(now - battery.updatedMs) / 1000UL;
    if (ageSeconds < 60) { display.print(ageSeconds); display.print("s"); }
    else { display.print(ageSeconds / 60); display.print("m"); }
    display.setCursor(72, 25);
    // A failed poll may still have a recent cached sample. Do not call it live.
    display.print(battery.error.isEmpty() ? "OK" : "CACHED");

    display.setCursor(0, 35);
    display.print(battery.voltage, 1);
    display.print("V");
    display.setCursor(54, 35);
    display.print(battery.current, 2);
    display.print("A");

    display.setCursor(0, 45);
    display.print(battery.remainingAh, 1);
    display.print("/");
    display.print(battery.capacityAh, 1);
    display.print(" Ah");

    display.setCursor(0, 55);
    display.print("Temp ");
    if (battery.temperatureValid)
    {
        display.print(battery.temperatureC * 9.0f / 5.0f + 32.0f, 1);
        display.print("F");
    }
    else display.print("-- F");
    display.display();
}

void Display::renderLTE(
    const AppState &state)
{
    display.clearDisplay();

    drawHeader("Connectivity");

    drawConnectionIndicator(
        0,
        17,
        state.cellular.networkConnected,
        "LTE");

    drawConnectionIndicator(
        64,
        17,
        state.mqtt.connected,
        "MQTT");

    display.setCursor(0, 31);
    display.print("Signal");

    drawSignalBars(
        state.cellular.signalQuality,
        45,
        29);

    display.setCursor(78, 31);

    if (state.cellular.signalQuality == 99)
    {
        display.print("Unknown");
    }
    else
    {
        display.print(state.cellular.signalQuality);
    }

    display.setCursor(0, 45);
    display.print("Carrier:");

    display.setCursor(0, 55);

    String carrier = state.cellular.operatorName;

    if (carrier.length() > 20)
    {
        carrier =
            carrier.substring(0, 20);
    }

    display.print(carrier);

    display.display();
}

void Display::renderInternet(
    const AppState &state)
{
    display.clearDisplay();

    drawHeader("Network");

    display.setCursor(0, 16);
    display.print("WiFi: ");
    if (state.wifi.connected)
    {
        display.print(state.wifi.rssi);
        display.print(" dBm");
    }
    else display.print(state.wifi.accessPointActive ? "Setup AP" : "OFF");

    display.setCursor(0, 29);
    display.print("LTE data: ");
    display.print(state.cellular.dataConnected ? "ON" : "OFF");

    display.setCursor(0, 42);
    display.print("MQTT: ");
    display.print(state.mqtt.connected ? state.mqtt.transport : "OFFLINE");

    display.setCursor(0, 54);

    display.print("IP ");
    const String &ip = state.wifi.connected ? state.wifi.ipAddress
        : state.wifi.accessPointActive ? state.wifi.accessPointIP : state.cellular.ipAddress;
    display.print(ip);

    display.display();
}

void Display::renderSystem(
    const AppState &state)
{
    const unsigned long days =
        state.system.uptimeSeconds / 86400UL;

    const unsigned long hours =
        (state.system.uptimeSeconds % 86400UL) /
        3600UL;

    const unsigned long minutes =
        (state.system.uptimeSeconds % 3600UL) /
        60UL;

    display.clearDisplay();

    drawHeader("System");

    if (state.wifi.accessPointActive)
    {
        display.setCursor(0, 16);
        display.print(state.wifi.accessPointSSID);
        display.setCursor(0, 29);
        display.print("Pass: Camper-");
        display.print(state.wifi.accessPointSSID.substring(state.wifi.accessPointSSID.length() - 6));
        display.setCursor(0, 42);
        display.print(state.wifi.accessPointIP);
        display.setCursor(0, 55);
        display.print("Open /settings");
        display.display();
        return;
    }

    display.setCursor(0, 16);
    display.print("Firmware");

    display.setCursor(68, 16);
    display.print(FW_VERSION);

    display.setCursor(0, 29);
    display.print("Uptime");

    display.setCursor(50, 29);

    if (days > 0)
    {
        display.print(days);
        display.print("d ");
    }

    display.print(hours);
    display.print("h ");
    display.print(minutes);
    display.print("m");

    display.setCursor(0, 42);
    display.print("RAM free");

    display.setCursor(68, 42);
    display.print(
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024UL);

    display.print(" KB");

    display.setCursor(0, 55);
    display.print("Heartbeat");

    display.setCursor(68, 55);
    display.print(
        state.mqtt.connected
            ? "ACTIVE"
            : "OFFLINE");

    display.display();
}

void Display::resetActivityTimer()
{
    lastActivityMs = millis();
}
