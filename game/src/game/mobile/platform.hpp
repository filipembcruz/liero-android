#ifndef LIERO_MOBILE_PLATFORM_HPP
#define LIERO_MOBILE_PLATFORM_HPP

#include <SDL.h>
#include <string>

struct Controller;
struct Settings;
struct Game;

namespace mobile {

struct Options
{
	bool autostart = false;  // skip Liero's main menu and go straight to a match
	int difficulty = 1;      // 0 easy (original Liero CPU), 1 medium, 2 hard (predictive AI)
	std::string name;        // player 1 name ("" = keep)
	int lives = -1;          // -1 = keep
	int newMap = -1;         // 1 = new random map every game, 0 = keep, -1 = keep setting
	std::string dataDir;     // where the launcher installed TC/ (cwd is set here)
};

extern Options options;

void parseArgs(int argc, char** argv);
// Applies launcher choices on top of the loaded/created liero.cfg.
void applySettings(Settings& s, bool freshSettings);
// Platform events (touch, gamepad, back button, lifecycle). Returns true if
// the event was fully handled and must not reach the original handler.
bool platformEvent(SDL_Event& ev, Controller* controller);
void recordResult(Game& game);
void saveNow();

} // namespace mobile

#endif
