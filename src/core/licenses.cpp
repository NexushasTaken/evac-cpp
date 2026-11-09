#include "core/licenses.h"

#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/fs.h"

static std::string read_text(const std::filesystem::path& path) {
  const std::ifstream f(path);
  if (!f) {
    return "(Could not read " + to_utf8(path) + ")";
  }
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static const std::string_view LIBRARIES_TEXT =
    "Evac uses these open source libraries. Each one is distributed under its "
    "own license; "
    "see the project's website for the full text.\n\n"
    "Qt - GNU LGPL 3 - https://www.qt.io\n"
    "cpr - MIT license - https://github.com/libcpr/cpr\n"
    "libcurl - curl license - https://curl.se\n"
    "OpenSSL - Apache License 2.0 - https://www.openssl.org\n"
    "libarchive - New BSD license - https://www.libarchive.org\n"
    "zlib - zlib license - https://zlib.net\n"
    "nlohmann/json - MIT license - https://github.com/nlohmann/json\n"
    "spdlog and fmt - MIT license - https://github.com/gabime/spdlog, "
    "https://github.com/fmtlib/fmt";

std::vector<LicenseSection> load_license_sections(
    const std::filesystem::path& data_dir) {
  std::vector<LicenseSection> sections;
  LicenseSection s;

  s.heading = "Evac";
  s.text = read_text(data_dir / "licenses" / "Evac.txt");
  sections.push_back(s);

  // Evac started as a port of Catapult; its license asks to keep this notice.
  s.heading = "Catapult (the original launcher)";
  s.text = read_text(data_dir / "licenses" / "Catapult.txt");
  sections.push_back(s);

  s.heading = "Libraries";
  s.text = LIBRARIES_TEXT;
  sections.push_back(s);

  static constexpr std::array<std::array<std::string_view, 2>, 5> fonts = {
      {
          {"Fantasque Sans Mono", "FantasqueSansMono"},
          {"Hack", "Hack"},
          {"Inconsolata LGC", "Inconsolata-LGC"},
          {"PT Mono", "PT_Mono"},
          {"Ubuntu Mono", "UbuntuMono"},
      },
  };
  for (auto font : fonts) {
    s.heading = std::format("Font: {}", font.at(0));
    s.text = read_text(data_dir / "fonts" / "licenses" /
                       to_path(std::string(font.at(1)) + ".txt"));
    sections.push_back(s);
  }
  return sections;
}
