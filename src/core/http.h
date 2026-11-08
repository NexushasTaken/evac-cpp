#ifndef EVAC_HTTP_H
#define EVAC_HTTP_H

#include <zoe/zoe.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

#include "core/settings.h"
#include "core/status.h"

struct HttpResponse {
  int code = 0;  // HTTP status code, 0 if the request never got an answer.
  std::string body;
};

struct Progress {
  int64_t downloaded;
  int64_t total;
  double bps;
};

struct Proxy {
  std::string_view host;
  int port;
};

// HTTP client on top of cpr (which wraps libcurl). Calls block until done, so
// use them from a worker thread. cancel() can be called from any thread to
// abort a transfer.
class Http {
 public:
  explicit Http(Status& status);
  ~Http();

  // An empty host or a port <= 0 means no proxy.
  void set_proxy(const std::string& host, int port);

  // Sets the proxy from the settings. Downloads use it when proxy_option is
  // "on" or "download", other requests only when it is "on".
  void apply_proxy_settings(Settings& settings, bool for_download);

  // GET into memory. Returns false if the request could not be made; an
  // HTTP error code is not a failure here, check response.code.
  bool get(const std::string& url, const std::vector<std::string>& headers,
           HttpResponse& response);

  // Downloads to target_dir/target_filename (target_dir is created if needed).
  // The file only appears under its final name when the download is complete.
  // Progress is posted to Status every couple of seconds.
  bool download_file(const std::string& url,
                     const std::filesystem::path& target_dir,
                     const std::string& target_filename);

  // Size in bytes of the file at url, from a HEAD request, or -1 if the
  // server does not say (GitHub's branch archives, for one) or cannot be
  // reached.
  int64_t remote_size(const std::string& url);

  // A line for the confirmation before downloading url to
  // cache_dir/filename: the download size, or that the cached archive is
  // used (as the downloads do unless "ignore_cache" is set).
  std::string describe_download(Settings& settings,
                                const std::filesystem::path& cache_dir,
                                const std::string& url,
                                const std::string& filename);

  void cancel();

  static double calc_bps(int64_t delta_msecs, double delta_bytes);
  // "1.20 GiB", "34.50 MiB", "12.00 KiB".
  static std::string format_size(double bytes);
  static std::string format_progress(Progress progress);

 private:
  zoe::Zoe m_zoe;
  Status& m_status;
  std::string m_proxy_host;
  int m_proxy_port = 0;
  std::atomic<bool> m_cancel;
};

#endif
