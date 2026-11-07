// Headless deterministic simulation harness for OpenLiero.
//
// Builds against ONLY the core game sources (no gfx.cpp / SDL window), so the
// exact same harness can be compiled against the pristine upstream tree and
// against the Android port tree. Both must produce byte-identical traces.
//
// Usage: liero_sim <TC dir> <scenario> <seed> <frames> <trace out> [ppm prefix]
//   scenario: "cpu"     both worms DumbLieroAI (original Liero CPU)
//             "scripted" worm 0 driven by seeded pseudo-random inputs
//                        (incl. ninja rope, weapon change, digging), worm 1 CPU
//             "rope"     worm 0 spams ninja rope + movement, worm 1 idle
//
// Trace: one line per frame with a hash of the complete relevant game state
// (RNG, worms, ninja rope, weapons, all object lists) and, every 35 frames,
// a hash of the level pixels/materials and of the rendered 320x200 frame.

#include "game/common.hpp"
#include "game/filesystem.hpp"
#include "game/game.hpp"
#include "game/worm.hpp"
#include "game/settings.hpp"
#include "game/viewport.hpp"
#include "game/spectatorviewport.hpp"
#include "game/mixer/player.hpp"
#include "game/gfx/renderer.hpp"
#include "game/weapon.hpp"
#include "game/nobject.hpp"
#include "game/sobject.hpp"
#include "game/bobject.hpp"
#include "game/bonus.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <unistd.h>
#include <new>

static bool g_dbg = false;
struct Hasher
{
	uint64_t h = 1469598103934665603ull;
	int n = 0;
	void u(uint64_t v)
	{
		if (g_dbg) std::fprintf(stderr, "%d:%llx ", n, (unsigned long long)v);
		++n;
		for (int i = 0; i < 8; ++i)
		{
			h ^= (v >> (i * 8)) & 0xff;
			h *= 1099511628211ull;
		}
	}
	void i(int64_t v) { u(uint64_t(v)); }
};

template<typename List>
static void hashObjects(Hasher& hs, List& list, int limit)
{
	hs.u(list.size());
	for (int k = 0; k < limit; ++k)
	{
		auto& o = list.arr[k];
		if (!o.used) continue;
		hs.i(k);
		hs.i(o.x); hs.i(o.y);
	}
}

static uint64_t stateHash(Game& game)
{
	Hasher hs;
	hs.u(game.rand.x); hs.u(game.rand.c);
	hs.i(game.cycles);
	hs.i(game.screenFlash);
	for (auto* wp : game.worms)
	{
		Worm& w = *wp;
		hs.i(w.pos.x); hs.i(w.pos.y); hs.i(w.vel.x); hs.i(w.vel.y);
		hs.i(w.aimingAngle); hs.i(w.aimingSpeed);
		hs.i(w.health); hs.i(w.lives); hs.i(w.kills);
		hs.i(w.direction); hs.i(w.currentWeapon); hs.i(w.currentFrame);
		hs.i(w.killedTimer); hs.i(w.visible); hs.i(w.ready);
		hs.i(w.fireCone); hs.i(w.leaveShellTimer);
		hs.i(w.ninjarope.out); hs.i(w.ninjarope.attached);
		// Rope length fields are uninitialised until the first throw, so
		// only hash rope geometry while the rope is out.
		if (w.ninjarope.out)
		{
			hs.i(w.ninjarope.pos.x); hs.i(w.ninjarope.pos.y);
			hs.i(w.ninjarope.vel.x); hs.i(w.ninjarope.vel.y);
			hs.i(w.ninjarope.length); hs.i(w.ninjarope.curLen);
		}
		for (int j = 0; j < 5; ++j)
		{
			hs.i(w.weapons[j].ammo); hs.i(w.weapons[j].delayLeft);
			hs.i(w.weapons[j].loadingLeft);
		}
	}
	for (int k = 0; k < 600; ++k)
	{
		auto& o = game.wobjects.arr[k];
		if (!o.used) continue;
		hs.i(k); hs.i(o.pos.x); hs.i(o.pos.y); hs.i(o.vel.x); hs.i(o.vel.y);
	}
	for (int k = 0; k < 600; ++k)
	{
		auto& o = game.nobjects.arr[k];
		if (!o.used) continue;
		hs.i(k); hs.i(o.pos.x); hs.i(o.pos.y); hs.i(o.vel.x); hs.i(o.vel.y); hs.i(o.timeLeft);
	}
	hashObjects(hs, game.sobjects, 700);
	hashObjects(hs, game.bonuses, 99);
	hs.u(game.bobjects.size());
	return hs.h;
}

static uint64_t levelHash(Game& game)
{
	Hasher hs;
	std::size_t n = std::size_t(game.level.width) * game.level.height;
	for (std::size_t k = 0; k < n; ++k)
		hs.u(game.level.data[k] | (uint64_t(game.level.materials[k].flags) << 8));
	return hs.h;
}

static uint64_t frameHash(Renderer& r)
{
	Hasher hs;
	for (int y = 0; y < r.renderResY; ++y)
		for (int x = 0; x < r.renderResX; ++x)
			hs.u(r.bmp.getPixel(x, y));
	return hs.h;
}

static void writePPM(Renderer& r, std::string const& path)
{
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return;
	Color pal[256];
	r.pal.activate(pal);
	std::fprintf(f, "P6\n%d %d\n255\n", r.renderResX, r.renderResY);
	for (int y = 0; y < r.renderResY; ++y)
		for (int x = 0; x < r.renderResX; ++x)
		{
			Color c = pal[r.bmp.getPixel(x, y)];
			unsigned char px[3] = { (unsigned char)c.r, (unsigned char)c.g, (unsigned char)c.b };
			std::fwrite(px, 1, 3, f);
		}
	std::fclose(f);
}

int main(int argc, char** argv)
{
	if (argc < 6)
	{
		std::fprintf(stderr, "usage: %s <TC dir> <cpu|scripted|rope> <seed> <frames> <trace> [ppm prefix]\n", argv[0]);
		return 2;
	}
	std::string tcDir = argv[1];
	std::string scenario = argv[2];
	uint32_t seed = (uint32_t)std::strtoul(argv[3], 0, 10);
	int frames = std::atoi(argv[4]);
	std::string tracePath = argv[5];
	std::string ppmPrefix = argc > 6 ? argv[6] : "";

	// OpenLiero's FsNode only handles relative paths on POSIX, so chdir to the
	// TC's parent directory (the Android port does the same with its data dir).
	{
		std::size_t slash = tcDir.find_last_of('/');
		if (slash != std::string::npos)
		{
			std::string parent = tcDir.substr(0, slash);
			if (chdir(parent.empty() ? "/" : parent.c_str()) != 0) { std::perror("chdir"); return 1; }
			tcDir = tcDir.substr(slash + 1);
		}
	}
	gvl::shared_ptr<Common> common(new Common());
	common->load(FsNode(tcDir));

	gvl::shared_ptr<Settings> settings(new Settings());
	settings->wormSettings[0]->controller = (scenario == "cpu") ? 1 : 0;
	settings->wormSettings[1]->controller = (scenario == "rope") ? 0 : 1;

	Game game(common, settings, gvl::shared_ptr<SoundPlayer>(new NullSoundPlayer()));
	game.rand.seed(seed);

	Rand levelRand;
	levelRand.seed(seed * 2654435761u + 1);
	game.level.generateFromSettings(*common, *settings, levelRand);

	Rand weapRand;
	weapRand.seed(seed ^ 0xA5A5A5A5u);

	for (int i = 0; i < 2; ++i)
	{
		Worm* worm = new Worm();
		worm->settings = settings->wormSettings[i];
		worm->health = worm->settings->health;
		worm->index = i;
		worm->statsX = i ? 218 : 0;
		if (worm->settings->controller == 1)
			worm->ai = gvl::shared_ptr<WormAI>(new DumbLieroAI());
		game.addWorm(worm);
	}

	game.addViewport(new Viewport(gvl::rect(0, 0, 158, 158), 0, 504, 350));
	game.addViewport(new Viewport(gvl::rect(160, 0, 158 + 160, 158), 1, 504, 350));
	game.addSpectatorViewport(new SpectatorViewport(gvl::rect(0, 0, 504 + 68, 350), 504, 350));

	// Deterministic weapon pick (replaces the interactive weapon menu)
	for (auto* wp : game.worms)
	{
		bool used[256] = {};
		for (int j = 0; j < 5; ++j)
		{
			int w;
			do { w = common->weapOrder[weapRand(0, 40)]; } while (used[w]);
			used[w] = true;
			wp->weapons[j].ammo = 0;
			wp->weapons[j].type = &common->weapons[w];
		}
		wp->currentWeapon = 0;
		wp->initWeapons(game);
		wp->lives = settings->lives;
	}
	game.releaseControls();
	game.startGame();

	Renderer renderer;
	renderer.init(320, 200);
	renderer.loadPalette(*common);
	game.focus(renderer);

	Rand inputRand;
	inputRand.seed(seed + 777);
	bool held[7] = {};

	FILE* trace = std::fopen(tracePath.c_str(), "w");
	if (!trace) { std::perror("trace"); return 1; }

	for (int f = 0; f < frames; ++f)
	{
		g_dbg = (f == 0 && std::getenv("SIM_DEBUG"));
		if (scenario == "scripted" || scenario == "rope")
		{
			Worm& w = *game.worms[0];
			// Change held controls at random intervals, like a (very twitchy) human.
			for (int c = 0; c < 7; ++c)
			{
				int p = (scenario == "rope" && (c == Worm::Change || c == Worm::Jump)) ? 6 : 12;
				if (inputRand(0, p) == 0)
					held[c] = !held[c];
			}
			if (held[Worm::Left] && held[Worm::Right] && inputRand(0, 2) == 0)
				held[Worm::Right] = false;
			for (int c = 0; c < 7; ++c)
				w.setControlState(static_cast<Worm::Control>(c), held[c]);
		}

		int phase = game.cycles % 2;
		for (std::size_t i = 0; i < game.worms.size(); ++i)
		{
			Worm& worm = *game.worms[(i + phase) % game.worms.size()];
			if (worm.ai.get())
				worm.ai->process(game, worm);
		}
		game.processFrame();

		std::fprintf(trace, "%d %016llx", f, (unsigned long long)stateHash(game));
		if (f % 35 == 0 || f == frames - 1)
		{
			renderer.clear();
			game.draw(renderer, StateGame, false);
			renderer.fadeValue = 33;
			std::fprintf(trace, " L%016llx R%016llx", (unsigned long long)levelHash(game), (unsigned long long)frameHash(renderer));
			if (!ppmPrefix.empty() && (f == 700 || f == frames - 1))
				writePPM(renderer, ppmPrefix + "_" + std::to_string(f) + ".ppm");
		}
		std::fprintf(trace, "\n");
		if (game.isGameOver())
		{
			std::fprintf(trace, "gameover %d\n", f);
			break;
		}
	}
	std::fprintf(trace, "end k0=%d k1=%d l0=%d l1=%d\n",
		game.worms[0]->kills, game.worms[1]->kills, game.worms[0]->lives, game.worms[1]->lives);
	std::fclose(trace);
	return 0;
}
