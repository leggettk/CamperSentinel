#pragma once
#include "AppState.h"
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class RenogyBatteries {
public:
    void begin();
    void update(AppState& state);
    void registerRoutes(WebServer& server);
    String status();
private:
    struct Candidate { String name, address; };
    SemaphoreHandle_t mutex_ = nullptr;
    AppState::LithiumBattery readings_[2];
    Candidate candidates_[12];
    int candidateCount_ = 0;
    uint8_t deviceIds_[2] = {255, 255};
    bool enabled_ = true;
    bool ready_ = false;
    String status_ = "Starting Bluetooth";
    static void task(void* self);
    void run();
    void setStatus(const String& message);
    void page(WebServer& server);
    void save(WebServer& server);
};
extern RenogyBatteries renogyBatteries;
