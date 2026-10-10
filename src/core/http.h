#ifndef CATAPULT_HTTP_H
#define CATAPULT_HTTP_H

#include <atomic>
#include <string>
#include <vector>

#include "core/settings.h"
#include "core/status.h"

struct HttpResponse
{
    long code = 0;  // HTTP status code, 0 if the request never got an answer.
    std::string body;
};

// HTTP client on top of libcurl. Calls block until done, so use them from a
// worker thread. cancel() can be called from any thread to abort a transfer.
class Http
{
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
    bool get(const std::string& url, const std::vector<std::string>& headers, HttpResponse& response);

    // Downloads to target_dir/target_filename (target_dir is created if needed).
    // The file only appears under its final name when the download is complete.
    // Progress is posted to Status every couple of seconds.
    bool download_file(const std::string& url, const std::string& target_dir, const std::string& target_filename);

    void cancel();

    // Exposed for tests.
    static std::string format_progress(double downloaded, double total, long delta_msecs, double delta_bytes);

private:
    Status& status_;
    std::string proxy_host_;
    int proxy_port_ = 0;
    std::atomic<bool> cancel_;
};

#endif
