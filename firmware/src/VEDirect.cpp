
#include "VEDirect.h"

#include <stdlib.h>
#include <string.h>

VEDirect vedirect;

// Working measurements for the frame being received.
// Only copied into the public readings after checksum validation.
namespace
{
    struct FrameData
    {
        float voltage = 0;
        float current = 0;
        float soc = 0;
        int power = 0;
        uint8_t fields = 0;
    };

    FrameData pending;

    bool checksumByteExpected = false;
    bool discardLineEnding = false;

    constexpr uint8_t FIELD_V = 0x01;
    constexpr uint8_t FIELD_I = 0x02;
    constexpr uint8_t FIELD_SOC = 0x04;
    constexpr uint8_t FIELD_P = 0x08;

    constexpr uint8_t REQUIRED_FIELDS =
        FIELD_V | FIELD_I | FIELD_SOC | FIELD_P;
}

void VEDirect::begin(uint8_t rxPin)
{
    serial.setRxBufferSize(512);

    // The 6N137 interface inverts the VE.Direct signal.
    serial.begin(19200, SERIAL_8N1, rxPin, -1, true);

    lineLength = 0;
    checksum = 0;
    frameValid = false;
    newData = false;
    lastFrameMs = 0;

    pending = FrameData{};
    checksumByteExpected = false;
    discardLineEnding = false;
}

void VEDirect::update()
{
    while (serial.available())
    {
        processByte(
            static_cast<uint8_t>(serial.read()));
    }
}

bool VEDirect::isValid() const
{
    return frameValid &&
           (millis() - lastFrameMs < 5000UL);
}

bool VEDirect::hasNewData()
{
    const bool result = newData;
    newData = false;
    return result;
}

float VEDirect::voltage() const
{
    return batteryVoltage;
}

float VEDirect::current() const
{
    return batteryCurrent;
}

float VEDirect::stateOfCharge() const
{
    return batterySOC;
}

int VEDirect::power() const
{
    return batteryPower;
}

void VEDirect::processByte(uint8_t value)
{
    // Ignore CR/LF immediately following a checksum byte.
    // These bytes belong to the next frame boundary.
    if (discardLineEnding)
    {
        discardLineEnding = false;
    }

    // The checksum is one raw binary byte, not ASCII text.
    if (checksumByteExpected)
    {
        checksum = static_cast<uint8_t>(
            checksum + value);

        if (checksum == 0 &&
            (pending.fields & REQUIRED_FIELDS) ==
                REQUIRED_FIELDS)
        {
            batteryVoltage = pending.voltage;
            batteryCurrent = pending.current;
            batterySOC = pending.soc;
            batteryPower = pending.power;

            frameValid = true;
            newData = true;
            lastFrameMs = millis();
        }

        pending = FrameData{};
        checksum = 0;
        lineLength = 0;
        checksumByteExpected = false;
        discardLineEnding = true;
        return;
    }

    checksum = static_cast<uint8_t>(
        checksum + value);

    // Detect the checksum field before its binary value.
    if (lineLength == 8 &&
    value == '\t' &&
    strncmp(lineBuffer, "Checksum", 8) == 0)
{
    checksumByteExpected = true;
    lineLength = 0;
    return;
}

    if (value == '\n')
    {
        lineBuffer[lineLength] = '\0';

        if (lineLength > 0 &&
            lineBuffer[lineLength - 1] == '\r')
        {
            lineBuffer[lineLength - 1] = '\0';
        }

        processLine(lineBuffer);
        lineLength = 0;
        return;
    }

    if (lineLength >= sizeof(lineBuffer) - 1)
    {
        lineLength = 0;
        checksum = 0;
        pending = FrameData{};
        return;
    }

    lineBuffer[lineLength++] =
        static_cast<char>(value);
}

void VEDirect::processLine(const char* line)
{
    const char* separator = strchr(line, '\t');

    if (separator == nullptr)
    {
        return;
    }

    const char* value = separator + 1;

    if (strncmp(line, "V\t", 2) == 0)
    {
        pending.voltage = atol(value) / 1000.0f;
        pending.fields |= FIELD_V;
    }
    else if (strncmp(line, "I\t", 2) == 0)
    {
        pending.current = atol(value) / 1000.0f;
        pending.fields |= FIELD_I;
    }
    else if (strncmp(line, "SOC\t", 4) == 0)
    {
        pending.soc = atol(value) / 10.0f;
        pending.fields |= FIELD_SOC;
    }
    else if (strncmp(line, "P\t", 2) == 0)
    {
        pending.power = atoi(value);
        pending.fields |= FIELD_P;
    }
}
