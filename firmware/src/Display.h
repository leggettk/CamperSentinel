#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AppState.h"

enum class DisplayPage : uint8_t
{
    Ambient = 0,
    Battery1,
    Battery2,
    LTE,
    Internet,
    System,
    Count
};

struct DisplayData
{
    float ambientTemperatureF = 0.0f;

    bool networkConnected = false;
    bool dataConnected = false;
    bool mqttConnected = false;

    String operatorName = "Unknown";
    String ipAddress = "No IP";

    int signalQuality = 99;
    unsigned long uptimeSeconds = 0;
};

class Display
{
public:
    bool begin();
    void bootScreen();

    void update(const AppState &state);

    void nextPage();
    void previousPage();

    void wake();
    void sleep();
    bool isAwake() const;

    DisplayPage currentPage() const;

    // Keep this available for temporary alerts and errors.
    void status(
        const String &line1,
        const String &line2,
        const String &line3);

private:
    void renderAmbient(const AppState &state);
    void renderBattery(const AppState &state, uint8_t slot);
    void renderLTE(const AppState &state);
    void renderInternet(const AppState &state);
    void renderSystem(const AppState &state);

    void resetActivityTimer();

    Adafruit_SSD1306 display =
        Adafruit_SSD1306(128, 64, &Wire, -1);

    DisplayPage page = DisplayPage::Ambient;

    bool awake = true;
    unsigned long lastActivityMs = 0;
    unsigned long lastRefreshMs = 0;

    static constexpr unsigned long REFRESH_INTERVAL_MS = 1000;
    static constexpr unsigned long SCREEN_TIMEOUT_MS = 300000;

void drawHeader(const char *title);
void drawSignalBars(int signalQuality, int16_t x, int16_t y);
void drawConnectionIndicator(
    int16_t x,
    int16_t y,
    bool connected,
    const char *label);

uint8_t signalBarCount(int signalQuality) const;
};

extern Display oled;
