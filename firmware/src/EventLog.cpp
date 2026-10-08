#include "EventLog.h"

EventLog eventLog;

void EventLog::add(
    EventLevel level,
    const String& message)
{
    size_t index;

    if (count_ < MAX_ENTRIES)
    {
        index =
            (start_ + count_) %
            MAX_ENTRIES;

        count_++;
    }
    else
    {
        //
        // Buffer is full.
        // Overwrite the oldest entry.
        //
        index = start_;

        start_ =
            (start_ + 1) %
            MAX_ENTRIES;
    }

    entries_[index].uptimeSeconds =
        millis() / 1000UL;

    entries_[index].level =
        level;

    entries_[index].message =
        message;
}

size_t EventLog::count() const
{
    return count_;
}

const EventEntry& EventLog::get(
    size_t index) const
{
    //
    // index 0 = oldest entry
    //
    const size_t actualIndex =
        (start_ + index) %
        MAX_ENTRIES;

    return entries_[actualIndex];
}

void EventLog::clear()
{
    for (size_t i = 0;
         i < MAX_ENTRIES;
         ++i)
    {
        entries_[i].message = "";
        entries_[i].uptimeSeconds = 0;
        entries_[i].level =
            EventLevel::Info;
    }

    start_ = 0;
    count_ = 0;
}