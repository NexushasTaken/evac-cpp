#ifndef EVAC_STATUS_H
#define EVAC_STATUS_H

#include <functional>
#include <mutex>
#include <string>
#include <vector>

enum MessageType { MSG_INFO = 0, MSG_WARN = 1, MSG_ERROR = 2, MSG_DEBUG = 3 };

struct StatusMessage {
  MessageType type;
  std::string text;  // Already formatted, with timestamp and tag.
};

// Collects status messages from anywhere in the app (including worker threads).
// Until a listener is set, messages are kept in a buffer, and are handed to the
// listener as soon as it is set. The listener is called on the posting thread,
// so the UI is responsible for moving the message to its own thread.
class Status {
 public:
  typedef std::function<void(const StatusMessage&)> Listener;

  void post(const std::string& msg, MessageType type = MSG_INFO);
  void set_listener(const Listener& listener);
  void set_debug_enabled(bool enabled);

  // Exposed for tests.
  static std::string format(const std::string& msg, MessageType type,
                            const std::string& timestamp);

 private:
  std::mutex m_mutex;
  Listener m_listener;
  std::vector<StatusMessage> m_buffer;
  bool m_debug_enabled = false;
};

#endif
