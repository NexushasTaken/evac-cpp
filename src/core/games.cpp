#include "core/games.h"

static std::vector<GameInfo> make_games()
{
    std::vector<GameInfo> games;
    GameInfo g;

    g.key = "dda";
    g.name = "Cataclysm: Dark Days Ahead";
    g.description =
        "<b>Cataclysm: Dark Days Ahead</b> is a turn-based survival game set in a post-apocalyptic world. "
        "Scavenge, explore, craft, build, farm, repair and modify vehicles, install bionics, mutate, defend "
        "against <span style=\"color:#009900\">zombies</span> and countless other monstrosities \xe2\x80\x94 "
        "all in a limitless, procedurally generated world!";
    g.has_stable_channel = true;
    games.push_back(g);

    g.key = "bn";
    g.name = "Cataclysm: Bright Nights";
    g.description =
        "<b>Cataclysm: Bright Nights</b>. Reject pedantry, embrace <span style=\"color:#ff3300\">!!fun!!</span>. "
        "This fork takes the game back to its sci-fi roguelike roots and reverts many controversial changes by "
        "the DDA team (pockets, proficiencies, freezing, and "
        "<a href=\"https://docs.cataclysmbn.org/game/changelog/\">more</a>). "
        "Special attention is paid to combat, game balance and pacing.";
    g.has_stable_channel = true;
    games.push_back(g);

    g.key = "eod";
    g.name = "Cataclysm: Era of Decay";
    g.description =
        "<b>Cataclysm: Era of Decay</b> is a fork of DDA, primarily aiming to have as much player-accessible "
        "configurability as practically possible, without completely removing any of the original game's "
        "features or content and leaving the game realistic with default settings. Have fun in the way that "
        "<span style=\"color:#ff3300\">you</span> want.";
    g.has_stable_channel = false;
    games.push_back(g);

    g.key = "tish";
    g.name = "Cataclysm: There Is Still Hope";
    g.description =
        "<b>Cataclysm: There Is Still Hope</b> is a fork of DDA with a focus on the game being interesting "
        "above all else, rejecting the dichotomies of \"realism or fun\" and \"verisimilitude or fun\".";
    g.has_stable_channel = false;
    games.push_back(g);

    g.key = "tlg";
    g.name = "Cataclysm: The Last Generation";
    g.description =
        "<b>Cataclysm: The Last Generation</b>. Humanity will not survive this, but you didn't get the memo, "
        "<i>yet</i>. This fork of DDA, guided by WormGirl, features significant rebalancing across the entire "
        "game, with an emphasis on survival and life simulation. Survival is intentional, and all of your "
        "choices matter. Explore this nearly plausible near-future world with all the guns, bionics, and "
        "charcoal smokers you can build, or die trying!";
    g.has_stable_channel = false;
    games.push_back(g);

    return games;
}

const std::vector<GameInfo>& all_games()
{
    static const std::vector<GameInfo> games = make_games();
    return games;
}

const GameInfo& find_game(const std::string& key)
{
    const std::vector<GameInfo>& games = all_games();
    for (size_t i = 0; i < games.size(); ++i)
    {
        if (games[i].key == key)
            return games[i];
    }
    return games[0];
}
