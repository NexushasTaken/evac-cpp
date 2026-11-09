#ifndef EVAC_GAMES_H
#define EVAC_GAMES_H

#include <string>
#include <vector>

struct GameInfo {
  std::string key;  // "dda", "bn", ... Also the name of the game's directory.
  std::string name;
  std::string description;  // Rich text: the HTML subset Qt labels show.
  bool has_stable_channel{};  // The others only have experimental builds.
};

// All supported games, in the order they are shown.
const std::vector<GameInfo>& all_games();

// Returns the game with the given key, or the first one if there is none.
const GameInfo& find_game(const std::string& key);

#endif
