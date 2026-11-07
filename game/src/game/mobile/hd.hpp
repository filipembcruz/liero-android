#ifndef LIERO_MOBILE_HD_HPP
#define LIERO_MOBILE_HD_HPP

// "Visual: HD" layer: cave background, textured terrain and HD sprites for
// worms, guns, the main projectiles, explosions and bonuses, drawn at screen
// resolution on top of the original renderer (which still draws particles,
// bullets, rope, laser sights, text...). Rendering only: no game state is
// touched, so rules/physics stay identical.

#include <SDL.h>
#include <vector>

struct Game;
struct Common;

namespace mobile {
namespace hd {

bool load(SDL_Renderer* r, char const* path);
bool ready();

struct Camera
{
	float x = 0, y = 0;    // top-left of the view in level pixels
	float scale = 1;       // screen pixels per level pixel
	float ox = 0, oy = 0;  // screen offset of level pixel (x, y)
	int screenW = 0, screenH = 0;
	int viewW = 0, viewH = 0; // view size in level pixels
};

// Per match
void newGame(SDL_Renderer* r, Game& game);
void drawBackground(SDL_Renderer* r, Game& game, Camera const& cam);
void drawTerrain(SDL_Renderer* r, Game& game, Camera const& cam);
void drawObjects(SDL_Renderer* r, Game& game, Camera const& cam);

// Per weapon index: true if the HD layer draws its projectile.
bool const* weaponMask(Common& common);
// HD icon for a weapon (gun or projectile), or nullptr.
SDL_Texture* weaponIcon(Common& common, int weaponIndex, int* w = nullptr, int* h = nullptr);

} // namespace hd
} // namespace mobile

#endif
