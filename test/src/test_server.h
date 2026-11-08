#pragma once
#include <httplib.h>

#include <string>
#include <thread>

class TestServer {
 public:
  httplib::Server svr;

  // Serves `root` at "/". Add custom handlers on `svr` before calling start().
  explicit TestServer(const std::string& root = "") {
    if (!root.empty()) {
      svr.set_mount_point("/", root);
    }
  }

  void start() {
    m_port = svr.bind_to_any_port("127.0.0.1");
    m_th = std::thread([this] { svr.listen_after_bind(); });
    svr.wait_until_ready();
  }

  ~TestServer() {
    svr.stop();
    if (m_th.joinable()) {
      m_th.join();
    }
  }

  std::string url(const std::string& path) const {
    return "http://127.0.0.1:" + std::to_string(m_port) + path;
  }

 private:
  int m_port = 0;
  std::thread m_th;
};
