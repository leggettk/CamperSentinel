#pragma once

 

#include <Arduino.h>

 

enum class LogLevel : uint8_t

{

    Debug = 0,

    Info,

    Warning,

    Error,

    None

};

 

class Logger

{

public:

    void begin(

        unsigned long baudRate = 115200,

        LogLevel minimumLevel = LogLevel::Info);

 

    void setLevel(LogLevel minimumLevel);

 

    void debug(const String &message);

    void info(const String &message);

    void warning(const String &message);

    void error(const String &message);

 

    void debugf(const char *format, ...);

    void infof(const char *format, ...);

    void warningf(const char *format, ...);

    void errorf(const char *format, ...);
    void telemetry(const String& message);
    void telemetryf(const char* format, ...);
 

private:

    void write(LogLevel level, const String &message);

    void writeFormatted(

        LogLevel level,

        const char *format,

        va_list arguments);

 

    const char *levelName(LogLevel level) const;
    
    void writeSerial(
    LogLevel level,
    const String& message);
 

    LogLevel currentLevel = LogLevel::Info;

};

 

extern Logger logger;