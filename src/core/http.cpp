#include "core/http.h"

#include <curl/curl.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Give progress updates after this time or this amount downloaded, whichever comes first.
static const long PROGRESS_AFTER_MSECS = 2000;
static const double PROGRESS_AFTER_BYTES = 5.0 * 1024 * 1024;

static const char* const USER_AGENT = "Catapult";

static long now_msecs()
{
    using namespace std::chrono;
    return (long)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// The curl built by Conan doesn't know where the system certificates are.
static const char* find_ca_bundle()
{
    static const char* const candidates[] = {
        "/etc/ssl/certs/ca-certificates.crt",                 // Debian, Ubuntu, Arch
        "/etc/pki/tls/certs/ca-bundle.crt",                   // Fedora, RHEL
        "/etc/ca-certificates/extracted/tls-ca-bundle.pem",   // Arch
        "/etc/ssl/ca-bundle.pem",                             // openSUSE
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        if (fs::exists(candidates[i]))
            return candidates[i];
    }
    return nullptr;
}

Http::Http(Status& status) : status_(status), cancel_(false)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

Http::~Http()
{
    curl_global_cleanup();
}

void Http::set_proxy(const std::string& host, int port)
{
    proxy_host_ = host;
    proxy_port_ = port;
}

void Http::apply_proxy_settings(Settings& settings, bool for_download)
{
    std::string option = settings.get_string("proxy_option");
    if (option == "on" || (for_download && option == "download"))
    {
        std::string host = settings.get_string("proxy_host");
        int port = settings.get_int("proxy_port");
        status_.post("Using proxy: " + host + ":" + std::to_string(port));
        set_proxy(host, port);
    }
    else
    {
        set_proxy("", 0);
    }
}

void Http::cancel()
{
    cancel_ = true;
}

// Options shared by all requests.
static void setup_common(CURL* curl, const std::string& url, const std::string& proxy_host, int proxy_port,
                         char* error_buffer)
{
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);

    const char* ca = find_ca_bundle();
    if (ca)
        curl_easy_setopt(curl, CURLOPT_CAINFO, ca);

    if (!proxy_host.empty() && proxy_port > 0)
    {
        std::string proxy = proxy_host + ":" + std::to_string(proxy_port);
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
    }
}

static size_t write_to_string(char* data, size_t size, size_t count, void* user)
{
    static_cast<std::string*>(user)->append(data, size * count);
    return size * count;
}

bool Http::get(const std::string& url, const std::vector<std::string>& headers, HttpResponse& response)
{
    cancel_ = false;
    response.code = 0;
    response.body.clear();

    CURL* curl = curl_easy_init();
    if (!curl)
        return false;

    char error[CURL_ERROR_SIZE] = "";
    setup_common(curl, url, proxy_host_, proxy_port_, error);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    struct curl_slist* header_list = nullptr;
    for (size_t i = 0; i < headers.size(); ++i)
        header_list = curl_slist_append(header_list, headers[i].c_str());
    if (header_list)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

    CURLcode result = curl_easy_perform(curl);
    if (result == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.code);
    else
        status_.post(std::string("HTTP request failed: ") + (error[0] ? error : curl_easy_strerror(result)), MSG_ERROR);

    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return result == CURLE_OK;
}

long long Http::remote_size(const std::string& url)
{
    CURL* curl = curl_easy_init();
    if (!curl)
        return -1;

    char error[CURL_ERROR_SIZE] = "";
    setup_common(curl, url, proxy_host_, proxy_port_, error);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    // After the redirects, this is the length of the file itself.
    curl_off_t size = -1;
    CURLcode result = curl_easy_perform(curl);
    if (result == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size);
    else
        status_.post(std::string("Could not get the download size: ") + (error[0] ? error : curl_easy_strerror(result)),
                     MSG_WARN);
    curl_easy_cleanup(curl);
    return size;
}

std::string Http::format_size(long long bytes)
{
    char buf[32];
    if (bytes >= 1024LL * 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%.1f GB", bytes / 1073741824.0);
    else if (bytes >= 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / 1048576.0);
    else
        std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    return buf;
}

std::string Http::describe_download(Settings& settings, const std::string& cache_dir, const std::string& url,
                                    const std::string& filename)
{
    // The same check as before the downloads: a cached archive is used instead.
    std::error_code ec;
    std::string cached = cache_dir + "/" + filename;
    if (!settings.get_bool("ignore_cache") && fs::exists(cached, ec))
    {
        uintmax_t size = fs::file_size(cached, ec);
        return "Nothing to download: " + filename + " is already in the cache"
               + (ec ? std::string() : " (" + format_size((long long)size) + ")") + ".";
    }

    apply_proxy_settings(settings, true);
    long long size = remote_size(url);
    if (size < 0)
        return "Download size: unknown, the server does not say.";
    return "Download size: " + format_size(size) + ".";
}

struct DownloadState
{
    std::ofstream file;
    Status* status;
    std::atomic<bool>* cancel;
    long last_time;
    double last_bytes;
};

static size_t write_to_file(char* data, size_t size, size_t count, void* user)
{
    DownloadState* state = static_cast<DownloadState*>(user);
    state->file.write(data, (std::streamsize)(size * count));
    return state->file ? size * count : 0;
}

static int on_progress(void* user, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t)
{
    DownloadState* state = static_cast<DownloadState*>(user);
    if (*state->cancel)
        return 1;  // Non-zero aborts the transfer.
    if (now < 1)
        return 0;

    long delta_time = now_msecs() - state->last_time;
    double delta_bytes = (double)now - state->last_bytes;
    if (delta_time >= PROGRESS_AFTER_MSECS || delta_bytes >= PROGRESS_AFTER_BYTES)
    {
        state->status->post(Http::format_progress((double)now, (double)total, delta_time, delta_bytes));
        state->last_time = now_msecs();
        state->last_bytes = (double)now;
    }
    return 0;
}

std::string Http::format_progress(double downloaded, double total, long delta_msecs, double delta_bytes)
{
    char buf[64];
    std::string result = "Download progress: ";

    if (downloaded > 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%.1f MB", downloaded / 1048576.0);
    else
        std::snprintf(buf, sizeof(buf), "%.1f KB", downloaded / 1024.0);
    result += buf;

    if (total > 0)
    {
        std::snprintf(buf, sizeof(buf), " (%.1f%%)", downloaded / total * 100.0);
        result += buf;
    }

    if (delta_msecs > 0)
    {
        double bps = delta_bytes / (double)delta_msecs * 1000.0;
        if (bps > 1048576.0)
            std::snprintf(buf, sizeof(buf), " at %.1f MB/s", bps / 1048576.0);
        else
            std::snprintf(buf, sizeof(buf), " at %d KB/s", (int)(bps / 1024.0));
        result += buf;
    }
    return result;
}

bool Http::download_file(const std::string& url, const std::string& target_dir, const std::string& target_filename)
{
    cancel_ = false;

    std::error_code ec;
    fs::create_directories(target_dir, ec);
    if (ec)
    {
        status_.post("Download of " + target_filename + " failed: cannot create " + target_dir, MSG_ERROR);
        return false;
    }

    fs::path final_path = fs::path(target_dir) / target_filename;
    fs::path part_path = final_path;
    part_path += ".part";

    status_.post("Downloading " + target_filename);

    DownloadState state;
    state.file.open(part_path, std::ios::binary | std::ios::trunc);
    state.status = &status_;
    state.cancel = &cancel_;
    state.last_time = now_msecs();
    state.last_bytes = 0;
    if (!state.file)
    {
        status_.post("Download of " + target_filename + " failed: cannot write " + part_path.string(), MSG_ERROR);
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl)
        return false;

    char error[CURL_ERROR_SIZE] = "";
    setup_common(curl, url, proxy_host_, proxy_port_, error);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &state);

    CURLcode result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    state.file.close();

    if (result != CURLE_OK)
    {
        if (cancel_)
            status_.post("Download of " + target_filename + " was cancelled", MSG_WARN);
        else
            status_.post("Download of " + target_filename + " failed: " + (error[0] ? error : curl_easy_strerror(result)),
                         MSG_ERROR);
        fs::remove(part_path, ec);
        return false;
    }

    fs::rename(part_path, final_path, ec);
    if (ec)
    {
        status_.post("Download of " + target_filename + " failed: " + ec.message(), MSG_ERROR);
        return false;
    }

    status_.post("Download of " + target_filename + " finished");
    return true;
}
