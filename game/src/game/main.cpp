#ifndef UUID_DC1D9513CDD34960AB8A648004DA149D
#define UUID_DC1D9513CDD34960AB8A648004DA149D

#include <SDL.h>

#include "gfx.hpp"
#include "sfx.hpp"
#include "sys.hpp"
#include "game.hpp"
#include "viewport.hpp"
#include "worm.hpp"
#include "reader.hpp"
#include "filesystem.hpp"
#include "text.hpp"
#include "keys.hpp"
#include "constants.hpp"
#include "math.hpp"
#include "console.hpp"
#include <gvl/support/platform.hpp>

#include <ctime>
#include <exception>
#include <gvl/math/cmwc.hpp>
#ifdef LIERO_MOBILE
#include "mobile/platform.hpp"
#include <unistd.h>
#endif

#if __APPLE__
#define gameEntry SDL_main
#endif

void benchAll();

int gameEntry(int argc, char* argv[])
try
{
	// TODO: Better PRNG seeding
	gfx.rand.seed(Uint32(std::time(0)));

#if 0
	benchAll();
	return 0;
#elif 0
	std::string const& path = "D:\\cpp\\liero\\tc";
	FsNode node(path);

	for (DirectoryListing dir = node.iter(); dir; ++dir)
	{
		auto d = *dir;

		try
		{
			auto zipNode = node / d;

			bool found = false;

			for (DirectoryListing zipDir = zipNode.iter(); zipDir; ++zipDir)
			{
				auto e = *zipDir;
				auto upper = e;
				toUpperCase(upper);

				if (upper.find(".EXE") != std::string::npos && upper != "LEVEDIT.EXE")
				{
					found = true;
					Common common(zipNode, e);

					printf("N: %s\n", common.guessName().c_str());
					//printf("N: %s ||| %s\n", common.S[SCopyright2].c_str(), common.S[SCopyright].c_str() + 4);
					break;
				}
			}

			if (!found)
				printf("** No exe found for %s\n", d.c_str());
		}
		catch (std::runtime_error& e)
		{
			//printf("Failed to load: %s\n", d.c_str());
			//printf("%s\n", e.what());
		}
	}

	return 0;
#endif

	bool tcSet = false;

	std::string tcName;
	std::string configPath; // Default to current dir

#ifdef LIERO_MOBILE
	mobile::parseArgs(argc, argv);
	if (!mobile::options.dataDir.empty())
	{
		// OpenLiero's FsNode works with paths relative to the cwd on POSIX.
		if (chdir(mobile::options.dataDir.c_str()) != 0)
			SDL_Log("liero: chdir(%s) failed", mobile::options.dataDir.c_str());
	}
	SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
	SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
	SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "1");
	SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE_PAUSEAUDIO, "1");
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
	for(int i = argc; i < argc; ++i)
#else
	for(int i = 1; i < argc; ++i)
#endif
	{
		if(argv[i][0] == '-')
		{
			switch(argv[i][1])
			{
			case '-':
				if (std::strcmp(argv[i] + 2, "config-root") == 0 && i + 1 < argc)
				{
					++i;
					configPath = argv[i];
				}
				break;
			}
		}
		else
		{
			tcName = argv[i];
			tcSet = true;
		}
	}

#ifdef LIERO_MOBILE
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO);
#else
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
#endif

	initKeys();

	precomputeTables();

	gfx.loadMenus();

	gfx.init();
	gfx.setConfigPath(configPath);

	FsNode configNode(gfx.getConfigNode());

	bool freshSettings = false;
	if (!gfx.loadSettings(configNode / "liero.cfg"))
	{
		if (!gfx.loadSettingsLegacy(configNode / "LIERO.DAT"))
		{
			gfx.settings.reset(new Settings);
			freshSettings = true;
#ifndef LIERO_MOBILE
			gfx.saveSettings(configNode / "liero.cfg");
#endif
		}
	}
#ifdef LIERO_MOBILE
	mobile::applySettings(*gfx.settings, freshSettings);
	gfx.saveSettings(configNode / "liero.cfg");
#else
	(void)freshSettings;
#endif

	if (tcSet)
		gfx.settings->tc = tcName;

	// TC loading
	FsNode lieroRoot(configNode / "TC" / gfx.settings->tc);
	gvl::shared_ptr<Common> common(new Common());
	common->load(std::move(lieroRoot));
	gfx.common = common;
	gfx.playRenderer.loadPalette(*common); // This gets the palette from common

	gfx.setVideoMode();
	sfx.init();

	gfx.mainLoop();

	gfx.settings->save(configNode / "liero.cfg", gfx.rand);

	sfx.deinit();
	SDL_Quit();

	//gvl::present_profile(std::cout);

	return 0;
}
catch(std::exception&)
{
	SDL_Quit();
	throw;
}

#endif // UUID_DC1D9513CDD34960AB8A648004DA149D
