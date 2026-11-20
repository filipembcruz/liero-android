#ifndef LIERO_MOBILE_UI_HPP
#define LIERO_MOBILE_UI_HPP

// Immediate-mode drawing helpers for the HD mobile HUD and menus
// (screen-resolution vector shapes + a baked Poppins Bold atlas).

#include <SDL.h>
#include <string>

namespace mobile {
namespace ui {

enum Align { Left = 0, Center = 1, Right = 2 };

enum Icon
{
	INone, IFire, IRocket, IJump, IRope, ISwap, IDig, IOk, IBack, IPause,
	ISkull, IHeart, IClock, IGear, IPlay, IRestart, IExit, IJoystick, IWeapon, ICrown, IDice, ISliders, IMap, ITrophy, IPeople
};

bool init(SDL_Renderer* r, char const* fontPath);
bool ready();

SDL_Color rgba(int r, int g, int b, float a = 1.0f);
// Multiplies the alpha of everything drawn through rgba()/text (HUD fading).
void setAlpha(float a);
float alpha();

// Text (UTF-8). px = cap-ish height in screen pixels (font em size).
// text()/textWidth() translate the string (i18n); the *Raw variants draw it as is
// (player names, numbers already formatted, ...).
float textWidth(std::string const& s, float px);
float textWidthRaw(std::string const& s, float px);
void text(SDL_Renderer* r, std::string const& s, float x, float y, float px, SDL_Color c,
	int align = Left, bool shadow = true);
void textRaw(SDL_Renderer* r, std::string const& s, float x, float y, float px, SDL_Color c,
	int align = Left, bool shadow = true);

// Shapes
void fillCircle(SDL_Renderer* r, float cx, float cy, float rad, SDL_Color c);
void ring(SDL_Renderer* r, float cx, float cy, float rad, float thick, SDL_Color c);
void tri(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_Color col);
void line(SDL_Renderer* r, float x0, float y0, float x1, float y1, float t, SDL_Color c);
void roundRect(SDL_Renderer* r, SDL_FRect rc, float rad, SDL_Color c);
void roundRectV(SDL_Renderer* r, SDL_FRect rc, float rad, SDL_Color top, SDL_Color bottom);
void roundRectOutline(SDL_Renderer* r, SDL_FRect rc, float rad, float thick, SDL_Color c);

// Styled widgets
void panel(SDL_Renderer* r, SDL_FRect rc, float s, float alpha = 0.82f, bool glow = true);
void bar(SDL_Renderer* r, SDL_FRect rc, float frac, SDL_Color a, SDL_Color b);
void button(SDL_Renderer* r, SDL_FRect rc, float s, std::string const& label, bool primary, bool pressed,
	int icon = INone, bool enabled = true);
void wormHead(SDL_Renderer* r, float cx, float cy, float rad, SDL_Color body);
void icon(SDL_Renderer* r, int icon, float x, float y, float s, SDL_Color c);
// Round action button with neon rim (HUD style)
void actionButton(SDL_Renderer* r, float x, float y, float rad, SDL_Color base, int icon, bool down, float alpha);

inline bool inside(SDL_FRect const& rc, float x, float y)
{
	return x >= rc.x && y >= rc.y && x < rc.x + rc.w && y < rc.y + rc.h;
}

} // namespace ui
} // namespace mobile

#endif
