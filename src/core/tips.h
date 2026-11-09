#ifndef EVAC_TIPS_H
#define EVAC_TIPS_H

#include <cstdint>
#include <string>
#include <vector>

// Tips of the day, as plain text.
const std::vector<std::string>& tips_of_the_day();

// Picks a tip; any number works as the seed.
std::string pick_tip(time_t seed);

#endif
