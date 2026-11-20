#include "i18n.hpp"
#include <SDL.h>
#include <unordered_map>
#include <vector>
#include <cstring>

namespace mobile {
namespace i18n {

const char* const codes[] = { "pt", "en", "es", "fr", "de" };
const char* const names[] = { "Português", "English", "Español", "Français", "Deutsch" };

namespace {
std::unordered_map<std::string, std::vector<std::string>> table;
int column = 1; // English until told otherwise

int systemColumn()
{
#if SDL_VERSION_ATLEAST(2, 0, 14)
	if (SDL_Locale* loc = SDL_GetPreferredLocales())
	{
		int found = -1;
		for (SDL_Locale* l = loc; l->language && found < 0; ++l)
			for (int i = 0; i < count; ++i)
				if (std::strncmp(l->language, codes[i], 2) == 0) { found = i; break; }
		SDL_free(loc);
		if (found >= 0) return found;
	}
#endif
	return 1; // English for every other language
}
} // anon

bool load(char const* path)
{
	SDL_RWops* f = SDL_RWFromFile(path, "rb");
	if (!f) { SDL_Log("liero: no translations (%s)", path); return false; }
	Sint64 n = SDL_RWsize(f);
	std::string data(n > 0 ? std::size_t(n) : 0, '\0');
	if (n > 0) SDL_RWread(f, &data[0], 1, std::size_t(n));
	SDL_RWclose(f);
	std::size_t pos = 0;
	bool header = true;
	while (pos < data.size())
	{
		std::size_t eol = data.find('\n', pos);
		if (eol == std::string::npos) eol = data.size();
		std::string line = data.substr(pos, eol - pos);
		pos = eol + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (header) { header = false; continue; }
		if (line.empty() || line[0] == '#') continue;
		std::vector<std::string> cols;
		std::size_t a = 0;
		for (;;)
		{
			std::size_t t = line.find('\t', a);
			cols.push_back(line.substr(a, t == std::string::npos ? std::string::npos : t - a));
			if (t == std::string::npos) break;
			a = t + 1;
		}
		// unescape \n
		for (auto& c : cols)
			for (std::size_t k; (k = c.find("\\n")) != std::string::npos;) c.replace(k, 2, "\n");
		if (!cols.empty() && !cols[0].empty()) table[cols[0]] = cols;
	}
	SDL_Log("liero: %d translations", (int)table.size());
	return true;
}

void setLanguage(int lang)
{
	column = (lang >= 1 && lang <= count) ? lang - 1 : systemColumn();
}

int current() { return column; }

std::string const& tr(std::string const& pt)
{
	if (column == 0) return pt;
	auto it = table.find(pt);
	if (it == table.end() || int(it->second.size()) <= column || it->second[column].empty()) return pt;
	return it->second[column];
}

} // namespace i18n
} // namespace mobile
