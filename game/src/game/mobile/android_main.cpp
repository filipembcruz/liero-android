// Android entry point: SDLActivity loads libmain.so and calls SDL_main().
#include <SDL.h>
#include <SDL_main.h>
#include <exception>
#include <string>

int gameEntry(int argc, char* argv[]);

int main(int argc, char* argv[])
{
	try
	{
		return gameEntry(argc, argv);
	}
	catch (std::exception& ex)
	{
		SDL_Log("liero: fatal: %s", ex.what());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Liero",
			(std::string("Sorry, something went wrong :(\n\n") + ex.what()).c_str(), NULL);
		return 1;
	}
}
