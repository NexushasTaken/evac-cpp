#include "core/status.h"

#include <chrono>

static std::string current_timestamp() {
  namespace chr = std::chrono;
  const auto now = chr::system_clock::now();
  const auto secs = chr::floor<chr::seconds>(now);
  const auto milli = chr::duration_cast<chr::milliseconds>(now - secs);

  auto stamp = std::format("{:%H:%M:%S}.{:03}", secs, milli.count());
  return stamp;
}

std::string Status::format(const std::string& msg, MessageType type,
                           const std::string& timestamp) {
  switch (type) {
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

void Status::post(const std::string& msg, MessageType type) {
  std::scoped_lock lock(m_mutex);

  if (type == MSG_DEBUG && !m_debug_enabled) {
    return;
  }

  StatusMessage m;
  m.type = type;
  m.text = format(msg, type, current_timestamp());

  if (m_listener) {
    m_listener(m);
  } else {
    m_buffer.push_back(m);
  }
}

void Status::set_listener(const Listener& listener) {
  std::scoped_lock lock(m_mutex);
  m_listener = listener;
  if (m_listener) {
    for (const auto& i : m_buffer) {
      m_listener(i);
    }
    m_buffer.clear();
  }
}

void Status::set_debug_enabled(bool enabled) {
  std::scoped_lock lock(m_mutex);
  m_debug_enabled = enabled;
}
