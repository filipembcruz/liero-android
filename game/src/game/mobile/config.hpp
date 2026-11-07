#ifndef LIERO_MOBILE_CONFIG_HPP
#define LIERO_MOBILE_CONFIG_HPP

// Mobile settings shared by the launcher (Java) and the game: one key=value
// file, mobile.cfg, in the app's data dir. The original game options live
// in liero.cfg as on PC.

#include <string>

struct Settings;

namespace mobile {

struct Config
{
	std::string name = "Jogador";
	int difficulty = 1;   // 0 easy (original CPU), 1 medium, 2 hard
	int lives = 15;
	int newMap = 1;
	int touchScale = 100; // %
	int touchOpacity = 60; // %
	int lefty = 0;
	int deadZone = 22;    // % of stick radius
	int vibrate = 1;
	int fov = 1;          // 0 close, 1 wide, 2 widest
	int smooth = 0;       // 0 crisp pixels, 1 smoothed (Scale2x + bilinear)
	int loadingTime = 100; // % (original "LOADING TIMES")
	int blood = 100;       // % (original "AMOUNT OF BLOOD")
	int minimap = 1;
	int digButton = 1;
};

extern Config config;

void loadConfig();
void saveConfig();
// Push the relevant values into the original game's Settings.
void applyConfigToSettings(Settings& s);

} // namespace mobile

#endif
