#ifndef LIERO_MOBILE_VIEW_HPP
#define LIERO_MOBILE_VIEW_HPP

// Mobile match view: one wide camera that fills the whole phone screen
// (instead of the PC's two 158px split-screen viewports + status bar), drawn
// by the original Viewport renderer, plus the HD HUD on top.
//
// Rendering only: the game's own two Viewports keep existing (the original
// weapon menu / logic use them); this camera is an extra Viewport that is not
// part of Game, so the simulation is untouched.

#include <SDL.h>
#include <vector>
#include <memory>
#include "../gfx/renderer.hpp"
#include "../viewport.hpp"

struct Game;
struct Common;

namespace mobile {

struct View
{
	void setup(SDL_Renderer* r, int winW, int winH);
	void newGame(Game& game);
	// Called once per displayed frame while a match is shown.
	void update(Game& game);
	void present(SDL_Renderer* r, Game& game, bool hud = true);
	// Palette without the original fade-to-black (frozen background behind menus).
	void unfade(Game& game);

	bool active = false;
	Game* game = nullptr;

	// weapon bar hit testing (screen coords), filled by present()
	SDL_FRect slotRect[5];
	int slotCount = 0;
	SDL_FRect pauseRect {0, 0, 0, 0};

	float uiScale() const { return s_; }
	SDL_Texture* weaponIcon(SDL_Renderer* r, Common& common, int weaponIndex)
	{
		ensureIcons(r, common);
		return weaponIndex >= 0 && weaponIndex < (int)weaponIcons_.size() ? weaponIcons_[weaponIndex] : nullptr;
	}
	void ensureIcons(SDL_Renderer* r, Common& common);
	int width() const { return winW_; }
	int height() const { return winH_; }

private:
	void drawHud(SDL_Renderer* r, Game& game);
	void drawMinimap(SDL_Renderer* r, Game& game, SDL_FRect area);
	void drawOffscreen(SDL_Renderer* r, Game& game);

	int winW_ = 0, winH_ = 0;
	int viewW_ = 320, viewH_ = 200, scale_ = 1;
	bool fractional_ = false;
	float fscale_ = 1;
	float s_ = 1;
	SDL_Rect dst_ {0, 0, 0, 0};
	Renderer renderer_;
	std::unique_ptr<Viewport> vp_;
	SDL_Texture* tex_ = nullptr;
	SDL_Texture* minimap_ = nullptr;
	int texW_ = 0, texH_ = 0;
	int smoothTex_ = -1;
	std::vector<uint32_t> buf_, buf2_;
	std::vector<SDL_Texture*> weaponIcons_;
	Common* iconCommon_ = nullptr;
	int minimapAge_ = 1000;
	int levelW_ = 0, levelH_ = 0;
};

extern View view;

} // namespace mobile

#endif
