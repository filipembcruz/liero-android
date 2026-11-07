#include "platform.hpp"
#include "touch.hpp"
#include "config.hpp"

#include "../gfx.hpp"
#include "../game.hpp"
#include "../worm.hpp"
#include "../settings.hpp"
#include "../keys.hpp"
#include "../controller/controller.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mobile {

Options options;
char lastResult = 0;

void parseArgs(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		auto next = [&]() -> std::string { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
		if (a == "--autostart") options.autostart = true;
		else if (a == "--data-dir") options.dataDir = next();
	}
}

void initFromConfig()
{
	loadConfig();
	TouchConfig tc;
	tc.scale = config.touchScale / 100.0f;
	tc.opacity = config.touchOpacity / 100.0f;
	tc.leftHanded = config.lefty != 0;
	tc.deadZone = config.deadZone / 100.0f;
	tc.vibrate = config.vibrate != 0;
	tc.digButton = config.digButton != 0;
	input.init(tc);
}

void applySettings(Settings& s, bool freshSettings)
{
	if (freshSettings)
	{
		// Phone-friendly defaults, only on first run.
		s.recordReplays = false;
	}
	s.fullscreen = true;
	s.spectatorWindow = false;
	s.singleScreenReplay = false;
	// Single player: the CPU's weapon menu can't be driven from a phone, so
	// bots get RANDOM weapons (an original OpenLiero option).
	if (s.selectBotWeapons == 1)
		s.selectBotWeapons = 0;
	applyConfigToSettings(s);
}

void saveNow()
{
	if (gfx.settings)
	{
		try
		{
			gfx.settings->save(FsNode("liero.cfg"), gfx.rand);
		}
		catch (std::exception& e)
		{
			SDL_Log("liero: could not save settings: %s", e.what());
		}
	}
}

void recordResult(Game& game)
{
	if (game.worms.size() < 2)
		return;
	if (game.cycles <= 0)
		return; // still in weapon selection (lives not set yet)
	Worm& me = *game.worms[0];
	Worm& cpu = *game.worms[1];
	bool won = me.lives > cpu.lives || (me.lives == cpu.lives && me.kills > cpu.kills);
	bool draw = me.lives == cpu.lives && me.kills == cpu.kills;
	lastResult = draw ? 'D' : (won ? 'W' : 'L');
	FILE* f = std::fopen("results.txt", "a");
	if (f)
	{
		std::fprintf(f, "%c %d %d %d\n", draw ? 'D' : (won ? 'W' : 'L'), me.kills, cpu.kills, config.difficulty);
		std::fclose(f);
	}
}

bool platformEvent(SDL_Event& ev, Controller* controller)
{
	switch (ev.type)
	{
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		// Android back button behaves like Esc: in a match it opens the
		// original pause/main menu (with "Resume"); in menus it goes back.
		// It never closes the app (SDL hint traps it).
		if (ev.key.keysym.scancode == SDL_SCANCODE_AC_BACK)
		{
			ev.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
			ev.key.keysym.sym = SDLK_ESCAPE;
		}
		return false;

	case SDL_APP_WILLENTERBACKGROUND:
	case SDL_APP_TERMINATING:
	case SDL_APP_LOWMEMORY:
		saveNow();
		if (input.mode() == Mode::Game && controller)
		{
			// Auto-pause: same as pressing Esc during the match.
			input.releaseAll(controller);
			controller->onKey(DkEscape, true);
			controller->onKey(DkEscape, false);
		}
		return true;

	case SDL_APP_DIDENTERFOREGROUND:
	case SDL_APP_WILLENTERFOREGROUND:
	case SDL_APP_DIDENTERBACKGROUND:
		return true;

	case SDL_WINDOWEVENT:
		if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
		{
			gfx.onWindowResize(ev.window.windowID);
			return true;
		}
		return false;

	case SDL_FINGERDOWN:
	case SDL_FINGERUP:
	case SDL_FINGERMOTION:
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
	case SDL_CONTROLLERAXISMOTION:
		input.handleEvent(ev, controller);
		return true;

	case SDL_CONTROLLERDEVICEADDED:
		input.handleEvent(ev, controller);
		return false;

	case SDL_JOYAXISMOTION:
	case SDL_JOYHATMOTION:
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		// Recognised gamepads are handled via the GameController API above;
		// let raw joystick events through for the original joystick binding
		// only when the device is not a known game controller.
		if (SDL_GameControllerFromInstanceID(ev.jbutton.which))
			return true;
		return false;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEMOTION:
		return true;

	default:
		return false;
	}
}

} // namespace mobile
