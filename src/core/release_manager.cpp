#include "core/release_manager.h"

#include "core/platform.h"

std::string ReleaseManager::current_platform() { return platform::os_name(); }

std::string ReleaseManager::make_key(const std::string& game,
                                     const std::string& channel) {
  return game + "-" + channel;
}

ReleaseManager::ReleaseManager(Settings& settings, Status& status)
    : m_settings(settings), m_status(status), m_http(status) {}

std::string ReleaseManager::releases_url(const std::string& game) {
  if (game == "dda")
    return "https://api.github.com/repos/CleverRaven/Cataclysm-DDA/releases";
  if (game == "bn")
    return "https://api.github.com/repos/cataclysmbn/Cataclysm-BN/releases";
  if (game == "eod")
    return "https://api.github.com/repos/AtomicFox556/Cataclysm-EOD/releases";
  if (game == "tish")
    return "https://api.github.com/repos/Cataclysm-TISH-team/Cataclysm-TISH/"
           "releases";
  if (game == "tlg")
    return "https://api.github.com/repos/Cataclysm-TLG/Cataclysm-TLG/releases";
  return "";
}

std::string ReleaseManager::asset_substring(const std::string& game,
                                            const std::string& platform) {
  if (platform != "linux" && platform != "win")
    return "";  // No builds the launcher can install (macOS ones are .dmg).
  bool lin = (platform == "linux");
  if (game == "dda")
    return lin ? "cdda-linux-with-graphics-and-sounds-x64"
               : "cdda-windows-with-graphics-and-sounds-x64";
  if (game == "bn")
    return lin ? "cbn-linux-tiles-x64" : "cbn-windows-tiles-x64";
  if (game == "eod")
    return lin ? "eod-linux-tiles-x64" : "eod-windows-tiles-x64";
  if (game == "tish")
    return lin ? "tish-linux-tiles-x64" : "tish-windows-tiles-x64";
  if (game == "tlg")
    return lin ? "ctlg-linux-tiles-x64" : "ctlg-windows-tiles-x64";
  return "";
}

bool ReleaseManager::parse_builds(const std::string& body,
                                  const std::string& asset_substring,
                                  bool shorten_names,
                                  std::vector<Release>& result,
                                  std::string& error) {
  nlohmann::json json = nlohmann::json::parse(body, nullptr, false);
  if (json.is_discarded()) {
    error = "could not parse the response";
    return false;
  }

  // E.g. when the API rate limit is exceeded, GitHub answers with {"message":
  // ...}.
  if (json.is_object()) {
    error = json.contains("message") && json["message"].is_string()
                ? json["message"].get<std::string>()
                : "unexpected response";
    return false;
  }
  if (!json.is_array()) {
    error = "unexpected response";
    return false;
  }

  result.clear();
  for (size_t i = 0; i < json.size(); ++i) {
    const nlohmann::json& rec = json[i];
    if (!rec.is_object() || !rec.contains("name") || !rec["name"].is_string() ||
        !rec.contains("assets") || !rec["assets"].is_array())
      continue;

    Release r;
    r.name = rec["name"].get<std::string>();
    if (shorten_names) {
      size_t space = r.name.find_last_of(' ');
      if (space != std::string::npos) r.name = r.name.substr(space + 1);
    }

    // If several assets match, the last one wins (like in the original).
    const nlohmann::json& assets = rec["assets"];
    for (size_t a = 0; a < assets.size(); ++a) {
      if (!assets[a].contains("name") || !assets[a]["name"].is_string() ||
          !assets[a].contains("browser_download_url") ||
          !assets[a]["browser_download_url"].is_string())
        continue;
      std::string asset_name = assets[a]["name"].get<std::string>();
      if (asset_name.find(asset_substring) != std::string::npos) {
        r.url = assets[a]["browser_download_url"].get<std::string>();
        r.filename = asset_name;
      }
    }

    if (!r.url.empty()) result.push_back(r);
  }
  return true;
}

bool ReleaseManager::fetch(const std::string& release_key) {
  size_t dash = release_key.find('-');
  if (dash == std::string::npos) {
    m_status.post("Invalid release key: " + release_key, MSG_ERROR);
    return false;
  }
  std::string game = release_key.substr(0, dash);
  std::string channel = release_key.substr(dash + 1);
  std::string platform = current_platform();

  if (channel == "stable") {
    std::vector<Release> list = stable_releases(game, platform);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_releases[release_key] = list;
    return true;
  }
  if (channel != "experimental") {
    m_status.post("Invalid release key: " + release_key, MSG_ERROR);
    return false;
  }

  std::string url = releases_url(game);
  std::string substring = asset_substring(game, platform);
  if (url.empty()) {
    m_status.post("Invalid release key: " + release_key, MSG_ERROR);
    return false;
  }
  if (substring.empty()) {
    // An empty substring would match every asset.
    m_status.post("There are no builds of this game for this platform.",
                  MSG_WARN);
    return false;
  }

  m_status.post("Fetching the list of releases for " + game + "...");
  m_http.apply_proxy_settings(m_settings, false);
  url += "?per_page=" +
         std::to_string(m_settings.get_int("num_releases_to_request"));

  HttpResponse response;
  std::vector<std::string> headers;
  headers.push_back("Accept: application/vnd.github+json");
  if (!m_http.get(url, headers, response)) {
    m_status.post("Could not retrieve the list of releases.", MSG_WARN);
    return false;
  }

  std::vector<Release> list;
  std::string error;
  if (!parse_builds(response.body, substring,
                    m_settings.get_bool("shorten_release_names"), list,
                    error)) {
    m_status.post("Could not get the list of releases (HTTP " +
                      std::to_string(response.code) + "): " + error,
                  MSG_WARN);
    return false;
  }

  if (list.empty()) {
    m_status.post("The response contained no usable releases.", MSG_WARN);
    return false;
  }

  m_status.post("Got " + std::to_string(list.size()) + " releases.");
  std::lock_guard<std::mutex> lock(m_mutex);
  m_releases[release_key] = list;
  return true;
}

std::vector<Release> ReleaseManager::get(const std::string& release_key) {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::map<std::string, std::vector<Release> >::const_iterator it =
      m_releases.find(release_key);
  if (it == m_releases.end()) return std::vector<Release>();
  return it->second;
}

void ReleaseManager::cancel() { m_http.cancel(); }
