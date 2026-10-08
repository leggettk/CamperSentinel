#pragma once

#include <Arduino.h>

enum class EventLevel
{
    Info,
    Warning,
    Error
};

struct EventEntry
{
    uint32_t uptimeSeconds = 0;
    EventLevel level = EventLevel::Info;
    String message;
};

class EventLog
{
public:
    static constexpr size_t MAX_ENTRIES = 100;

    void add(
        EventLevel level,
        const String& message);

    size_t count() const;

    const EventEntry& get(
        size_t index) const;

    void clear();

private:
    EventEntry entries_[MAX_ENTRIES];

    size_t start_ = 0;
    size_t count_ = 0;
};

extern EventLog eventLog;