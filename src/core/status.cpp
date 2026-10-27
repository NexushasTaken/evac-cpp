#include "core/status.h"

#include <chrono>
#include <cstdio>
#include <ctime>

#include "core/platform.h"

static std::string current_timestamp()
{
    using namespace std::chrono;
    system_clock::time_point now = system_clock::now();
    std::time_t secs = system_clock::to_time_t(now);
    long msecs = (long)(duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000);

    std::tm local = platform::local_time(secs);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "[%02d:%02d:%02d.%03ld]", local.tm_hour, local.tm_min, local.tm_sec, msecs);
    return buf;
}

std::string Status::format(const std::string& msg, MessageType type, const std::string& timestamp)
{
    switch (type)
    {
    case MSG_WARN:
        return timestamp + " [Warning] " + msg;
    case MSG_ERROR:
        return timestamp + " [Error] " + msg;
    case MSG_DEBUG:
        return timestamp + " [Debug] " + msg;
    default:
        return timestamp + " " + msg;
    }
}

void Status::post(const std::string& msg, MessageType type)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (type == MSG_DEBUG && !debug_enabled_)
        return;

    StatusMessage m;
    m.type = type;
    m.text = format(msg, type, current_timestamp());

    if (listener_)
        listener_(m);
    else
        buffer_.push_back(m);
}

void Status::set_listener(const Listener& listener)
{
    std::lock_guard<std::mutex> lock(mutex_);
    listener_ = listener;
    if (listener_)
    {
        for (size_t i = 0; i < buffer_.size(); ++i)
            listener_(buffer_[i]);
        buffer_.clear();
    }
}

void Status::set_debug_enabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(mutex_);
    debug_enabled_ = enabled;
}
