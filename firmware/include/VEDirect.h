
#pragma once

#include <Arduino.h>

class VEDirect
{
public:
    void begin(uint8_t rxPin);
    void update();

    bool isValid() const;
    bool hasNewData();

    float voltage() const;
    float current() const;
    float stateOfCharge() const;
    int power() const;

private:
    HardwareSerial serial{2};

    char lineBuffer[96] = {};
    size_t lineLength = 0;

    uint8_t checksum = 0;
    bool frameValid = false;
    bool newData = false;
    unsigned long lastFrameMs = 0;

    float batteryVoltage = 0;
    float batteryCurrent = 0;
    float batterySOC = 0;
    int batteryPower = 0;

    void processByte(uint8_t value);
    void processLine(const char* line);
};

extern VEDirect vedirect;
