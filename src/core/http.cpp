#include "core/http.h"

#include <cpr/cpr.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "core/fs.h"

namespace fs = std::filesystem;

// Give progress updates after this time or this amount downloaded, whichever
// comes first.
static const long PROGRESS_AFTER_MSECS = 2000;
static const double PROGRESS_AFTER_BYTES = 5.0 * 1024 * 1024;

static const char* const USER_AGENT = "Evac";

static long now_msecs() {
  using namespace std::chrono;
  return (long)duration_cast<milliseconds>(
             steady_clock::now().time_since_epoch())
      .count();
}

#ifndef _WIN32
// The curl under cpr, as built by Conan, doesn't know where the system
// certificates are.
static const char* find_ca_bundle() {
  static const char* const candidates[] = {
      "/etc/ssl/certs/ca-certificates.crt",  // Debian, Ubuntu, Arch
      "/etc/pki/tls/certs/ca-bundle.crt",    // Fedora, RHEL
      "/etc/ca-certificates/extracted/tls-ca-bundle.pem",  // Arch
      "/etc/ssl/ca-bundle.pem",                            // openSUSE
      "/etc/ssl/cert.pem",  // macOS, Alpine, BSDs
  };
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
    if (fs::exists(candidates[i])) return candidates[i];
  }
  return nullptr;
}
#endif

Http::Http(Status& status) : m_status(status), m_cancel(false) {}

Http::~Http() {}

void Http::set_proxy(const std::string& host, int port) {
  m_proxy_host = host;
  m_proxy_port = port;
}

void Http::apply_proxy_settings(Settings& settings, bool for_download) {
  std::string option = settings.get_string("proxy_option");
  if (option == "on" || (for_download && option == "download")) {
    std::string host = settings.get_string("proxy_host");
    int port = settings.get_int("proxy_port");
    m_status.post("Using proxy: " + host + ":" + std::to_string(port));
    set_proxy(host, port);
  } else {
    set_proxy("", 0);
  }
}

void Http::cancel() { m_cancel = true; }

// Options shared by all requests. Redirects are followed by default.
static void setup_common(cpr::Session& session, const std::string& url,
                         const std::string& proxy_host, int proxy_port) {
  session.SetUrl(cpr::Url{url});
  session.SetUserAgent(cpr::UserAgent{USER_AGENT});
  session.SetConnectTimeout(cpr::ConnectTimeout{std::chrono::seconds(30)});

#ifdef _WIN32
  // Windows has no bundle file. Setting any SSL options makes cpr use the
  // system certificate store (CURLSSLOPT_NATIVE_CA).
  session.SetSslOptions(cpr::SslOptions{});
#else
  const char* ca = find_ca_bundle();
  if (ca) session.SetSslOptions(cpr::Ssl(cpr::ssl::CaInfo{ca}));
#endif

  if (!proxy_host.empty() && proxy_port > 0) {
    std::string proxy = proxy_host + ":" + std::to_string(proxy_port);
    session.SetProxies(cpr::Proxies{{"http", proxy}, {"https", proxy}});
  }
}

bool Http::get(const std::string& url, const std::vector<std::string>& headers,
               HttpResponse& response) {
  m_cancel = false;
  response.code = 0;
  response.body.clear();

  cpr::Session session;
  setup_common(session, url, m_proxy_host, m_proxy_port);

  // "Name: value" lines.
  cpr::Header header;
  for (const std::string& line : headers) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    size_t value_start = line.find_first_not_of(' ', colon + 1);
    header[line.substr(0, colon)] =
        value_start == std::string::npos ? "" : line.substr(value_start);
  }
  session.SetHeader(header);

  cpr::Response r = session.Get();
  if (r.error) {
    m_status.post("HTTP request failed: " + r.error.message, MSG_ERROR);
    return false;
  }
  response.code = r.status_code;
  response.body = std::move(r.text);
  return true;
}

long long Http::remote_size(const std::string& url) {
  cpr::Session session;
  setup_common(session, url, m_proxy_host, m_proxy_port);
  session.SetTimeout(cpr::Timeout{std::chrono::seconds(30)});

  cpr::Response r = session.Head();
  if (r.error) {
    m_status.post("Could not get the download size: " + r.error.message,
                  MSG_WARN);
    return -1;
  }
  if (r.status_code >= 400) {
    m_status.post("Could not get the download size: HTTP " +
                      std::to_string(r.status_code),
                  MSG_WARN);
    return -1;
  }

  // After the redirects, these are the headers of the file itself.
  auto it = r.header.find("Content-Length");
  if (it == r.header.end()) return -1;
  char* end = nullptr;
  long long size = std::strtoll(it->second.c_str(), &end, 10);
  if (end == it->second.c_str() || size < 0) return -1;
  return size;
}

std::string Http::format_size(long long bytes) {
  char buf[32];
  if (bytes >= 1024LL * 1024 * 1024)
    std::snprintf(buf, sizeof(buf), "%.1f GB", bytes / 1073741824.0);
  else if (bytes >= 1024 * 1024)
    std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / 1048576.0);
  else
    std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
  return buf;
}

std::string Http::describe_download(Settings& settings,
                                    const fs::path& cache_dir,
                                    const std::string& url,
                                    const std::string& filename) {
  // The same check as before the downloads: a cached archive is used instead.
  std::error_code ec;
  fs::path cached = cache_dir / to_path(filename);
  if (!settings.get_bool("ignore_cache") && fs::exists(cached, ec)) {
    uintmax_t size = fs::file_size(cached, ec);
    return "Nothing to download: " + filename + " is already in the cache" +
           (ec ? std::string() : " (" + format_size((long long)size) + ")") +
           ".";
  }

  apply_proxy_settings(settings, true);
  long long size = remote_size(url);
  if (size < 0) return "Download size: unknown, the server does not say.";
  return "Download size: " + format_size(size) + ".";
}

std::string Http::format_progress(double downloaded, double total,
                                  long delta_msecs, double delta_bytes) {
  char buf[64];
  std::string result = "Download progress: ";

  if (downloaded > 1024 * 1024)
    std::snprintf(buf, sizeof(buf), "%.1f MB", downloaded / 1048576.0);
  else
    std::snprintf(buf, sizeof(buf), "%.1f KB", downloaded / 1024.0);
  result += buf;

  if (total > 0) {
    std::snprintf(buf, sizeof(buf), " (%.1f%%)", downloaded / total * 100.0);
    result += buf;
  }

  if (delta_msecs > 0) {
    double bps = delta_bytes / (double)delta_msecs * 1000.0;
    if (bps > 1048576.0)
      std::snprintf(buf, sizeof(buf), " at %.1f MB/s", bps / 1048576.0);
    else
      std::snprintf(buf, sizeof(buf), " at %d KB/s", (int)(bps / 1024.0));
    result += buf;
  }
  return result;
}

bool Http::download_file(const std::string& url, const fs::path& target_dir,
                         const std::string& target_filename) {
  m_cancel = false;

  std::error_code ec;
  fs::create_directories(target_dir, ec);
  if (ec) {
    m_status.post("Download of " + target_filename + " failed: cannot create " +
                      to_utf8(target_dir),
                  MSG_ERROR);
    return false;
  }

  fs::path final_path = target_dir / to_path(target_filename);
  fs::path part_path = final_path;
  part_path += ".part";

  m_status.post("Downloading " + target_filename);

  std::ofstream file(part_path, std::ios::binary | std::ios::trunc);
  if (!file) {
    m_status.post("Download of " + target_filename + " failed: cannot write " +
                      to_utf8(part_path),
                  MSG_ERROR);
    return false;
  }

  cpr::Session session;
  setup_common(session, url, m_proxy_host, m_proxy_port);

  long last_time = now_msecs();
  double last_bytes = 0;
  // Called by cpr while the transfer runs. Returning false aborts it.
  session.SetProgressCallback(cpr::ProgressCallback{
      [&](cpr::cpr_pf_arg_t total, cpr::cpr_pf_arg_t now, cpr::cpr_pf_arg_t,
          cpr::cpr_pf_arg_t, intptr_t) {
        if (m_cancel) return false;
        if (now < 1) return true;

        long delta_time = now_msecs() - last_time;
        double delta_bytes = (double)now - last_bytes;
        if (delta_time >= PROGRESS_AFTER_MSECS ||
            delta_bytes >= PROGRESS_AFTER_BYTES) {
          m_status.post(format_progress((double)now, (double)total, delta_time,
                                        delta_bytes));
          last_time = now_msecs();
          last_bytes = (double)now;
        }
        return true;
      }});

  cpr::Response r = session.Download(file);
  file.close();

  // cpr does not stop on a failed write or an HTTP error, so check both here.
  std::string error;
  if (r.error)
    error = r.error.message;
  else if (r.status_code >= 400)
    error = "HTTP " + std::to_string(r.status_code);
  else if (!file)
    error = "cannot write " + to_utf8(part_path);

  if (!error.empty()) {
    if (m_cancel)
      m_status.post("Download of " + target_filename + " was cancelled",
                    MSG_WARN);
    else
      m_status.post("Download of " + target_filename + " failed: " + error,
                    MSG_ERROR);
    fs::remove(part_path, ec);
    return false;
  }

  fs::rename(part_path, final_path, ec);
  if (ec) {
    m_status.post("Download of " + target_filename + " failed: " + ec.message(),
                  MSG_ERROR);
    return false;
  }

  m_status.post("Download of " + target_filename + " finished");
  return true;
}
