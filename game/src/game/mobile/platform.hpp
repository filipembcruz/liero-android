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
	bool autostart = false;  // go straight to the weapon picker / match
	std::string dataDir;     // where the launcher installed TC/ (cwd is set here)
};

extern Options options;
extern char lastResult; // result of the last finished match (W/L/D)

void parseArgs(int argc, char** argv);
// Loads mobile.cfg (after chdir to the data dir) and configures input.
void initFromConfig();
// Applies launcher choices on top of the loaded/created liero.cfg.
void applySettings(Settings& s, bool freshSettings);
// Platform events (touch, gamepad, back button, lifecycle). Returns true if
// the event was fully handled and must not reach the original handler.
bool platformEvent(SDL_Event& ev, Controller* controller);
void recordResult(Game& game);
void saveNow();

} // namespace mobile

#endif
