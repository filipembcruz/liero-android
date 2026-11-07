// Automated visual test: runs the *mobile* build of the game (same code that
// goes into the APK, LIERO_MOBILE defined) with SDL's offscreen video driver
// at a phone resolution, drives it with real SDL finger events through the
// on-screen controls, and saves screenshots (BMP) of key moments.
//
//   LIERO_TEST_WINDOW=3120x1440 SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
//     liero_shots <data dir> <out dir>

#include <SDL.h>
#include "game/gfx.hpp"
#include "game/mobile/touch.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>

int gameEntry(int argc, char* argv[]);

static std::string outDir;
static int frame = 0;

struct Step { int frame; std::function<void(SDL_Renderer*)> fn; };
static std::vector<Step> steps;

static void finger(SDL_EventType type, int id, float px, float py)
{
	SDL_Event e;
	SDL_zero(e);
	e.type = type;
	e.tfinger.touchId = 1;
	e.tfinger.fingerId = id;
	e.tfinger.x = px / mobile::input.width();
	e.tfinger.y = py / mobile::input.height();
	e.tfinger.pressure = 1.0f;
	SDL_PushEvent(&e);
}

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
	++frame;
	for (auto& s : steps)
		if (s.frame == frame)
			s.fn(r);
}

int main(int argc, char** argv)
{
	if (argc < 3) { std::fprintf(stderr, "usage: %s <data dir> <out dir>\n", argv[0]); return 2; }
	outDir = argv[2];
	int f = 60;
	auto at = [&](int fr, std::function<void(SDL_Renderer*)> fn) { steps.push_back({ fr, fn }); };

	// 1) Weapon selection (original Liero screen), autostarted from the launcher.
	at(f, [](SDL_Renderer* r) { shot(r, "01_weapon_select"); });
	// Navigate to "Done" with the stick (down x6) and confirm with FIRE.
	// These are *instant flicks* (down+move+up delivered in the same frame):
	// the input layer must still hold each press for one game frame.
	for (int i = 0; i < 6; ++i)
	{
		at(f + 10 + i * 12, [](SDL_Renderer*) { stick(1, 0, 0.9f, true); stick(1, 0, 0.9f, false); });
	}
	f += 10 + 6 * 12 + 10;
	at(f, [](SDL_Renderer*) { pressButton(mobile::VFire, 2, true); });
	at(f + 6, [](SDL_Renderer*) { pressButton(mobile::VFire, 2, false); });
	f += 160;
	at(f, [](SDL_Renderer* r) { shot(r, "02_match_start"); });
	// 2) Walk right and shoot.
	at(f + 5, [](SDL_Renderer*) { stick(1, 1.0f, -0.3f, true); });
	at(f + 20, [](SDL_Renderer*) { pressButton(mobile::VFire, 2, true); });
	at(f + 70, [](SDL_Renderer* r) { shot(r, "03_firing"); });
	at(f + 72, [](SDL_Renderer*) { pressButton(mobile::VFire, 2, false); stick(1, 1.0f, -0.3f, false); });
	f += 90;
	// 3) Weapon swap (Change + Right pulse) – Liero shows the weapon name.
	at(f, [](SDL_Renderer*) { pressButton(mobile::VSwapTap, 3, true); });
	at(f + 4, [](SDL_Renderer*) { pressButton(mobile::VSwapTap, 3, false); });
	at(f + 12, [](SDL_Renderer* r) { shot(r, "04_weapon_swap"); });
	f += 30;
	// 4) Ninja rope: aim up, throw rope, hold.
	at(f, [](SDL_Renderer*) { stick(1, 0.2f, -1.0f, true); });
	at(f + 15, [](SDL_Renderer*) { pressButton(mobile::VRope, 4, true); });
	at(f + 45, [](SDL_Renderer* r) { shot(r, "05_ninja_rope"); });
	at(f + 50, [](SDL_Renderer*) { pressButton(mobile::VRope, 4, false); stick(1, 0.2f, -1.0f, false); });
	f += 80;
	// 5) Pause via the on-screen pause button -> original menu with RESUME GAME.
	at(f, [](SDL_Renderer*) { pressButton(mobile::VEscape, 5, true); });
	at(f + 4, [](SDL_Renderer*) { pressButton(mobile::VEscape, 5, false); });
	at(f + 90, [](SDL_Renderer* r) { shot(r, "06_pause_menu"); });
	// 6) Android back button (AC_BACK) in the menu must NOT quit.
	at(f + 100, [](SDL_Renderer*) {
		SDL_Event e; SDL_zero(e);
		e.type = SDL_KEYDOWN; e.key.keysym.scancode = SDL_SCANCODE_AC_BACK; SDL_PushEvent(&e);
		e.type = SDL_KEYUP; SDL_PushEvent(&e);
	});
	at(f + 130, [](SDL_Renderer* r) { shot(r, "07_after_back"); });
	// 7) Resume with OK on "RESUME GAME" (move up to it first).
	at(f + 140, [](SDL_Renderer*) { stick(1, 0, -0.9f, true); });
	at(f + 146, [](SDL_Renderer*) { stick(1, 0, -0.9f, false); });
	for (int i = 0; i < 8; ++i)
	{
		at(f + 150 + i * 10, [](SDL_Renderer*) { stick(1, 0, -0.9f, true); });
		at(f + 155 + i * 10, [](SDL_Renderer*) { stick(1, 0, -0.9f, false); });
	}
	at(f + 240, [](SDL_Renderer* r) { shot(r, "08_menu_resume_selected"); });
	at(f + 245, [](SDL_Renderer*) { pressButton(mobile::VEnter, 6, true); });
	at(f + 250, [](SDL_Renderer*) { pressButton(mobile::VEnter, 6, false); });
	at(f + 330, [](SDL_Renderer* r) { shot(r, "09_resumed"); std::fflush(stdout); std::_Exit(0); });

	gfx.mobileFrameHook = hook;

	std::string dataDir = argv[1];
	std::vector<std::string> args = { "liero", "--data-dir", dataDir, "--autostart", "--difficulty", "0",
		"--name", "Voce" };
	std::vector<char*> av;
	for (auto& a : args) av.push_back(&a[0]);
	av.push_back(nullptr);
	return gameEntry((int)args.size(), av.data());
}
