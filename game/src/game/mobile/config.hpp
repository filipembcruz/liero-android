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
	int smooth = 2;       // visual: 0 classic pixels, 1 smoothed (Scale2x + bilinear), 2 HD sprites/terrain
	int loadingTime = 100; // % (original "LOADING TIMES")
	int blood = 100;       // % (original "AMOUNT OF BLOOD")
	int minimap = 1;
	int digButton = 1;
	int touchShow = 0;   // 0 auto (hide while a gamepad is used), 1 always, 2 hidden
	int hudOpacity = 100; // %
	int hudAutoFade = 1;  // fade HUD panels that cover your worm
	int marker = 1;       // arrow above your worm
	int supporter = 0;    // tipped the developer (Google Play): heart by the name
	int lang = 0;         // 0 = device language, 1.. = i18n::codes[lang - 1]
	int theme = 0;        // map theme (drawn visuals): 0..9, see hd.cpp themes[]

	// Gamepad mapping (direction keys are fixed: D-pad / left stick).
	// Values: SDL_GameControllerButton, or 100 = left trigger, 101 = right trigger.
	enum PadAction { PadFire, PadJump, PadRope, PadChange, PadNext, PadPrev, PadDig, PadPause, PadCount };
	int pad[PadCount] = { 101, 0, 100, 1, 10, 9, 3, 6 };
};

char const* padActionName(int action);
std::string padButtonName(int code);

extern Config config;

void loadConfig();
void saveConfig();
// Push the relevant values into the original game's Settings.
void applyConfigToSettings(Settings& s);

} // namespace mobile

#endif
