#ifndef EVAC_RELEASE_DATA_H
#define EVAC_RELEASE_DATA_H

#include <string>
#include <vector>

struct Release {
  std::string name;  // What the user sees, and what the install is called.
  std::string url;
  std::string filename;  // Name of the downloaded archive.
};

struct StableRelease {
  const char* name;
  const char* url;
  const char* filename;
};

// The hardcoded stable releases for a game ("dda" or "bn"; empty for the
// others) on a platform ("linux" or "win"; empty for any other).
std::vector<Release> stable_releases(const std::string& game,
                                     const std::string_view& platform);

#endif
