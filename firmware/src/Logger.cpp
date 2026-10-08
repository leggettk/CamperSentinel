#include "Logger.h"
#include "EventLog.h"

 

#include <cstdarg>

 

Logger logger;

 

void Logger::begin(

    unsigned long baudRate,

    LogLevel minimumLevel)

{

    Serial.begin(baudRate);

    currentLevel = minimumLevel;

 

    delay(200);

 

    info("Logger initialized");

}

 

void Logger::setLevel(LogLevel minimumLevel)

{

    currentLevel = minimumLevel;

}

 

void Logger::debug(const String &message)

{

    write(LogLevel::Debug, message);

}

 

void Logger::info(const String &message)

{

    write(LogLevel::Info, message);

}

 

void Logger::warning(const String &message)

{

    write(LogLevel::Warning, message);

}

 

void Logger::error(const String &message)

{

    write(LogLevel::Error, message);

}

 

void Logger::debugf(const char *format, ...)

{

    va_list arguments;

    va_start(arguments, format);

    writeFormatted(LogLevel::Debug, format, arguments);

    va_end(arguments);

}

 

void Logger::infof(const char *format, ...)

{

    va_list arguments;

    va_start(arguments, format);

    writeFormatted(LogLevel::Info, format, arguments);

    va_end(arguments);

}

 

void Logger::warningf(const char *format, ...)

{

    va_list arguments;

    va_start(arguments, format);

    writeFormatted(LogLevel::Warning, format, arguments);

    va_end(arguments);

}

 

void Logger::errorf(const char *format, ...)

{

    va_list arguments;

    va_start(arguments, format);

    writeFormatted(LogLevel::Error, format, arguments);

    va_end(arguments);

}

 

void Logger::write(
    LogLevel level,
    const String& message)
{
    if (
        currentLevel == LogLevel::None ||
        static_cast<uint8_t>(level) <
            static_cast<uint8_t>(currentLevel))
    {
        return;
    }

    writeSerial(
        level,
        message);

    switch (level)
    {
        case LogLevel::Warning:
            eventLog.add(
                EventLevel::Warning,
                message);
            break;

        case LogLevel::Error:
            eventLog.add(
                EventLevel::Error,
                message);
            break;

        case LogLevel::Debug:
        case LogLevel::Info:
        default:
            eventLog.add(
                EventLevel::Info,
                message);
            break;
    }
}

 

void Logger::writeFormatted(

    LogLevel level,

    const char *format,

    va_list arguments)

{

    char buffer[192];

 

    vsnprintf(

        buffer,

        sizeof(buffer),

        format,

        arguments);

 

    write(level, String(buffer));

}

 

const char *Logger::levelName(LogLevel level) const

{

    switch (level)

    {

        case LogLevel::Debug:

            return "DEBUG";

 

        case LogLevel::Info:

            return "INFO";

 

        case LogLevel::Warning:

            return "WARN";

 

        case LogLevel::Error:

            return "ERROR";

 

        case LogLevel::None:

            return "NONE";

    }

 

    return "UNKNOWN";

}
void Logger::telemetry(
    const String& message)
{
    writeSerial(
        LogLevel::Info,
        message);
}

void Logger::telemetryf(
    const char* format,
    ...)
{
    char buffer[192];

    va_list arguments;
    va_start(arguments, format);

    vsnprintf(
        buffer,
        sizeof(buffer),
        format,
        arguments);

    va_end(arguments);

    writeSerial(
        LogLevel::Info,
        String(buffer));
}

void Logger::writeSerial(
    LogLevel level,
    const String& message)
{
    if (
        currentLevel == LogLevel::None ||
        static_cast<uint8_t>(level) <
            static_cast<uint8_t>(currentLevel))
    {
        return;
    }

    const unsigned long seconds =
        millis() / 1000UL;

    Serial.print("[");
    Serial.print(seconds);
    Serial.print("s] [");
    Serial.print(levelName(level));
    Serial.print("] ");
    Serial.println(message);
}