// The downloadable soundpacks, taken from the original launcher.
#include "core/soundpack_manager.h"

static std::vector<SoundpackSource> make_sources()
{
    std::vector<SoundpackSource> list;
    SoundpackSource s;

    s.name = "CC-Sounds";
    s.url = "https://github.com/Fris0uman/CDDA-Soundpacks/releases/latest/download/CC-Sounds.zip";
    s.filename = "CC-Sounds.zip";
    s.internal_path = "CC-Sounds";
    list.push_back(s);

    s.name = "CC-Sounds-sfx-only";
    s.url = "https://github.com/Fris0uman/CDDA-Soundpacks/releases/latest/download/CC-Sounds-sfx-only.zip";
    s.filename = "CC-Sounds-sfx-only.zip";
    s.internal_path = "CC-Sounds-sfx-only";
    list.push_back(s);

    s.name = "CO.AG-music-only";
    s.url = "https://github.com/Fris0uman/CDDA-Soundpacks/releases/latest/download/CO.AG-music-only.zip";
    s.filename = "CO.AG-music-only.zip";
    s.internal_path = "CO.AG-music-only";
    list.push_back(s);

    s.name = "BeepBoopBip";
    s.url = "https://github.com/Golfavel/CDDA-Soundpacks_BeepBoop/archive/refs/heads/master.zip";
    s.filename = "BeepBoopBip.zip";
    s.internal_path = "CDDA-Soundpacks_BeepBoop-master/sound/BeepBoopBip";
    list.push_back(s);

    s.name = "@'s soundpack";
    s.url = "https://github.com/damalsk/damalsksoundpack/archive/refs/heads/master.zip";
    s.filename = "ats-soundpack.zip";
    s.internal_path = "damalsksoundpack-master";
    list.push_back(s);

    s.name = "CDDA-Soundpack";
    s.url = "https://github.com/budg3/CDDA-Soundpack/archive/master.zip";
    s.filename = "cdda-soundpack.zip";
    s.internal_path = "CDDA-Soundpack-master/CDDA-Soundpack";
    list.push_back(s);

    s.name = "ChestHole";
    s.url = "https://web.archive.org/web/20240122133501if_/https://chezzo.com/cdda/ChestHoleSoundSet.zip";
    s.filename = "chesthole-soundpack.zip";
    s.internal_path = "ChestHole";
    list.push_back(s);

    s.name = "ChestHoleCC";
    s.url = "https://web.archive.org/web/20210401095201if_/http://chezzo.com/cdda/ChestHoleCCSoundset.zip";
    s.filename = "chesthole-cc-soundpack.zip";
    s.internal_path = "ChestHoleCC";
    list.push_back(s);

    s.name = "ChestOldTimey";
    s.url = "https://web.archive.org/web/20210401095200if_/http://chezzo.com/cdda/ChestOldTimeyLessismore.zip";
    s.filename = "chest-old-timey-soundpack.zip";
    s.internal_path = "ChestHoleOldTimey";
    list.push_back(s);

    s.name = "Otopack";
    s.url = "https://github.com/Kenan2000/Otopack-Mods-Updates/archive/master.zip";
    s.filename = "otopack.zip";
    s.internal_path = "Otopack-Mods-Updates-master/Otopack+ModsUpdates";
    list.push_back(s);

    s.name = "Otopack-BN-Mk-2";
    s.url = "https://github.com/NarandBD/Otopack-BN-Mk-2/archive/main.zip";
    s.filename = "otopack-bn-mk2.zip";
    s.internal_path = "Otopack-BN-Mk-2-main/Otopack+ModsUpdates BN";
    list.push_back(s);

    s.name = "RRFSounds";
    s.url = "https://dl.dropboxusercontent.com/s/d8dfmb2facvkdh6/RRFSounds.zip";
    s.filename = "rrfsounds.zip";
    s.internal_path = "data/sound/RRFSounds";
    list.push_back(s);

    return list;
}

const std::vector<SoundpackSource>& available_soundpacks()
{
    static const std::vector<SoundpackSource> list = make_sources();
    return list;
}
