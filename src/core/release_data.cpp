// Stable release tables, taken from the original launcher.
#include "core/release_data.h"

static const StableRelease DDA_STABLE_LINUX[] = {
    {"0.I Ito",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.I-1/cdda-linux-with-graphics-x64-2026-09-19-2324.tar.gz",
     "cdda-linux-with-graphics-x64-2026-09-19-2324.tar.gz"},
    {"0.H Herbert",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.H-RELEASE/cdda-linux-with-graphics-x64-2024-11-23-1857.tar.gz",
     "cdda-linux-with-graphics-x64-2024-11-23-1857.tar.gz"},
    {"0.G Gaiman",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.G/cdda-linux-tiles-x64-2023-03-01-0054.tar.gz",
     "cdda-linux-tiles-x64-2023-03-01-0054.tar.gz"},
    {"0.F-2 Frank-2",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.F-2/cataclysmdda-0.F-Linux_x64-Tiles-0.F-2.tar.gz",
     "cataclysmdda-0.F-Linux_x64-Tiles-0.F-2.tar.gz"},
    {"0.F-1 Frank-1",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.F-1/cataclysmdda-0.F-Linux_x64-Tiles-0.F-1.tar.gz",
     "cataclysmdda-0.F-Linux_x64-Tiles-0.F-1.tar.gz"},
    {"0.F Frank",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.F/cdda-linux-tiles-x64-2021-07-03-0512.tar.gz",
     "cdda-linux-tiles-x64-2021-07-03-0512.tar.gz"},
    {"0.E-3 Ellison-3",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.E-3/cataclysmdda-0.E-Linux_x64-Tiles-0.E-3.tar.gz",
     "cataclysmdda-0.E-Linux_x64-Tiles-0.E-3.tar.gz"},
    {"0.E-2 Ellison-2",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.E-2/cataclysmdda-0.E-Linux_x64-Tiles-0.E-2.tar.gz",
     "cataclysmdda-0.E-Linux_x64-Tiles-0.E-2.tar.gz"},
    {"0.E-1 Ellison-1",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.E-1/cataclysmdda-0.E-Linux_x64-Tiles-0.E-1.tar.gz",
     "cataclysmdda-0.E-Linux_x64-Tiles-0.E-1.tar.gz"},
    {"0.E Ellison",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.E/cataclysmdda-0.E-Linux_x64-Tiles-10478.tar.gz",
     "cataclysmdda-0.E-Linux_x64-Tiles-10478.tar.gz"},
    {"0.D Danny",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.D/cataclysmdda-0.D-8574-Linux-Tiles.tar.gz",
     "cataclysmdda-0.D-8574-Linux-Tiles.tar.gz"},
};

static const StableRelease DDA_STABLE_WIN[] = {
    {"0.I Ito",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.I-1/cdda-windows-with-graphics-x64-2026-09-19-2324.zip",
     "cdda-windows-with-graphics-x64-2026-09-19-2324.zip"},
    {"0.H Herbert",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.H-RELEASE/cdda-windows-with-graphics-x64-2024-11-23-1857.zip",
     "cdda-windows-with-graphics-x64-2024-11-23-1857.zip"},
    {"0.G Gaiman",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.G/cdda-windows-tiles-x64-2023-03-01-0054.zip",
     "cdda-windows-tiles-x64-2023-03-01-0054.zip"},
    {"0.F-3 Frank-3",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.F-3/cataclysmdda-0.F-Windows_x64-Tiles-0.F-3.zip",
     "cataclysmdda-0.F-Windows_x64-Tiles-0.F-3.zip"},
    {"0.F-2 Frank-2",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.F-2/cataclysmdda-0.F-Windows_x64-Tiles-0.F-2.zip",
     "cataclysmdda-0.F-Windows_x64-Tiles-0.F-2.zip"},
    {"0.F-1 Frank-1",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.F-1/cataclysmdda-0.F-Windows_x64-Tiles-0.F-1.zip",
     "cataclysmdda-0.F-Windows_x64-Tiles-0.F-1.zip"},
    {"0.F Frank",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.F/cdda-windows-tiles-x64-2021-07-03-0512.zip",
     "cdda-windows-tiles-x64-2021-07-03-0512.zip"},
    {"0.E-3 Ellison-3",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.E-3/cataclysmdda-0.E-Windows_x64-Tiles-0.E-3.zip",
     "cataclysmdda-0.E-Windows_x64-Tiles-0.E-3.zip"},
    {"0.E-2 Ellison-2",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.E-2/cataclysmdda-0.E-Windows_x64-Tiles-0.E-2.zip",
     "cataclysmdda-0.E-Windows_x64-Tiles-0.E-2.zip"},
    {"0.E-1 Ellison-1",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.E-1/cataclysmdda-0.E-Windows_x64-Tiles-0.E-1.zip",
     "cataclysmdda-0.E-Windows_x64-Tiles-0.E-1.zip"},
    {"0.E Ellison",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.E/cataclysmdda-0.E-Windows_x64-Tiles-10478.zip",
     "cataclysmdda-0.E-Windows_x64-Tiles-10478.zip"},
    {"0.D Danny",
     "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/0.D/cataclysmdda-0.D-8574-Win64-Tiles.zip",
     "cataclysmdda-0.D-8574-Win64-Tiles.zip"},
};

static const StableRelease BN_STABLE_LINUX[] = {
    {"0.10.0",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.10.0/cbn-linux-tiles-x64-v0.10.0.tar.gz",
     "cbn-linux-tiles-x64-v0.10.0.tar.gz"},
    {"0.9.1",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.9.1/cbn-linux-tiles-x64-v0.9.1.tar.gz",
     "cbn-linux-tiles-x64-v0.9.1.tar.gz"},
    {"0.9.0",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.9.0/cbn-linux-tiles-x64-v0.9.0.tar.gz",
     "cbn-linux-tiles-x64-v0.9.0.tar.gz"},
    {"0.8.0",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.8.0/cbn-linux-tiles-x64-v0.8.0.tar.gz",
     "cbn-linux-tiles-x64-v0.8.0.tar.gz"},
    {"0.7.1",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.7.1/cbn-linux-tiles-x64-v0.7.1.tar.gz",
     "cbn-linux-tiles-x64-v0.7.1.tar.gz"},
    {"0.7.0",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.7.0/cbn-linux-tiles-x64-v0.7.0.tar.gz",
     "cbn-linux-tiles-x64-v0.7.0.tar.gz"},
    {"0.6.0",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.6.0/cbn-linux-tiles-x64-v0.6.0.tar.gz",
     "cbn-linux-tiles-x64-v0.6.0.tar.gz"},
    {"0.5.2",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.5.2/cbn-linux-tiles-x64-0.5.2.tar.gz",
     "cbn-linux-tiles-x64-0.5.2.tar.gz"},
    {"0.5.1",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.5.1/cbn-linux-tiles-x64-0.5.1.tar.gz",
     "cbn-linux-tiles-x64-0.5.1.tar.gz"},
    {"0.5",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.5/cbn-linux-tiles-x64-0.5.tar.gz",
     "cbn-linux-tiles-x64-0.5.tar.gz"},
    {"0.4",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.4/cbn-linux-tiles-x64-0.4.tar.gz",
     "cbn-linux-tiles-x64-0.4.tar.gz"},
    {"0.3",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.3/cbn-linux-tiles-x64-0.3.tar.gz",
     "cbn-linux-tiles-x64-0.3.tar.gz"},
    {"0.2",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.2/cbn-linux-tiles-x64-0.2.tar.gz",
     "cbn-linux-tiles-x64-0.2.tar.gz"},
    {"0.1",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.1/cbn-linux-tiles-x64-0.1.tar.gz",
     "cbn-linux-tiles-x64-0.1.tar.gz"},
};

static const StableRelease BN_STABLE_WIN[] = {
    {"0.10.0",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.10.0/cbn-windows-tiles-x64-msvc-v0.10.0.zip",
     "cbn-windows-tiles-x64-msvc-v0.10.0.zip"},
    {"0.9.1",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.9.1/cbn-windows-tiles-x64-msvc-v0.9.1.zip",
     "cbn-windows-tiles-x64-msvc-v0.9.1.zip"},
    {"0.9.0",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.9.0/cbn-windows-tiles-x64-msvc-v0.9.0.zip",
     "cbn-windows-tiles-x64-msvc-v0.9.0.zip"},
    {"0.8.0",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.8.0/cbn-windows-tiles-x64-msvc-v0.8.0.zip",
     "cbn-windows-tiles-x64-msvc-v0.8.0.zip"},
    {"0.7.1",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.7.1/cbn-windows-tiles-x64-msvc-v0.7.1.zip",
     "cbn-windows-tiles-x64-msvc-v0.7.1.zip"},
    {"0.7.0",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.7.0/cbn-windows-tiles-x64-v0.7.0.zip",
     "cbn-windows-tiles-x64-v0.7.0.zip"},
    {"0.6.0",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.6.0/cbn-windows-tiles-x64-v0.6.0.zip",
     "cbn-windows-tiles-x64-v0.6.0.zip"},
    {"0.5.2",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/v0.5.2/cbn-windows-tiles-x64-msvc-v0.5.2.zip",
     "cbn-windows-tiles-x64-msvc-0.5.2.zip"},
    {"0.5.1",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.5.1/cbn-windows-tiles-x64-msvc-0.5.1.zip",
     "cbn-windows-tiles-x64-msvc-0.5.1.zip"},
    {"0.5",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.5/cbn-windows-tiles-x64-msvc-0.5.zip",
     "cbn-windows-tiles-x64-msvc-0.5.zip"},
    {"0.4",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.4/cbn-windows-tiles-x64-msvc-0.4.zip",
     "cbn-windows-tiles-x64-msvc-0.4.zip"},
    {"0.3",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.3/cbn-windows-tiles-x64-msvc-0.3.zip",
     "cbn-windows-tiles-x64-msvc-0.3.zip"},
    {"0.2",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.2/cbn-windows-tiles-x64-msvc-0.2.zip",
     "cbn-windows-tiles-x64-msvc-0.2.zip"},
    {"0.1",
     "https://github.com/cataclysmbn/Cataclysm-BN/releases/download/cbn-0.1/cbn-windows-tiles-x64-msvc-0.1.zip",
     "cbn-windows-tiles-x64-msvc-0.1.zip"},
};

static std::vector<Release> to_vector(const StableRelease* items, size_t count)
{
    std::vector<Release> result;
    for (size_t i = 0; i < count; ++i)
    {
        Release r;
        r.name = items[i].name;
        r.url = items[i].url;
        r.filename = items[i].filename;
        result.push_back(r);
    }
    return result;
}

std::vector<Release> stable_releases(const std::string& game, const std::string& platform)
{
    bool linux_platform = (platform == "linux");
    if (game == "dda")
    {
        return linux_platform ? to_vector(DDA_STABLE_LINUX, sizeof(DDA_STABLE_LINUX) / sizeof(DDA_STABLE_LINUX[0]))
                              : to_vector(DDA_STABLE_WIN, sizeof(DDA_STABLE_WIN) / sizeof(DDA_STABLE_WIN[0]));
    }
    if (game == "bn")
    {
        return linux_platform ? to_vector(BN_STABLE_LINUX, sizeof(BN_STABLE_LINUX) / sizeof(BN_STABLE_LINUX[0]))
                              : to_vector(BN_STABLE_WIN, sizeof(BN_STABLE_WIN) / sizeof(BN_STABLE_WIN[0]));
    }
    return std::vector<Release>();
}
