#ifndef EVAC_RELEASE_MANAGER_H
#define EVAC_RELEASE_MANAGER_H

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "core/http.h"
#include "core/release_data.h"
#include "core/settings.h"
#include "core/status.h"

// Knows which releases of the games are available. Stable releases are
// hardcoded, experimental ones are fetched from the GitHub API.
// A release key looks like "dda-stable" or "bn-experimental".
class ReleaseManager {
 public:
  ReleaseManager(Settings& settings, Status& status);

  // "linux", "win" or "mac" (see platform::os_name).
  static std::string current_platform();

  static std::string make_key(const std::string& game,
                              const std::string& channel);

  // Fills in the list for the key (blocks while fetching, so use a worker
  // thread). Returns false if fetching failed; the old list is kept then.
  bool fetch(const std::string& release_key);

  // A copy of the current list for the key.
  std::vector<Release> get(const std::string& release_key);

  void cancel();

  // Reads a GitHub releases response and keeps the releases that have an
  // asset whose name contains asset_substring. Exposed for tests.
  static bool parse_builds(const std::string& body,
                           const std::string& asset_substring,
                           bool shorten_names, std::vector<Release>& result,
                           std::string& error);

  // The GitHub API URL for the releases of a game, empty if unknown.
  static std::string releases_url(const std::string& game);

  // The part of the asset name that identifies the right build, empty if
  // unknown or if there are no builds for the platform.
  static std::string asset_substring(const std::string& game,
                                     const std::string& platform);

 private:
  Settings& m_settings;
  Status& m_status;
  Http m_http;
  std::mutex m_mutex;
  std::map<std::string, std::vector<Release> > m_releases;
};

#endif
