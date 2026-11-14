#include "hd.hpp"

#include "../game.hpp"
#include "../worm.hpp"
#include "../weapon.hpp"
#include "../sobject.hpp"
#include "../bonus.hpp"
#include "../common.hpp"
#include "../level.hpp"
#include "../math.hpp"
#include "../constants.hpp"
#include "config.hpp"

#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <algorithm>

extern "C" int mz_uncompress(unsigned char* pDest, unsigned long* pDest_len, const unsigned char* pSource, unsigned long source_len);

namespace mobile {
namespace hd {

namespace {

const float Pi = 3.14159265f;

struct Spr
{
	SDL_Texture* tex = nullptr;
	int w = 0, h = 0, ax = 0, ay = 0;
	std::vector<uint32_t> px; // ARGB copy (only kept for recolourable/texture entries)
};

std::map<std::string, Spr> sprites;     // base HD set (ui/hd.bin)
std::map<std::string, Spr> skinSprites; // overrides of the current skin (ui/hd_<skin>.bin)
bool loaded = false;
int curSkin = 0;                        // 0 = HD, 1.. = extra skins
SDL_Renderer* rend = nullptr;
const char* skinFiles[] = { nullptr, "ui/hd_mole.bin", "ui/hd_army.bin", "ui/hd_bot.bin" };
const int SkinCount = 4;

// per-game state
Game* curGame = nullptr;
SDL_Texture* wormTex[2][8] = {};
const char* wormFrames[8] = { "worm_idle", "worm_walk1", "worm_walk2", "worm_walk3", "worm_walk4", "worm_jump", "worm_hurt", "worm_rope" };
int lastHealth[2] = { 0, 0 };
int hurtTimer[2] = { 0, 0 };

// terrain
const int T = 4; // terrain texture pixels per level pixel
SDL_Texture* terrainTex = nullptr;
int terrW = 0, terrH = 0;
std::vector<unsigned char> levelCache;
std::vector<uint32_t> genBuf;

struct WeaponLook
{
	const char* proj = nullptr; // HD projectile sprite
	float size = 8;             // longest side in level pixels
	int rot = 0;                // 0 = static, 1 = face velocity, 2 = spin
	const char* gun = "gun_pistol";
	float gunLen = 8;
};
struct Theme
{
	const char* bg; const char* dirt; const char* rock;
	uint32_t rim;      // colour of the soft rim inside tunnels (RGB)
	int rimAlpha;      // max alpha of that rim
	uint32_t edge;     // colour of the solid edge line (RGB)
	float edgeK;       // 0 = dark outline (multiply), 1 = coloured glow
};
// Map themes (Configurações -> Visual -> Tema do mapa): background + ground + rock.
const int ThemeCount = 10;
const Theme themes[ThemeCount] = {
	{ "bg_t0", "tex_dirt_t0", "tex_rock_t0", 0x0A0612, 150, 0x000000, 0.0f },
	{ "bg_t1", "tex_dirt_t1", "tex_rock_t1", 0x1A0E06, 140, 0x000000, 0.0f },
	{ "bg_t2", "tex_dirt_t2", "tex_rock_t2", 0x0A1020, 120, 0x000000, 0.0f },
	{ "bg_t3", "tex_dirt_t3", "tex_rock_t3", 0x140606, 150, 0x000000, 0.0f },
	{ "bg_t4", "tex_dirt_t4", "tex_rock_t4", 0x060E06, 150, 0x000000, 0.0f },
	{ "bg_t5", "tex_dirt_t5", "tex_rock_t5", 0x100404, 150, 0xFF6A20, 0.45f },
	{ "bg_t6", "tex_dirt_t6", "tex_rock_t6", 0x04060E, 150, 0x000000, 0.0f },
	{ "bg_t7", "tex_dirt_t7", "tex_rock_t7", 0x140604, 150, 0x000000, 0.0f },
	{ "bg_t8", "tex_dirt_t8", "tex_rock_t8", 0x080A04, 150, 0x000000, 0.0f },
	{ "bg_t9", "tex_dirt_t9", "tex_rock_t9", 0x120608, 150, 0x000000, 0.0f },
};
Theme const* theme = &themes[0];

std::vector<WeaponLook> looks;
std::vector<char> mask; // bool per weapon
Common* lookCommon = nullptr;

Spr* get(const char* name)
{
	if (curSkin > 0)
	{
		auto sk = skinSprites.find(name);
		if (sk != skinSprites.end() && sk->second.tex) return &sk->second;
	}
	auto it = sprites.find(name);
	return it == sprites.end() || !it->second.tex ? nullptr : &it->second;
}

void rgb2hsv(float r, float g, float b, float& h, float& s, float& v)
{
	float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b)), d = mx - mn;
	v = mx; s = mx > 0 ? d / mx : 0; h = 0;
	if (d > 1e-5f)
	{
		if (mx == r) h = std::fmod((g - b) / d, 6.0f);
		else if (mx == g) h = (b - r) / d + 2;
		else h = (r - g) / d + 4;
		h *= 60; if (h < 0) h += 360;
	}
}

void hsv2rgb(float h, float s, float v, float& r, float& g, float& b)
{
	float c = v * s, x = c * (1 - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1)), m = v - c;
	float rr = 0, gg = 0, bb = 0;
	if (h < 60) { rr = c; gg = x; } else if (h < 120) { rr = x; gg = c; } else if (h < 180) { gg = c; bb = x; }
	else if (h < 240) { gg = x; bb = c; } else if (h < 300) { rr = x; bb = c; } else { rr = c; bb = x; }
	r = rr + m; g = gg + m; b = bb + m;
}

SDL_Texture* makeTex(SDL_Renderer* r, std::vector<uint32_t> const& px, int w, int h, bool linear = true)
{
	SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
	if (!t) return nullptr;
	SDL_UpdateTexture(t, NULL, px.data(), w * 4);
	SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(t, linear ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	return t;
}

// Recolour the red worm to the worm's own colour (hue shift of saturated reds).
SDL_Texture* recolor(SDL_Renderer* r, Spr const& s, int R, int G, int B)
{
	float th, ts, tv;
	rgb2hsv(R / 255.0f, G / 255.0f, B / 255.0f, th, ts, tv);
	std::vector<uint32_t> out(s.px.size());
	for (std::size_t i = 0; i < s.px.size(); ++i)
	{
		uint32_t p = s.px[i];
		uint32_t a = p >> 24;
		float rr = ((p >> 16) & 255) / 255.0f, gg = ((p >> 8) & 255) / 255.0f, bb = (p & 255) / 255.0f;
		float h, sat, v;
		rgb2hsv(rr, gg, bb, h, sat, v);
		// skins have skin tones / helmets / noses next to the red: only strong reds are team-coloured
		bool red = curSkin > 0 ? ((h < 14 || h > 345) && sat > 0.5f) : ((h < 40 || h > 330) && sat > 0.22f);
		if (red && a)
		{
			float nh = th, ns = std::min(1.0f, sat * (0.35f + ts * 0.75f));
			hsv2rgb(nh, ns, v, rr, gg, bb);
			p = (a << 24) | (uint32_t(rr * 255) << 16) | (uint32_t(gg * 255) << 8) | uint32_t(bb * 255);
		}
		out[i] = p;
	}
	return makeTex(r, out, s.w, s.h);
}

void buildLooks(Common& common)
{
	if (lookCommon == &common && looks.size() == common.weapons.size())
		return;
	lookCommon = &common;
	looks.assign(common.weapons.size(), WeaponLook());
	mask.assign(common.weapons.size(), 0);
	struct Def { const char* id; const char* proj; float size; int rot; const char* gun; float gunLen; };
	static const Def defs[] = {
		{ "bazooka", "p_rocket", 10, 1, "gun_bazooka", 13 },
		{ "missile", "p_missile", 11, 1, "gun_bazooka", 13 },
		{ "mini_rockets", "p_minirocket", 7, 1, "gun_bazooka", 13 },
		{ "hellraider", "p_hellraider", 9, 1, "gun_bazooka", 13 },
		{ "cannon", "p_cannonball", 6, 2, "gun_cannon", 12 },
		{ "dart", "p_dart", 8, 1, "gun_pistol", 8 },
		{ "explosives", "p_dynamite", 8, 2, "gun_glauncher", 11 },
		{ "chiquita_bomb", "p_banana", 8, 2, "gun_glauncher", 11 },
		{ "spikeballs", "p_spikeball", 6, 2, "gun_glauncher", 11 },
		{ "greenball", "p_greenball", 5, 0, "gun_laser", 11 },
		{ "larpa", "p_larpa", 7, 2, "gun_laser", 11 },
		{ "bouncy_larpa", "p_larpa", 7, 2, "gun_laser", 11 },
		{ "zimm", "p_zimm", 5, 2, "gun_laser", 11 },
		{ "crackler", "p_crackler", 8, 1, "gun_glauncher", 11 },
		{ "float_mine", "p_floatmine", 9, 0, "gun_glauncher", 11 },
		{ "bouncy_mine", "p_bouncymine", 8, 0, "gun_glauncher", 11 },
		{ "booby_trap", "bonus_weapon", 7, 0, "gun_glauncher", 11 }, // disguised as a bonus, like the original
		{ "grasshopper", "p_grasshopper", 8, 0, "gun_glauncher", 11 },
		{ "mini_nuke", "p_mininuke", 8, 1, "gun_cannon", 12 },
		{ "big_nuke", "p_bignuke", 12, 1, "gun_cannon", 13 },
		{ "doomsday", "p_doomsday", 14, 1, "gun_cannon", 13 },
		{ "napalm", "p_fireball", 7, 1, "gun_flamer", 12 },
		{ "flamer", "p_flame1", 6, 1, "gun_flamer", 12 },
		{ "dirtball", "p_dirt", 6, 2, "gun_glauncher", 11 },
		{ "blaster", "p_blaster", 9, 1, "gun_laser", 11 },
		{ "grenade", "p_grenade", 6, 2, "gun_glauncher", 11 },
		{ "cluster_bomb", "p_cluster", 7, 2, "gun_glauncher", 11 },
		{ "mine", "p_mine", 8, 0, "gun_glauncher", 11 },
		// bullet / beam weapons: original pixels for the shots, HD gun in hand
		{ "handgun", nullptr, 0, 0, "gun_pistol", 7 },
		{ "uzi", nullptr, 0, 0, "gun_pistol", 8 },
		{ "rifle", nullptr, 0, 0, "gun_rifle", 13 },
		{ "winchester", nullptr, 0, 0, "gun_rifle", 13 },
		{ "shotgun", nullptr, 0, 0, "gun_shotgun", 12 },
		{ "super_shotgun", nullptr, 0, 0, "gun_shotgun", 12 },
		{ "rb_rampage", nullptr, 0, 0, "gun_shotgun", 12 },
		{ "minigun", nullptr, 0, 0, "gun_minigun", 12 },
		{ "chaingun", nullptr, 0, 0, "gun_mgun", 12 },
		{ "laser", nullptr, 0, 0, "gun_laser", 11 },
		{ "gauss_gun", nullptr, 0, 0, "gun_laser", 12 },
		{ "fan", nullptr, 0, 0, "gun_flamer", 12 },
	};
	for (std::size_t i = 0; i < common.weapons.size(); ++i)
	{
		std::string id = common.weapons[i].idStr;
		for (auto const& d : defs)
			if (id == d.id)
			{
				WeaponLook& l = looks[i];
				l.proj = d.proj; l.size = d.size; l.rot = d.rot; l.gun = d.gun; l.gunLen = d.gunLen;
				mask[i] = d.proj && get(d.proj) ? 1 : 0;
			}
	}
}

inline float fx(fixed v) { return v / 65536.0f; }

void drawSpr(SDL_Renderer* r, SDL_Texture* tex, int w, int h, float cx, float cy, float longSide, float angleDeg,
	SDL_RendererFlip flip = SDL_FLIP_NONE, float alpha = 1.0f)
{
	if (!tex) return;
	float k = longSide / std::max(w, h);
	SDL_FRect d = { cx - w * k / 2, cy - h * k / 2, w * k, h * k };
	if (alpha < 1.0f) SDL_SetTextureAlphaMod(tex, (Uint8)(alpha * 255));
	SDL_RenderCopyExF(r, tex, NULL, &d, angleDeg, NULL, flip);
	if (alpha < 1.0f) SDL_SetTextureAlphaMod(tex, 255);
}

void regenTerrain(Game& g, int x0, int y0, int x1, int y1)
{
	Level& lv = g.level;
	int W = lv.width, H = lv.height;
	x0 = std::max(0, x0 - 1); y0 = std::max(0, y0 - 1);
	x1 = std::min(W, x1 + 1); y1 = std::min(H, y1 + 1);
	if (x1 <= x0 || y1 <= y0) return;
	Spr* dirt = get(theme->dirt);
	Spr* rock = get(theme->rock);
	if (!dirt || dirt->px.empty()) dirt = get("tex_dirt_t0");
	if (!rock || rock->px.empty()) rock = get("tex_rock_t0");
	if (!dirt || !rock || dirt->px.empty() || rock->px.empty()) return;
	int ow = (x1 - x0) * T, oh = (y1 - y0) * T;
	genBuf.assign(std::size_t(ow) * oh, 0);
	auto solid = [&](int x, int y) -> float {
		if (x < 0 || y < 0 || x >= W || y >= H) return 1.0f;
		return lv.materials[y * W + x].background() ? 0.0f : 1.0f;
	};
	for (int oy = 0; oy < oh; ++oy)
	{
		int Y = y0 * T + oy;
		float v = (Y + 0.5f) / T - 0.5f;
		int ly = (int)std::floor(v);
		float fy = v - ly;
		for (int ox = 0; ox < ow; ++ox)
		{
			int X = x0 * T + ox;
			float u = (X + 0.5f) / T - 0.5f;
			int lx = (int)std::floor(u);
			float fxx = u - lx;
			float s = (solid(lx, ly) * (1 - fxx) + solid(lx + 1, ly) * fxx) * (1 - fy)
				+ (solid(lx, ly + 1) * (1 - fxx) + solid(lx + 1, ly + 1) * fxx) * fy;
			int nx = std::min(W - 1, std::max(0, (int)(X / T))), ny = std::min(H - 1, std::max(0, (int)(Y / T)));
			uint32_t out = 0;
			if (s >= 0.5f)
			{
				// material: nearest solid pixel
				Material m = lv.materials[ny * W + nx];
				if (m.background())
				{
					for (int dy = -1; dy <= 1 && m.background(); ++dy)
						for (int dx = -1; dx <= 1 && m.background(); ++dx)
						{
							int qx = nx + dx, qy = ny + dy;
							if (qx >= 0 && qy >= 0 && qx < W && qy < H && !lv.materials[qy * W + qx].background())
								m = lv.materials[qy * W + qx];
						}
				}
				Spr* t = m.rock() ? rock : dirt;
				uint32_t c = t->px[(Y % t->h) * t->w + (X % t->w)];
				float k = std::min(1.0f, (s - 0.5f) * 2.6f);  // 0 at the edge .. 1 inside
				float shade = s < 0.58f ? 0.18f : (0.55f + 0.3f * k);
				// top-lit bevel: brighter where air is above
				float above = solid(nx, ny - 1);
				if (above < 0.5f && k < 0.9f) shade *= 1.18f;
				int rr = std::min(255, (int)(((c >> 16) & 255) * shade));
				int gg = std::min(255, (int)(((c >> 8) & 255) * shade));
				int bb = std::min(255, (int)((c & 255) * shade));
				if (s < 0.6f && theme->edgeK > 0)
				{
					// themed edge: lava glow / frost line instead of a dark outline
					float e = theme->edgeK;
					rr = (int)(rr * (1 - e) + ((theme->edge >> 16) & 255) * e);
					gg = (int)(gg * (1 - e) + ((theme->edge >> 8) & 255) * e);
					bb = (int)(bb * (1 - e) + (theme->edge & 255) * e);
				}
				out = 0xFF000000u | (rr << 16) | (gg << 8) | bb;
			}
			else
			{
				// Tunnel back wall: the dirt texture, darkened and at half frequency, so
				// open areas read as one calm surface (no busy picture behind the play).
				uint32_t c = dirt->px[((Y / 2) % dirt->h) * dirt->w + ((X / 2) % dirt->w)];
				float sh = 0.30f;
				if (s > 0.32f) sh *= 1.0f - (s - 0.32f) / 0.18f * 0.6f; // ambient occlusion near the edge
				int rr = (int)(((c >> 16) & 255) * sh), gg = (int)(((c >> 8) & 255) * sh), bb = (int)((c & 255) * sh);
				// cool tint so air never looks like solid dirt
				rr = rr * 85 / 100; gg = gg * 90 / 100; bb = std::min(255, bb * 105 / 100 + 6);
				out = 0xFF000000u | (rr << 16) | (gg << 8) | bb;
			}
			genBuf[std::size_t(oy) * ow + ox] = out;
		}
	}
	SDL_Rect rc = { x0 * T, y0 * T, ow, oh };
	SDL_UpdateTexture(terrainTex, &rc, genBuf.data(), ow * 4);
}

void updateTerrain(Game& g)
{
	Level& lv = g.level;
	int W = lv.width, H = lv.height;
	std::size_t n = std::size_t(W) * H;
	if (levelCache.size() != n)
	{
		levelCache.assign(lv.data.begin(), lv.data.end());
		regenTerrain(g, 0, 0, W, H);
		return;
	}
	int minX = W, minY = H, maxX = -1, maxY = -1;
	for (int y = 0; y < H; ++y)
	{
		unsigned char const* a = &lv.data[std::size_t(y) * W];
		unsigned char* b = &levelCache[std::size_t(y) * W];
		if (std::memcmp(a, b, W) == 0) continue;
		for (int x = 0; x < W; ++x)
			if (a[x] != b[x]) { minX = std::min(minX, x); maxX = std::max(maxX, x); }
		minY = std::min(minY, y); maxY = y;
		std::memcpy(b, a, W);
	}
	if (maxX >= 0)
		regenTerrain(g, minX, minY, maxX + 1, maxY + 1);
}

} // anon

bool ready() { return loaded; }

namespace {
bool readFile(SDL_Renderer* r, char const* path, std::map<std::string, Spr>& into)
{
	SDL_RWops* f = SDL_RWFromFile(path, "rb");
	if (!f) { SDL_Log("liero: hd assets not found: %s", path); return false; }
	char magic[4];
	if (SDL_RWread(f, magic, 1, 4) != 4 || std::memcmp(magic, "LHD1", 4) != 0) { SDL_RWclose(f); return false; }
	int count = SDL_ReadLE16(f);
	std::vector<unsigned char> z, raw;
	for (int i = 0; i < count; ++i)
	{
		Uint8 nl = 0;
		SDL_RWread(f, &nl, 1, 1);
		std::string name(nl, ' ');
		SDL_RWread(f, &name[0], 1, nl);
		Spr s;
		s.w = SDL_ReadLE16(f); s.h = SDL_ReadLE16(f);
		s.ax = (int16_t)SDL_ReadLE16(f); s.ay = (int16_t)SDL_ReadLE16(f);
		Uint32 zl = SDL_ReadLE32(f);
		z.resize(zl);
		SDL_RWread(f, z.data(), 1, zl);
		unsigned long rl = (unsigned long)s.w * s.h * 4;
		raw.resize(rl);
		if (mz_uncompress(raw.data(), &rl, z.data(), zl) != 0) { SDL_Log("liero: hd decode failed %s", name.c_str()); continue; }
		std::vector<uint32_t> px(std::size_t(s.w) * s.h);
		for (std::size_t k = 0; k < px.size(); ++k)
			px[k] = (uint32_t(raw[k * 4 + 3]) << 24) | (uint32_t(raw[k * 4]) << 16) | (uint32_t(raw[k * 4 + 1]) << 8) | raw[k * 4 + 2];
		bool keep = name.compare(0, 5, "worm_") == 0 || name.compare(0, 4, "tex_") == 0;
		s.tex = makeTex(r, px, s.w, s.h);
		if (keep) s.px.swap(px);
		into[name] = std::move(s);
	}
	SDL_RWclose(f);
	SDL_Log("liero: hd assets loaded from %s (%d)", path, (int)into.size());
	return !into.empty();
}

// Load the sprites of the skin chosen in the settings (Visual >= 2).
void ensureSkin()
{
	int want = std::max(0, std::min(SkinCount - 1, config.smooth - 2));
	if (want == curSkin || !rend) return;
	for (auto& kv : skinSprites)
		if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
	skinSprites.clear();
	curSkin = want;
	if (want > 0 && !readFile(rend, skinFiles[want], skinSprites))
		curSkin = 0;
	lookCommon = nullptr; // weapon looks are rebuilt (sprite sizes differ)
}
} // anon

bool load(SDL_Renderer* r, char const* path)
{
	if (loaded) return true;
	rend = r;
	loaded = readFile(r, path, sprites);
	return loaded;
}

void newGame(SDL_Renderer* r, Game& g)
{
	if (!loaded) return;
	curGame = &g;
	ensureSkin();
	theme = &themes[std::max(0, std::min(ThemeCount - 1, config.theme))];
	buildLooks(*g.common);
	for (int w = 0; w < 2; ++w)
		for (int f = 0; f < 8; ++f)
		{
			if (wormTex[w][f]) { SDL_DestroyTexture(wormTex[w][f]); wormTex[w][f] = nullptr; }
			Spr* s = get(wormFrames[f]);
			if (!s || s->px.empty() || (int)g.worms.size() <= w) continue;
			WormSettings const& ws = *g.worms[w]->settings;
			wormTex[w][f] = recolor(r, *s, ws.rgb[0] * 4 + 3, ws.rgb[1] * 4 + 3, ws.rgb[2] * 4 + 3);
		}
	int W = g.level.width, H = g.level.height;
	if (!terrainTex || terrW != W * T || terrH != H * T)
	{
		if (terrainTex) SDL_DestroyTexture(terrainTex);
		terrW = W * T; terrH = H * T;
		terrainTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, terrW, terrH);
		SDL_SetTextureBlendMode(terrainTex, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(terrainTex, SDL_ScaleModeLinear);
	}
	levelCache.clear();
	lastHealth[0] = lastHealth[1] = 0;
	hurtTimer[0] = hurtTimer[1] = 0;
}

void drawBackground(SDL_Renderer* r, Game& g, Camera const& cam)
{
	Spr* bg = get(theme->bg);
	if (!bg) bg = get("bg_t0");
	if (!bg) return;
	// cover the screen, gentle parallax with the camera
	float levelW = (float)g.level.width, levelH = (float)g.level.height;
	float k = std::max(cam.screenW * 1.25f / bg->w, cam.screenH * 1.25f / bg->h);
	float bw = bg->w * k, bh = bg->h * k;
	float px = levelW > cam.viewW ? cam.x / (levelW - cam.viewW) : 0.5f;
	float py = levelH > cam.viewH ? cam.y / (levelH - cam.viewH) : 0.5f;
	px = std::max(0.0f, std::min(1.0f, px));
	py = std::max(0.0f, std::min(1.0f, py));
	SDL_FRect d = { -(bw - cam.screenW) * px, -(bh - cam.screenH) * py, bw, bh };
	SDL_RenderCopyF(r, bg->tex, NULL, &d);
	// only seen past the map border now: keep it dim so it doesn't compete
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(r, 6, 6, 16, 120);
	SDL_RenderFillRectF(r, NULL);
}

void drawTerrain(SDL_Renderer* r, Game& g, Camera const& cam)
{
	if (!terrainTex || curGame != &g) return;
	updateTerrain(g);
	// visible part of the level (the camera may look past the border)
	float x0 = std::max(0.0f, cam.x), y0 = std::max(0.0f, cam.y);
	float x1 = std::min((float)g.level.width, cam.x + cam.viewW), y1 = std::min((float)g.level.height, cam.y + cam.viewH);
	if (x1 <= x0 || y1 <= y0) return;
	SDL_Rect src = { (int)(x0 * T), (int)(y0 * T), (int)((x1 - x0) * T), (int)((y1 - y0) * T) };
	SDL_FRect dst = { cam.ox + (x0 - cam.x) * cam.scale, cam.oy + (y0 - cam.y) * cam.scale, (x1 - x0) * cam.scale, (y1 - y0) * cam.scale };
	SDL_RenderCopyF(r, terrainTex, &src, &dst);
	// rocky frame around the level border
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(r, 12, 8, 24, 200);
	float t = 3 * cam.scale;
	SDL_FRect edges[4] = {
		{ dst.x - t, dst.y - t, dst.w + 2 * t, t }, { dst.x - t, dst.y + dst.h, dst.w + 2 * t, t },
		{ dst.x - t, dst.y, t, dst.h }, { dst.x + dst.w, dst.y, t, dst.h } };
	if (cam.x < 0 || cam.y < 0 || cam.x + cam.viewW > g.level.width || cam.y + cam.viewH > g.level.height)
		SDL_RenderFillRectsF(r, edges, 4);
}

void drawObjects(SDL_Renderer* r, Game& g, Camera const& cam)
{
	if (!loaded || curGame != &g) return;
	Common& common = *g.common;
	buildLooks(common);
	auto sx = [&](float lx) { return cam.ox + (lx - cam.x) * cam.scale; };
	auto sy = [&](float ly) { return cam.oy + (ly - cam.y) * cam.scale; };
	float S = cam.scale;

	// Bonuses (health / weapon crates)
	{
		Spr* hp = get("bonus_health");
		Spr* wp = get("bonus_weapon");
		auto br = g.bonuses.all();
		for (Bonus* b; (b = br.next()); )
		{
			if (!(b->timer > LC(BonusFlickerTime) || (g.cycles & 3) == 0)) continue;
			Spr* s = b->frame == 0 ? wp : hp;
			if (s) drawSpr(r, s->tex, s->w, s->h, sx(fx(b->x)), sy(fx(b->y)), 9 * S, 0);
		}
	}

	// Projectiles with an HD look
	{
		auto wr = g.wobjects.all();
		for (WObject* o; (o = wr.next()); )
		{
			int wi = int(o->type - &common.weapons[0]);
			if (wi < 0 || wi >= (int)looks.size() || !mask[wi]) continue;
			WeaponLook const& l = looks[wi];
			const char* name = l.proj;
			if (!std::strcmp(name, "p_mine") && ((g.cycles / 12) & 1)) name = "p_mine_lit";
			if (!std::strcmp(name, "p_flame1") && ((g.cycles / 4 + (int)(o - g.wobjects.arr)) & 1)) name = "p_flame2";
			Spr* s = get(name);
			if (!s) continue;
			float ang = 0;
			if (l.rot == 1)
			{
				float vx = fx(o->vel.x), vy = fx(o->vel.y);
				if (vx * vx + vy * vy > 1e-6f) ang = std::atan2(vy, vx) * 180 / Pi;
			}
			else if (l.rot == 2)
				ang = (float)((g.cycles * 9 + (int)(o - g.wobjects.arr) * 37) % 360);
			drawSpr(r, s->tex, s->w, s->h, sx(fx(o->pos.x)), sy(fx(o->pos.y)), l.size * S, ang);
		}
	}

	// Worms + guns
	for (auto* wp : g.worms)
	{
		Worm const& w = *wp;
		int wi = w.index & 1;
		if (lastHealth[wi] > 0 && w.health < lastHealth[wi]) hurtTimer[wi] = 18;
		lastHealth[wi] = w.health;
		if (hurtTimer[wi] > 0) --hurtTimer[wi];
		if (!w.visible) continue;

		fixedvec aim = cossinTable[ftoi(w.aimingAngle) & 127];
		float ax = fx(aim.x), ay = fx(aim.y);
		bool right = ax >= 0;
		bool onGround = w.reacts[Worm::RFDown] > 0;
		float vx = fx(w.vel.x);
		int frame = 0;
		if (hurtTimer[wi] > 0) frame = 6;
		else if (w.ninjarope.attached || !onGround) frame = 5; // on the rope: jump pose, the game draws the rope
		else if (std::fabs(vx) > 0.05f) frame = 1 + ((int)(fx(w.pos.x) / 2.5f) & 3);
		SDL_Texture* t = wormTex[wi][frame];
		Spr* base = get(wormFrames[frame]);
		Spr* idle = get("worm_idle");
		if (!t || !base || !idle) continue;
		float k = (curSkin > 0 ? 14.0f : 16.0f) * S / idle->h; // same scale for all frames
		float px = sx(fx(w.pos.x)), feet = sy(fx(w.pos.y) + 5.0f);
		SDL_FRect d = { px - base->ax * k, feet - base->ay * k, base->w * k, base->h * k };
		if (!right) d.x = px - (base->w - base->ax) * k;
		SDL_RenderCopyExF(r, t, NULL, &d, 0, NULL, right ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
	

		// gun
		WormWeapon const& ww = w.weapons[w.currentWeapon];
		if (!ww.type || frame == 6) continue;
		int widx = int(ww.type - &common.weapons[0]);
		WeaponLook const& l = (widx >= 0 && widx < (int)looks.size()) ? looks[widx] : WeaponLook();
		Spr* gun = get(l.gun);
		if (!gun) continue;
		float gk = l.gunLen * 0.72f * S / gun->w;
		float gx = px + (right ? 1.5f : -1.5f) * S, gy = sy(fx(w.pos.y) - 0.5f);
		SDL_FRect gd = { gx - gun->ax * gk, gy - gun->ay * gk, gun->w * gk, gun->h * gk };
		SDL_FPoint c = { gun->ax * gk, gun->ay * gk };
		float ang = std::atan2(ay, ax) * 180 / Pi;
		SDL_RenderCopyExF(r, gun->tex, NULL, &gd, ang, &c, right ? SDL_FLIP_NONE : SDL_FLIP_VERTICAL);
		// muzzle flash while firing
		if (w.fireCone > 0)
		{
			Spr* sp = get("spark");
			float mlen = (gun->w - gun->ax) * gk;
			if (sp) drawSpr(r, sp->tex, sp->w, sp->h, gx + ax * mlen, gy + ay * mlen, 7 * S, ang);
		}
	}

	// Explosions / smoke / flashes
	{
		static const char* expl[6] = { "exp1", "exp2", "exp3", "exp4", "exp5", "exp6" };
		static const char* smoke[3] = { "smoke1", "smoke2", "smoke3" };
		auto sr = g.sobjects.all();
		for (SObject* o; (o = sr.next()); )
		{
			SObjectType const& t = common.sobjectTypes[o->id];
			float prog = t.numFrames > 0 ? std::min(0.999f, o->curFrame / float(t.numFrames + 1)) : 0;
			float size; int kind = 0; // 0 explosion, 1 smoke, 2 flash
			switch (o->id)
			{
			case 0: size = 30; break;
			case 1: size = 20; break;
			case 2: case 8: size = 12; break;
			case 3: size = 14; kind = 1; break;
			case 4: case 7: size = 12; kind = 2; break;
			case 5: size = 24; kind = 1; break;
			case 9: size = 7; break;
			case 10: size = 16; break;
			case 11: size = 24; break;
			case 12: size = 24; break;
			default: continue; // flashing pixel / unknown: original pixels
			}
			const char* name = kind == 1 ? smoke[(int)(prog * 3)] : kind == 2 ? "spark" : expl[(int)(prog * 6)];
			Spr* s = get(name);
			if (!s) continue;
			float a = kind == 1 ? 0.8f : 1.0f;
			drawSpr(r, s->tex, s->w, s->h, sx(o->x + 8.0f), sy(o->y + 8.0f), size * S, 0, SDL_FLIP_NONE, a);
		}
	}
}

SDL_Texture* sprite(char const* name, int* w, int* h)
{
	Spr* s = get(name);
	if (!s) return nullptr;
	if (w) *w = s->w;
	if (h) *h = s->h;
	return s->tex;
}

bool const* weaponMask(Common& common)
{
	if (!loaded) return nullptr;
	buildLooks(common);
	return reinterpret_cast<bool const*>(mask.data());
}

SDL_Texture* weaponIcon(Common& common, int wi, int* w, int* h)
{
	if (!loaded) return nullptr;
	ensureSkin();
	buildLooks(common);
	if (wi < 0 || wi >= (int)looks.size()) return nullptr;
	Spr* s = looks[wi].proj ? get(looks[wi].proj) : nullptr;
	if (!s) s = get(looks[wi].gun);
	if (!s) return nullptr;
	if (w) *w = s->w;
	if (h) *h = s->h;
	return s->tex;
}

} // namespace hd
} // namespace mobile
