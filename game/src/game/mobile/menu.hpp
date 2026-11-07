#ifndef LIERO_MOBILE_MENU_HPP
#define LIERO_MOBILE_MENU_HPP

// Native HD menus (Portuguese, touch + gamepad): pause/main menu with the
// settings screens, and the pre-match weapon picker.

struct Common;
struct Settings;
struct Game;

namespace mobile {

enum MenuResult { MenuResume, MenuNewGame, MenuQuit, MenuOriginal };

// gameRunning: a match is in progress (offer "Continuar").
// lastResult: 'W', 'L', 'D' or 0 (shown as the end-of-match banner).
MenuResult pauseMenu(Game* game, bool gameRunning, char lastResult);

// Returns false if the player backed out.
bool weaponPicker(Game& game);

// Set while the native picker has chosen player 1's weapons: the original
// weapon menu then only arms the worms and starts immediately.
extern bool weaponsPreselected;

} // namespace mobile

#endif
