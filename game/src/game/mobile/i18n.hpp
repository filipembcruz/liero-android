#pragma once
#include <string>

// UI translations. Keys are the Portuguese source strings; the table lives in
// ui/lang.tsv (shared with the Android launcher). Unknown strings (weapon names,
// player names, numbers) pass through unchanged.
namespace mobile {
namespace i18n {

// Language codes in table column order.
extern const char* const codes[];
extern const char* const names[];  // native names, for the settings
const int count = 5;               // pt, en, es, fr, de

bool load(char const* path);
// lang: 0 = automatic (device language), 1.. = codes[lang - 1]
void setLanguage(int lang);
int current();                       // resolved column (0 = pt)
std::string const& tr(std::string const& pt);

} // namespace i18n

inline std::string const& T(std::string const& pt) { return i18n::tr(pt); }

} // namespace mobile
