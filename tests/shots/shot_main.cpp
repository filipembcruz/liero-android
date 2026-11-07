// Automated visual test: runs the *mobile* build of the game (same code that
// goes into the APK, LIERO_MOBILE defined) with SDL's offscreen video driver
// at a phone resolution, drives it with real SDL finger events through the
// native menus and on-screen controls, and saves screenshots (BMP).
//
//   LIERO_TEST_WINDOW=3120x1440 SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
//     liero_shots <data dir> <out dir>

#include <SDL.h>
#include "game/gfx.hpp"
#include "game/mobile/touch.hpp"
#include "game/mobile/view.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>

int gameEntry(int argc, char* argv[]);

static std::string outDir;
static int frame = 0;
static int W = 0, H = 0;
static float S = 1;

struct Step { int frame; std::function<void(SDL_Renderer*)> fn; };
static std::vector<Step> steps;

static void finger(SDL_EventType type, int id, float px, float py)
{
	SDL_Event e;
	SDL_zero(e);
	e.type = type;
	e.tfinger.touchId = 1;
	e.tfinger.fingerId = id;
	e.tfinger.x = px / W;
	e.tfinger.y = py / H;
	e.tfinger.pressure = 1.0f;
	SDL_PushEvent(&e);
}
static void tapAt(float x, float y, int id = 9) { finger(SDL_FINGERDOWN, id, x, y); finger(SDL_FINGERUP, id, x, y); }

static void pressButton(mobile::VIn in, int id, bool down)
{
	float x, y;
	if (!mobile::input.buttonCenter(in, x, y)) { std::printf("button %d not found\n", (int)in); return; }
	finger(down ? SDL_FINGERDOWN : SDL_FINGERUP, id, x, y);
}

static void stick(int id, float dx, float dy, bool down)
{
	float x, y, r;
	mobile::input.stickCenter(x, y, r);
	if (down)
	{
		finger(SDL_FINGERDOWN, id, x, y);
		finger(SDL_FINGERMOTION, id, x + dx * r, y + dy * r);
	}
	else
		finger(SDL_FINGERUP, id, x + dx * r, y + dy * r);
}

static void shot(SDL_Renderer* r, char const* name)
{
	int w, h;
	SDL_GetRendererOutputSize(r, &w, &h);
	SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
	SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
	std::string path = outDir + "/" + name + ".bmp";
	SDL_SaveBMP(s, path.c_str());
	SDL_FreeSurface(s);
	std::printf("shot %s (frame %d)\n", path.c_str(), frame);
	std::fflush(stdout);
}

static void hook(SDL_Renderer* r)
{
	if (!W) { SDL_GetRendererOutputSize(r, &W, &H); S = H / 720.0f; }
	++frame;
	for (auto& s : steps)
		if (s.frame == frame)
			s.fn(r);
}

// Geometry of the native menus (mirrors mobile/menu.cpp)
static void pickerGo(float& x, float& y)
{
	float sw = std::min(200 * S, (W - 620 * S) / 5);
	float barx = 30 * S, bary = H - 176 * S, barw = sw * 5 + 16 * S;
	float bx = barx + barw + 24 * S, bw = W - bx - 30 * S;
	x = bx + bw / 2; y = bary + 80 * S + 35 * S;
}
static void pickerRandom(float& x, float& y) { pickerGo(x, y); y -= 76 * S; }
static void pauseButton(int i, bool running, float& x, float& y)
{
	float ph = (running ? 520 : 470) * S;
	float py = (H - ph) / 2;
	x = W / 2.0f; y = py + 136 * S + i * 80 * S + 33 * S;
}

int main(int argc, char** argv)
{
	if (argc < 3) { std::fprintf(stderr, "usage: %s <data dir> <out dir>\n", argv[0]); return 2; }
	outDir = argv[2];
	auto at = [&](int fr, std::function<void(SDL_Renderer*)> fn) { steps.push_back({ fr, fn }); };
	int f = 30;

	// 1) Native weapon picker
	at(f, [](SDL_Renderer* r) { shot(r, "01_weapon_picker"); });
	at(f + 5, [](SDL_Renderer*) { float x, y; pickerRandom(x, y); tapAt(x, y); });
	at(f + 15, [](SDL_Renderer* r) { shot(r, "02_weapon_picker_random"); });
	at(f + 20, [](SDL_Renderer*) { float x, y; pickerGo(x, y); tapAt(x, y); });
	f += 20 + 170;
	at(f, [](SDL_Renderer* r) { shot(r, "03_match_hud"); });
	// 2) Walk right and shoot.
	at(f + 5, [](SDL_Renderer*) { stick(1, 1.0f, -0.3f, true); });
	at(f + 20, [](SDL_Renderer*) { pressButton(mobile::VFire, 2, true); });
	at(f + 70, [](SDL_Renderer* r) { shot(r, "04_firing"); });
	at(f + 72, [](SDL_Renderer*) { pressButton(mobile::VFire, 2, false); stick(1, 1.0f, -0.3f, false); });
	f += 90;
	// 3) Weapon bar: tap slot 4 directly
	at(f, [](SDL_Renderer*) {
		SDL_FRect r = mobile::view.slotRect[3];
		tapAt(r.x + r.w / 2, r.y + r.h / 2, 3);
	});
	at(f + 15, [](SDL_Renderer* r) { shot(r, "05_weapon_bar_select"); });
	f += 30;
	// 4) Ninja rope: aim up, throw rope, hold.
	at(f, [](SDL_Renderer*) { stick(1, 0.2f, -1.0f, true); });
	at(f + 15, [](SDL_Renderer*) { pressButton(mobile::VRope, 4, true); });
	at(f + 45, [](SDL_Renderer* r) { shot(r, "06_ninja_rope"); });
	at(f + 50, [](SDL_Renderer*) { pressButton(mobile::VRope, 4, false); stick(1, 0.2f, -1.0f, false); });
	f += 70;
	// 5) Dig button
	at(f, [](SDL_Renderer*) { pressButton(mobile::VDig, 6, true); });
	at(f + 40, [](SDL_Renderer* r) { shot(r, "07_dig"); });
	at(f + 42, [](SDL_Renderer*) { pressButton(mobile::VDig, 6, false); });
	f += 60;
	// 6) Pause via the HUD pause button -> native menu
	at(f, [](SDL_Renderer*) { SDL_FRect p = mobile::view.pauseRect; tapAt(p.x + p.w / 2, p.y + p.h / 2, 5); });
	f += 60; // fade out + menu
	at(f, [](SDL_Renderer* r) { shot(r, "08_pause_menu"); });
	// 7) Settings (third button)
	at(f + 5, [](SDL_Renderer*) { float x, y; pauseButton(2, true, x, y); tapAt(x, y); });
	at(f + 15, [](SDL_Renderer* r) { shot(r, "09_settings_controls"); });
	at(f + 20, [](SDL_Renderer*) { tapAt(W * 0.5f - 560 * S + 420 * S + 225 * S + 105 * S, 30 * S + 18 * S + 27 * S); });
	at(f + 30, [](SDL_Renderer* r) { shot(r, "10_settings_game"); });
	at(f + 35, [](SDL_Renderer*) { tapAt(W * 0.5f - 560 * S + 420 * S + 450 * S + 105 * S, 30 * S + 18 * S + 27 * S); });
	at(f + 45, [](SDL_Renderer* r) { shot(r, "11_settings_visual"); });
	// back with Android BACK key
	at(f + 50, [](SDL_Renderer*) {
		SDL_Event e; SDL_zero(e);
		e.type = SDL_KEYDOWN; e.key.keysym.scancode = SDL_SCANCODE_AC_BACK; SDL_PushEvent(&e);
		e.type = SDL_KEYUP; SDL_PushEvent(&e);
	});
	// 8) Resume (first button)
	at(f + 60, [](SDL_Renderer*) { float x, y; pauseButton(0, true, x, y); tapAt(x, y); });
	at(f + 140, [](SDL_Renderer* r) { shot(r, "12_resumed"); std::fflush(stdout); std::_Exit(0); });

	gfx.mobileFrameHook = hook;

	std::string dataDir = argv[1];
	std::vector<std::string> args = { "liero", "--data-dir", dataDir, "--autostart" };
	std::vector<char*> av;
	for (auto& a : args) av.push_back(&a[0]);
	av.push_back(nullptr);
	return gameEntry((int)args.size(), av.data());
}
