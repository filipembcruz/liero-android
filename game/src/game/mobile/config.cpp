#include "config.hpp"
#include "../settings.hpp"
#include "../worm.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace mobile {

Config config;

namespace {

struct Field { char const* key; int* i; std::string* s; };

std::vector<Field> fields()
{
	Config& c = config;
	std::vector<Field> f = {
		{ "name", nullptr, &c.name }, { "difficulty", &c.difficulty, nullptr }, { "lives", &c.lives, nullptr },
		{ "newMap", &c.newMap, nullptr }, { "touchScale", &c.touchScale, nullptr },
		{ "touchOpacity", &c.touchOpacity, nullptr }, { "lefty", &c.lefty, nullptr },
		{ "deadZone", &c.deadZone, nullptr }, { "vibrate", &c.vibrate, nullptr }, { "fov", &c.fov, nullptr },
		{ "smooth", &c.smooth, nullptr }, { "loadingTime", &c.loadingTime, nullptr },
		{ "blood", &c.blood, nullptr }, { "minimap", &c.minimap, nullptr }, { "digButton", &c.digButton, nullptr },
		{ "touchShow", &c.touchShow, nullptr }, { "hudOpacity", &c.hudOpacity, nullptr },
		{ "hudAutoFade", &c.hudAutoFade, nullptr }, { "marker", &c.marker, nullptr }, { "theme", &c.theme, nullptr },
	};
	static const char* padKeys[Config::PadCount] = { "padFire", "padJump", "padRope", "padChange", "padNext", "padPrev", "padDig", "padPause" };
	for (int i = 0; i < Config::PadCount; ++i)
		f.push_back({ padKeys[i], &c.pad[i], nullptr });
	return f;
}

int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

} // anon

void loadConfig()
{
	FILE* f = std::fopen("mobile.cfg", "r");
	if (!f)
		return;
	char line[512];
	auto fs = fields();
	while (std::fgets(line, sizeof(line), f))
	{
		char* eq = std::strchr(line, '=');
		if (!eq) continue;
		*eq = 0;
		std::string key = line, val = eq + 1;
		while (!val.empty() && (val.back() == '\n' || val.back() == '\r')) val.pop_back();
		for (auto& fd : fs)
			if (key == fd.key)
			{
				if (fd.i) *fd.i = std::atoi(val.c_str());
				else *fd.s = val;
			}
	}
	std::fclose(f);
	Config& c = config;
	c.difficulty = clampi(c.difficulty, 0, 2);
	c.lives = clampi(c.lives, 1, 999);
	c.touchScale = clampi(c.touchScale, 50, 180);
	c.touchOpacity = clampi(c.touchOpacity, 10, 100);
	c.deadZone = clampi(c.deadZone, 5, 60);
	c.fov = clampi(c.fov, 0, 2);
	c.smooth = clampi(c.smooth, 0, 2);
	c.touchShow = clampi(c.touchShow, 0, 2);
	c.theme = clampi(c.theme, 0, 2);
	c.hudOpacity = clampi(c.hudOpacity, 20, 100);
	c.loadingTime = clampi(c.loadingTime, 0, 9999);
	c.blood = clampi(c.blood, 0, 500);
	if (c.name.empty()) c.name = "Jogador";
	if (c.name.size() > 20) c.name.resize(20);
}

char const* padActionName(int a)
{
	static const char* n[Config::PadCount] = { "Atirar", "Pular", "Corda ninja", "Trocar (segurar)", "Próxima arma", "Arma anterior", "Cavar", "Pausa" };
	return a >= 0 && a < Config::PadCount ? n[a] : "?";
}

std::string padButtonName(int code)
{
	switch (code)
	{
	case 0: return "A"; case 1: return "B"; case 2: return "X"; case 3: return "Y";
	case 4: return "Select/Back"; case 5: return "Guide"; case 6: return "Start";
	case 7: return "L3"; case 8: return "R3"; case 9: return "LB / L1"; case 10: return "RB / R1";
	case 11: return "D-pad cima"; case 12: return "D-pad baixo"; case 13: return "D-pad esq."; case 14: return "D-pad dir.";
	case 15: return "Misc"; case 16: return "Paddle 1"; case 17: return "Paddle 2"; case 18: return "Paddle 3"; case 19: return "Paddle 4";
	case 20: return "Touchpad";
	case 100: return "LT / L2"; case 101: return "RT / R2";
	case -1: return "—";
	default: return "Botão " + std::to_string(code);
	}
}

void saveConfig()
{
	FILE* f = std::fopen("mobile.cfg.tmp", "w");
	if (!f)
		return;
	for (auto& fd : fields())
	{
		if (fd.i) std::fprintf(f, "%s=%d\n", fd.key, *fd.i);
		else std::fprintf(f, "%s=%s\n", fd.key, fd.s->c_str());
	}
	std::fclose(f);
	std::rename("mobile.cfg.tmp", "mobile.cfg");
}

void applyConfigToSettings(Settings& s)
{
	Config& c = config;
	s.wormSettings[0]->controller = 0;
	switch (c.difficulty)
	{
	case 0: s.wormSettings[1]->controller = 1; break;           // original Liero CPU
	case 2: s.wormSettings[1]->controller = 2; s.aiFrames = 140; s.aiMutations = 2; break;
	default: s.wormSettings[1]->controller = 2; s.aiFrames = 35; s.aiMutations = 1; break;
	}
	s.wormSettings[0]->name = c.name;
	if (s.wormSettings[1]->name.empty() || s.wormSettings[1]->name == "Player 2" || s.wormSettings[1]->name == "CPU")
		s.wormSettings[1]->name = "CPU";
	s.lives = c.lives;
	s.regenerateLevel = c.newMap != 0;
	s.loadingTime = c.loadingTime;
	s.blood = c.blood;
}

} // namespace mobile
