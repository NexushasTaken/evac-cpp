#include "core/tips.h"

static std::vector<std::string> make_tips() {
  std::vector<std::string> tips;
  tips.push_back(
      "You can bring the Evac folder from Windows to Linux (or vice versa) and "
      "keep using it, but you will "
      "have to download the correct Evac executable and update your game at "
      "least once.");
  tips.push_back(
      "Both C:DDA and C:BN can be installed at the same time. Their "
      "installations are completely independent, "
      "and their updates, mods and soundpacks are managed separately.");
  tips.push_back(
      "You can downgrade your game version by choosing to \"update\" to an "
      "earlier build, but beware of save "
      "compatibility issues.");
  tips.push_back(
      "Mods that come with the game are not managed by Evac (except for "
      "preventing them from being installed "
      "a second time). Evac only deals with the /mods folder, not /data/mods.");
  tips.push_back(
      "You can select multiple mods to install/delete by Ctrl-clicking and "
      "Shift-clicking in the lists.");
  tips.push_back(
      "Kenan's Modpack is updated quite often. Just click Get Kenan Modpack "
      "again to re-download it and replace "
      "the mods in your local repository.");
  tips.push_back(
      "Getting unhelpful error messages? Try enabling debug mode under "
      "Settings for more verbose output.");
  tips.push_back(
      "If you're having an issue with the launcher and don't see it already "
      "reported on GitHub, then you may well "
      "be the first person to notice it! Consider creating a ticket for it, if "
      "you have a GitHub account.");
  tips.push_back(
      "If you keep multiple worlds in your game, beware that savegame backups "
      "always operate on the entire save "
      "directory. This means that when a backup is restored, all previous "
      "contents of the save directory are lost "
      "(including any worlds that weren't in this backup).");
  tips.push_back(
      "You can now have multiple versions of the game installed at the same "
      "time and easily switch between them. "
      "However, saves and other user data are shared between installs, so "
      "beware of breakages or corruption when "
      "switching between too-dissimilar versions (e.g. stable and "
      "experimental).");
  return tips;
}

const std::vector<std::string>& tips_of_the_day() {
  static const std::vector<std::string> tips = make_tips();
  return tips;
}

std::string pick_tip(unsigned long seed) {
  const std::vector<std::string>& tips = tips_of_the_day();
  return tips[seed % tips.size()];
}
