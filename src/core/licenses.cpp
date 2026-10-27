#include "core/licenses.h"

#include <fstream>
#include <sstream>

static std::string read_text(const std::string& path)
{
    std::ifstream f(path);
    if (!f)
        return "(Could not read " + path + ")";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static const char LIBRARIES_TEXT[] =
    "Evac uses these open source libraries. Each one is distributed under its own license; "
    "see the project's website for the full text.\n\n"
    "gtkmm and GTK - GNU LGPL 2.1 or later - https://www.gtkmm.org\n"
    "libcurl - curl license - https://curl.se\n"
    "OpenSSL - Apache License 2.0 - https://www.openssl.org\n"
    "libarchive - New BSD license - https://www.libarchive.org\n"
    "zlib - zlib license - https://zlib.net\n"
    "nlohmann/json - MIT license - https://github.com/nlohmann/json\n"
    "spdlog and fmt - MIT license - https://github.com/gabime/spdlog, https://github.com/fmtlib/fmt";

std::vector<LicenseSection> load_license_sections(const std::string& data_dir)
{
    std::vector<LicenseSection> sections;
    LicenseSection s;

    s.heading = "Evac";
    s.text = read_text(data_dir + "/licenses/Evac.txt");
    sections.push_back(s);

    // Evac started as a port of Catapult; its license asks to keep this notice.
    s.heading = "Catapult (the original launcher)";
    s.text = read_text(data_dir + "/licenses/Catapult.txt");
    sections.push_back(s);

    s.heading = "Libraries";
    s.text = LIBRARIES_TEXT;
    sections.push_back(s);

    static const char* const fonts[][2] = {
        {"Fantasque Sans Mono", "FantasqueSansMono"},
        {"Hack", "Hack"},
        {"Inconsolata LGC", "Inconsolata-LGC"},
        {"PT Mono", "PT_Mono"},
        {"Ubuntu Mono", "UbuntuMono"},
    };
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); ++i)
    {
        s.heading = std::string("Font: ") + fonts[i][0];
        s.text = read_text(data_dir + "/fonts/licenses/" + fonts[i][1] + ".txt");
        sections.push_back(s);
    }
    return sections;
}
