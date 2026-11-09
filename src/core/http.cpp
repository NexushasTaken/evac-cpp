#include "core/http.h"

#include <boost/predef.h>
#include <cpr/cprtypes.h>
#include <cpr/response.h>
#include <cpr/session.h>
#include <cpr/ssl_options.h>
#include <zoe/zoe.h>

#include <array>
#include <boost/algorithm/string/trim.hpp>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "core/fs.h"
#include "core/settings.h"
#include "core/status.h"

namespace fs = std::filesystem;

static const char* const USER_AGENT = "Evac";

#if !BOOST_OS_WINDOWS
// The curl under cpr, as built by Conan, doesn't know where the system
// certificates are.
static const char* find_ca_bundle() {
  const std::array<const char*, 5> candidates = {
      "/etc/ssl/certs/ca-certificates.crt",  // Debian, Ubuntu, Arch
      "/etc/pki/tls/certs/ca-bundle.crt",    // Fedora, RHEL
      "/etc/ca-certificates/extracted/tls-ca-bundle.pem",  // Arch
      "/etc/ssl/ca-bundle.pem",                            // openSUSE
      "/etc/ssl/cert.pem",  // macOS, Alpine, BSDs
  };
  for (const auto* candidate : candidates) {
    if (fs::exists(candidate)) {
      return candidate;
    }
  }
  return nullptr;
}
#endif

Http::Http(Status& status) : m_status(status), m_cancel(false) {}

Http::~Http() = default;

void Http::set_proxy(const std::string& host, int port) {
  m_proxy_host = host;
  m_proxy_port = port;
}

void Http::apply_proxy_settings(Settings& settings, bool for_download) {
  const std::string option = settings.get_string("proxy_option");
  if (option == "on" || (for_download && option == "download")) {
    const std::string host = settings.get_string("proxy_host");
    const int port = settings.get_int("proxy_port");
    m_status.post("Using proxy: " + host + ":" + std::to_string(port));
    set_proxy(host, port);
  } else {
    set_proxy("", 0);
  }
}

void Http::cancel() {
  m_cancel = true;
  m_zoe.stop();
}

// Options shared by all requests. Redirects are followed by default.
static void setup_common(cpr::Session& session, const std::string& url,
                         Proxy proxy) {
  session.SetUrl(cpr::Url{url});
  session.SetUserAgent(cpr::UserAgent{USER_AGENT});
  session.SetConnectTimeout(cpr::ConnectTimeout{std::chrono::seconds(30)});

#if BOOST_OS_WINDOWS
  // Windows has no bundle file. Setting any SSL options makes cpr use the
  // system certificate store (CURLSSLOPT_NATIVE_CA).
  session.SetSslOptions(cpr::SslOptions{});
#else
  const char* ca = find_ca_bundle();
  if (ca != nullptr) {
    session.SetSslOptions(cpr::Ssl(cpr::ssl::CaInfo{ca}));
  }
#endif

  if (!proxy.host.empty() && proxy.port > 0) {
    std::string proxy_url =
        std::format("{}:{}", proxy.host, std::to_string(proxy.port));
    session.SetProxies(cpr::Proxies{{"http", proxy_url}, {"https", proxy_url}});
  }
}

static void setup_common(zoe::Zoe& zoe, const std::string& proxy_host,
                         int proxy_port) {
#if !BOOST_OS_WINDOWS
  zoe.setVerifyCAEnabled(true, find_ca_bundle());
#endif

  if (!proxy_host.empty() && proxy_port > 0) {
    const std::string proxy = proxy_host + ":" + std::to_string(proxy_port);
    zoe.setProxy(proxy);
  } else {
    zoe.setProxy("");
  }
}

bool Http::get(const std::string& url, const std::vector<std::string>& headers,
               HttpResponse& response) {
  m_cancel = false;
  response.code = 0;
  response.body.clear();

  cpr::Session session;
  setup_common(session, url, {.host = m_proxy_host, .port = m_proxy_port});

  // "Name: value" lines.
  cpr::Header header;
  for (const std::string& line : headers) {
    const size_t colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    const size_t value_start = line.find(' ', colon + 1);
    std::string name = line.substr(0, colon);
    std::string value = line.substr(value_start);
    boost::algorithm::trim(name);
    boost::algorithm::trim(value);
    header[name] = value_start == std::string::npos ? "" : value;
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

int64_t Http::remote_size(const std::string& url) {
  cpr::Session session;
  setup_common(session, url, {.host = m_proxy_host, .port = m_proxy_port});
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
  const auto it = r.header.find("Content-Length");
  if (it == r.header.end()) {
    return -1;
  }
  char* end = nullptr;
  constexpr int base = 10;
  const int64_t size = std::strtoll(it->second.c_str(), &end, base);
  if (end == it->second.c_str() || size < 0) {
    return -1;
  }
  return size;
}

std::string Http::format_size(double bytes) {
  constexpr double KiB = 1024.0;
  static constexpr std::array<std::string, 5> units = {
      "B", "KiB", "MiB", "GiB", "TiB",
  };

  size_t i = 0;
  while (bytes >= KiB && i < units.size() - 1) {
    bytes /= KiB;
    ++i;
  }
  return i == 0 ? std::format("{} B", static_cast<int64_t>(bytes))
                : std::format("{:.2f} {}", bytes, units.at(i));
}

std::string Http::describe_download(Settings& settings,
                                    const fs::path& cache_dir,
                                    const std::string& url,
                                    const std::string& filename) {
  // The same check as before the downloads: a cached archive is used instead.
  std::error_code ec;
  const fs::path cached = cache_dir / to_path(filename);
  if (!settings.get_bool("ignore_cache") && fs::exists(cached, ec)) {
    const uintmax_t size = fs::file_size(cached, ec);
    return "Nothing to download: " + filename + " is already in the cache" +
           (ec ? std::string()
               : " (" + format_size(static_cast<double>(size)) + ")") +
           ".";
  }

  apply_proxy_settings(settings, true);
  const int64_t size = remote_size(url);
  if (size < 0) {
    return "Download size: unknown, the server does not say.";
  }
  return "Download size: " + format_size(static_cast<double>(size)) + ".";
}

double Http::calc_bps(int64_t delta_msecs, double delta_bytes) {
  constexpr double MSECS_PER_SEC = 1000.0;

  if (delta_msecs <= 0) {
    return 0.0;
  }

  const double bps =
      delta_bytes / static_cast<double>(delta_msecs) * MSECS_PER_SEC;
  return bps;
}

std::string Http::format_progress(Progress progress) {
  std::string result = "Download progress: " +
                       format_size(static_cast<double>(progress.downloaded));
  if (progress.total > 0) {
    result += std::format(" ({:.2f}%)",
                          static_cast<double>(progress.downloaded) /
                              static_cast<double>(progress.total) * 100.0);
  }
  result += " at " + format_size(progress.bps) + "/s";
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

  const fs::path final_path = target_dir / to_path(target_filename);

  m_status.post("Downloading " + target_filename);

  // z.setVerboseOutput([&](const utf8string& verbose) {
  //   m_status.post("Error downloading " + target_filename + ": " + verbose);
  // });

  setup_common(m_zoe, m_proxy_host, m_proxy_port);
  const auto future = m_zoe.start(
      url, final_path,
      [&](zoe::ZoeResult result) {
        switch (result) {
          case zoe::ZoeResult::SUCCESSED:
            m_status.post("Download of " + target_filename + " finished");
            break;
          case zoe::ZoeResult::CANCELED:
            m_status.post("Download of " + target_filename + " was canceled",
                          MSG_WARN);
            break;
          default:
            m_status.post("Error downloading " + target_filename + ": " +
                          zoe::Zoe::GetResultString(result));
            break;
        }
      },
      [&](int64_t total, int64_t downloaded) {
        if (total > 0) {
          m_status.post(format_progress(
              {.downloaded = downloaded, .total = total, .bps = 0}));
        }
      },
      [&](int64_t bytes_per_second) {
        m_status.post(
            format_progress({.downloaded = 0,
                             .total = 0,
                             .bps = static_cast<double>(bytes_per_second)}));
      });
  future.wait();

  return future.get() == zoe::ZoeResult::SUCCESSED;
}
