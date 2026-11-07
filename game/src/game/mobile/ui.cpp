#include "ui.hpp"

#include <cmath>
#include <cstring>
#include <map>
#include <vector>
#include <algorithm>

namespace mobile {
namespace ui {

namespace {

const float Pi = 3.14159265f;

struct Glyph { int x, y, w, h, xoff, yoff, adv; };

SDL_Texture* fontTex = nullptr;
std::map<uint32_t, Glyph> glyphs;
int atlasEm = 56;
int lineH = 79, ascent = 59;

uint32_t nextCp(std::string const& s, std::size_t& i)
{
	unsigned char c = (unsigned char)s[i++];
	if (c < 0x80) return c;
	int n = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
	uint32_t cp = c & (0x3F >> n);
	for (int k = 0; k < n && i < s.size(); ++k)
		cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
	return cp;
}

void setColor(SDL_Vertex& v, SDL_Color c) { v.color = c; v.tex_coord = { 0, 0 }; }

} // anon

SDL_Color rgba(int r, int g, int b, float a)
{
	SDL_Color c;
	c.r = (Uint8)std::max(0, std::min(255, r));
	c.g = (Uint8)std::max(0, std::min(255, g));
	c.b = (Uint8)std::max(0, std::min(255, b));
	c.a = (Uint8)std::max(0.0f, std::min(255.0f, a * 255.0f));
	return c;
}

bool init(SDL_Renderer* r, char const* path)
{
	if (fontTex)
		return true;
	SDL_RWops* f = SDL_RWFromFile(path, "rb");
	if (!f)
	{
		SDL_Log("liero: ui font not found: %s", path);
		return false;
	}
	char magic[4];
	if (SDL_RWread(f, magic, 1, 4) != 4 || std::memcmp(magic, "LFNT", 4) != 0) { SDL_RWclose(f); return false; }
	Uint16 w = SDL_ReadLE16(f), h = SDL_ReadLE16(f);
	lineH = SDL_ReadLE16(f); ascent = SDL_ReadLE16(f);
	Uint16 count = SDL_ReadLE16(f);
	for (int i = 0; i < count; ++i)
	{
		uint32_t cp = SDL_ReadLE32(f);
		Glyph g;
		g.x = SDL_ReadLE16(f); g.y = SDL_ReadLE16(f); g.w = SDL_ReadLE16(f); g.h = SDL_ReadLE16(f);
		g.xoff = (int16_t)SDL_ReadLE16(f); g.yoff = (int16_t)SDL_ReadLE16(f); g.adv = SDL_ReadLE16(f);
		glyphs[cp] = g;
	}
	std::vector<Uint8> alpha(std::size_t(w) * h);
	SDL_RWread(f, alpha.data(), 1, alpha.size());
	SDL_RWclose(f);
	std::vector<Uint32> px(alpha.size());
	for (std::size_t i = 0; i < alpha.size(); ++i)
		px[i] = (Uint32(alpha[i]) << 24) | 0x00FFFFFFu;
	fontTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
	if (!fontTex) return false;
	SDL_UpdateTexture(fontTex, NULL, px.data(), w * 4);
	SDL_SetTextureBlendMode(fontTex, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(fontTex, SDL_ScaleModeLinear);
	return true;
}

bool ready() { return fontTex != nullptr; }

float textWidth(std::string const& s, float px)
{
	float k = px / atlasEm, w = 0;
	for (std::size_t i = 0; i < s.size();)
	{
		auto it = glyphs.find(nextCp(s, i));
		w += (it != glyphs.end() ? it->second.adv : atlasEm / 3) * k;
	}
	return w;
}

static void drawRun(SDL_Renderer* r, std::string const& s, float x, float y, float k, SDL_Color c)
{
	SDL_SetTextureColorMod(fontTex, c.r, c.g, c.b);
	SDL_SetTextureAlphaMod(fontTex, c.a);
	for (std::size_t i = 0; i < s.size();)
	{
		auto it = glyphs.find(nextCp(s, i));
		if (it == glyphs.end()) { x += atlasEm / 3 * k; continue; }
		Glyph const& g = it->second;
		if (g.w > 1)
		{
			SDL_Rect src = { g.x, g.y, g.w, g.h };
			SDL_FRect dst = { x + g.xoff * k, y + g.yoff * k, g.w * k, g.h * k };
			SDL_RenderCopyF(r, fontTex, &src, &dst);
		}
		x += g.adv * k;
	}
}

void text(SDL_Renderer* r, std::string const& s, float x, float y, float px, SDL_Color c, int align, bool shadow)
{
	if (!fontTex || s.empty()) return;
	float k = px / atlasEm;
	float w = textWidth(s, px);
	if (align == Center) x -= w / 2;
	else if (align == Right) x -= w;
	// y is the vertical centre of the text line
	float top = y - ascent * k * 0.62f;
	if (shadow)
		drawRun(r, s, x + px * 0.05f, top + px * 0.07f, k, rgba(0, 0, 0, c.a / 255.0f * 0.55f));
	drawRun(r, s, x, top, k, c);
}

void fillCircle(SDL_Renderer* r, float cx, float cy, float rad, SDL_Color c)
{
	int seg = std::max(16, std::min(64, (int)(rad / 2.5f)));
	std::vector<SDL_Vertex> v(seg + 2);
	std::vector<int> idx(seg * 3);
	v[0].position = { cx, cy }; setColor(v[0], c);
	for (int i = 0; i <= seg; ++i)
	{
		float a = (float)i / seg * 2 * Pi;
		v[i + 1].position = { cx + std::cos(a) * rad, cy + std::sin(a) * rad };
		setColor(v[i + 1], c);
	}
	for (int i = 0; i < seg; ++i) { idx[i * 3] = 0; idx[i * 3 + 1] = i + 1; idx[i * 3 + 2] = i + 2; }
	SDL_RenderGeometry(r, NULL, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

void ring(SDL_Renderer* r, float cx, float cy, float rad, float thick, SDL_Color c)
{
	int seg = std::max(20, std::min(72, (int)(rad / 2.0f)));
	std::vector<SDL_Vertex> v;
	std::vector<int> idx;
	v.reserve((seg + 1) * 2);
	for (int i = 0; i <= seg; ++i)
	{
		float a = (float)i / seg * 2 * Pi, ca = std::cos(a), sa = std::sin(a);
		SDL_Vertex o, in;
		o.position = { cx + ca * rad, cy + sa * rad };
		in.position = { cx + ca * (rad - thick), cy + sa * (rad - thick) };
		setColor(o, c); setColor(in, c);
		v.push_back(o); v.push_back(in);
		if (i < seg) { int b = i * 2; idx.insert(idx.end(), { b, b + 1, b + 2, b + 1, b + 3, b + 2 }); }
	}
	SDL_RenderGeometry(r, NULL, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

void tri(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_Color col)
{
	SDL_Vertex v[3];
	v[0].position = a; v[1].position = b; v[2].position = c;
	for (auto& x : v) setColor(x, col);
	SDL_RenderGeometry(r, NULL, v, 3, NULL, 0);
}

void line(SDL_Renderer* r, float x0, float y0, float x1, float y1, float t, SDL_Color c)
{
	float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
	if (len < 0.001f) return;
	float nx = -dy / len * t * 0.5f, ny = dx / len * t * 0.5f;
	tri(r, { x0 + nx, y0 + ny }, { x0 - nx, y0 - ny }, { x1 - nx, y1 - ny }, c);
	tri(r, { x0 + nx, y0 + ny }, { x1 - nx, y1 - ny }, { x1 + nx, y1 + ny }, c);
	fillCircle(r, x0, y0, t * 0.5f, c);
	fillCircle(r, x1, y1, t * 0.5f, c);
}

static void roundPath(SDL_FRect rc, float rad, std::vector<SDL_FPoint>& pts)
{
	rad = std::min(rad, std::min(rc.w, rc.h) / 2);
	const int seg = 8;
	float cx[4] = { rc.x + rc.w - rad, rc.x + rad, rc.x + rad, rc.x + rc.w - rad };
	float cy[4] = { rc.y + rad, rc.y + rad, rc.y + rc.h - rad, rc.y + rc.h - rad };
	float a0[4] = { 0, -Pi / 2, Pi, Pi / 2 };
	for (int c = 0; c < 4; ++c)
		for (int i = 0; i <= seg; ++i)
		{
			float a = a0[c] - (float)i / seg * (Pi / 2);
			// go counter-clockwise visually: top-right -> top-left -> bottom-left -> bottom-right
			pts.push_back({ cx[c] + std::cos(a) * rad, cy[c] + std::sin(a) * rad });
		}
}

void roundRectV(SDL_Renderer* r, SDL_FRect rc, float rad, SDL_Color top, SDL_Color bottom)
{
	std::vector<SDL_FPoint> pts;
	roundPath(rc, rad, pts);
	std::vector<SDL_Vertex> v(pts.size() + 1);
	std::vector<int> idx;
	SDL_Color mid = { (Uint8)((top.r + bottom.r) / 2), (Uint8)((top.g + bottom.g) / 2), (Uint8)((top.b + bottom.b) / 2), (Uint8)((top.a + bottom.a) / 2) };
	v[0].position = { rc.x + rc.w / 2, rc.y + rc.h / 2 }; setColor(v[0], mid);
	for (std::size_t i = 0; i < pts.size(); ++i)
	{
		float t = rc.h > 0 ? (pts[i].y - rc.y) / rc.h : 0;
		SDL_Color c = { (Uint8)(top.r + (bottom.r - top.r) * t), (Uint8)(top.g + (bottom.g - top.g) * t),
			(Uint8)(top.b + (bottom.b - top.b) * t), (Uint8)(top.a + (bottom.a - top.a) * t) };
		v[i + 1].position = pts[i]; setColor(v[i + 1], c);
	}
	for (std::size_t i = 0; i < pts.size(); ++i)
		idx.insert(idx.end(), { 0, (int)i + 1, (int)((i + 1) % pts.size()) + 1 });
	SDL_RenderGeometry(r, NULL, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

void roundRect(SDL_Renderer* r, SDL_FRect rc, float rad, SDL_Color c) { roundRectV(r, rc, rad, c, c); }

void roundRectOutline(SDL_Renderer* r, SDL_FRect rc, float rad, float t, SDL_Color c)
{
	std::vector<SDL_FPoint> o, in;
	roundPath(rc, rad, o);
	SDL_FRect ri = { rc.x + t, rc.y + t, rc.w - 2 * t, rc.h - 2 * t };
	roundPath(ri, std::max(0.0f, rad - t), in);
	std::vector<SDL_Vertex> v;
	std::vector<int> idx;
	std::size_t n = o.size();
	for (std::size_t i = 0; i < n; ++i)
	{
		SDL_Vertex a, b;
		a.position = o[i]; b.position = in[i];
		setColor(a, c); setColor(b, c);
		v.push_back(a); v.push_back(b);
	}
	for (std::size_t i = 0; i < n; ++i)
	{
		int a = (int)i * 2, b = (int)((i + 1) % n) * 2;
		idx.insert(idx.end(), { a, a + 1, b, a + 1, b + 1, b });
	}
	SDL_RenderGeometry(r, NULL, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

void panel(SDL_Renderer* r, SDL_FRect rc, float s, float alpha, bool glow)
{
	float rad = 14 * s;
	if (glow)
	{
		SDL_FRect g = { rc.x - 3 * s, rc.y - 3 * s, rc.w + 6 * s, rc.h + 6 * s };
		roundRectOutline(r, g, rad + 3 * s, 3 * s, rgba(60, 170, 255, alpha * 0.25f));
	}
	roundRectV(r, rc, rad, rgba(22, 34, 66, alpha), rgba(12, 18, 40, alpha));
	roundRectOutline(r, rc, rad, std::max(1.5f, 2.2f * s), rgba(90, 180, 255, alpha * 0.95f));
}

void bar(SDL_Renderer* r, SDL_FRect rc, float frac, SDL_Color a, SDL_Color b)
{
	frac = std::max(0.0f, std::min(1.0f, frac));
	float rad = rc.h / 2;
	roundRect(r, rc, rad, rgba(10, 14, 28, 0.85f));
	if (frac > 0.01f)
	{
		SDL_FRect f = { rc.x, rc.y, std::max(rc.h, rc.w * frac), rc.h };
		roundRectV(r, f, rad, a, b);
		SDL_FRect hl = { f.x + rad * 0.5f, f.y + rc.h * 0.12f, std::max(0.0f, f.w - rad), rc.h * 0.25f };
		roundRect(r, hl, hl.h / 2, rgba(255, 255, 255, 0.28f));
	}
}

void button(SDL_Renderer* r, SDL_FRect rc, float s, std::string const& label, bool primary, bool pressed, int ic, bool enabled)
{
	float rad = 16 * s;
	SDL_FRect shadow = { rc.x, rc.y + 5 * s, rc.w, rc.h };
	roundRect(r, shadow, rad, rgba(0, 0, 0, 0.45f));
	float dy = pressed ? 3 * s : 0;
	SDL_FRect b = { rc.x, rc.y + dy, rc.w, rc.h };
	float ea = enabled ? 1.0f : 0.45f;
	if (primary)
	{
		roundRectV(r, b, rad, rgba(255, 214, 70, ea), rgba(240, 160, 20, ea));
		roundRectOutline(r, b, rad, 3 * s, rgba(255, 240, 170, ea));
	}
	else
	{
		roundRectV(r, b, rad, rgba(96, 72, 190, ea), rgba(64, 44, 140, ea));
		roundRectOutline(r, b, rad, 3 * s, rgba(150, 130, 255, ea));
	}
	if (pressed)
		roundRect(r, b, rad, rgba(255, 255, 255, 0.15f));
	SDL_Color tc = primary ? rgba(60, 30, 10, ea) : rgba(255, 255, 255, ea);
	float px = std::min(b.h * 0.42f, 34 * s);
	float tw = textWidth(label, px);
	float iconW = ic != INone ? px * 1.5f : 0;
	float x0 = b.x + (b.w - tw - iconW) / 2;
	if (ic != INone)
		icon(r, ic, x0 + px * 0.55f, b.y + b.h / 2, px * 0.7f, tc);
	text(r, label, x0 + iconW, b.y + b.h / 2, px, tc, Left, !primary);
}

void wormHead(SDL_Renderer* r, float cx, float cy, float rad, SDL_Color body)
{
	SDL_Color dark = rgba(body.r * 6 / 10, body.g * 6 / 10, body.b * 6 / 10);
	fillCircle(r, cx, cy + rad * 0.08f, rad, dark);
	fillCircle(r, cx, cy, rad * 0.92f, body);
	fillCircle(r, cx - rad * 0.28f, cy - rad * 0.32f, rad * 0.22f, rgba(255, 255, 255, 0.35f));
	// angry eyes
	fillCircle(r, cx + rad * 0.05f, cy - rad * 0.05f, rad * 0.26f, rgba(255, 255, 255));
	fillCircle(r, cx + rad * 0.55f, cy - rad * 0.05f, rad * 0.24f, rgba(255, 255, 255));
	fillCircle(r, cx + rad * 0.12f, cy, rad * 0.12f, rgba(10, 10, 20));
	fillCircle(r, cx + rad * 0.6f, cy, rad * 0.11f, rgba(10, 10, 20));
	line(r, cx - rad * 0.2f, cy - rad * 0.42f, cx + rad * 0.25f, cy - rad * 0.22f, rad * 0.12f, rgba(10, 10, 20));
	line(r, cx + rad * 0.85f, cy - rad * 0.42f, cx + rad * 0.45f, cy - rad * 0.22f, rad * 0.12f, rgba(10, 10, 20));
}

void icon(SDL_Renderer* r, int ic, float x, float y, float s, SDL_Color c)
{
	float t = std::max(2.0f, s * 0.17f);
	switch (ic)
	{
	case IFire:
		for (int i = 0; i < 8; ++i)
		{
			float a = i * Pi / 4;
			float ri = (i % 2) ? s * 0.3f : s * 0.2f, ro = (i % 2) ? s * 0.62f : s * 0.85f;
			tri(r, { x + std::cos(a - 0.22f) * ri, y + std::sin(a - 0.22f) * ri }, { x + std::cos(a + 0.22f) * ri, y + std::sin(a + 0.22f) * ri },
				{ x + std::cos(a) * ro, y + std::sin(a) * ro }, c);
		}
		fillCircle(r, x, y, s * 0.33f, c);
		break;
	case IRocket:
	{
		// rocket pointing up-right
		float a = -Pi / 4, ca = std::cos(a), sa = std::sin(a);
		auto P = [&](float u, float v) { return SDL_FPoint{ x + (u * ca - v * sa) * s, y + (u * sa + v * ca) * s }; };
		tri(r, P(-0.45f, -0.2f), P(0.45f, -0.2f), P(0.45f, 0.2f), c);
		tri(r, P(-0.45f, -0.2f), P(0.45f, 0.2f), P(-0.45f, 0.2f), c);
		tri(r, P(0.45f, -0.2f), P(0.85f, 0), P(0.45f, 0.2f), c);
		tri(r, P(-0.45f, -0.2f), P(-0.75f, -0.5f), P(-0.2f, -0.2f), c);
		tri(r, P(-0.45f, 0.2f), P(-0.75f, 0.5f), P(-0.2f, 0.2f), c);
		tri(r, P(-0.5f, -0.12f), P(-0.95f, 0), P(-0.5f, 0.12f), rgba(255, 200, 80, c.a / 255.0f));
		break;
	}
	case IJump:
		for (int k = 0; k < 2; ++k)
		{
			float yy = y + s * 0.3f - k * s * 0.5f;
			line(r, x - s * 0.55f, yy + s * 0.25f, x, yy - s * 0.2f, t * 1.3f, c);
			line(r, x, yy - s * 0.2f, x + s * 0.55f, yy + s * 0.25f, t * 1.3f, c);
		}
		break;
	case IRope:
		line(r, x - s * 0.7f, y + s * 0.7f, x + s * 0.15f, y - s * 0.15f, t, c);
		ring(r, x + s * 0.3f, y - s * 0.3f, s * 0.42f, t, c);
		line(r, x + s * 0.3f, y - s * 0.72f, x + s * 0.3f, y - s * 0.95f, t, c);
		break;
	case ISwap:
	case IRestart:
		ring(r, x, y, s * 0.62f, t, c);
		tri(r, { x + s * 0.62f - s * 0.28f, y - s * 0.05f }, { x + s * 0.62f + s * 0.28f, y - s * 0.05f }, { x + s * 0.62f, y + s * 0.32f }, c);
		if (ic == ISwap)
			tri(r, { x - s * 0.62f - s * 0.28f, y + s * 0.05f }, { x - s * 0.62f + s * 0.28f, y + s * 0.05f }, { x - s * 0.62f, y - s * 0.32f }, c);
		break;
	case IDig:
		// explosion burst
		for (int i = 0; i < 10; ++i)
		{
			float a = i * Pi / 5 + 0.3f;
			float ro = (i % 2) ? s * 0.6f : s * 0.95f;
			tri(r, { x + std::cos(a - 0.3f) * s * 0.35f, y + std::sin(a - 0.3f) * s * 0.35f },
				{ x + std::cos(a + 0.3f) * s * 0.35f, y + std::sin(a + 0.3f) * s * 0.35f },
				{ x + std::cos(a) * ro, y + std::sin(a) * ro }, c);
		}
		fillCircle(r, x, y, s * 0.4f, c);
		break;
	case IOk:
		line(r, x - s * 0.55f, y, x - s * 0.12f, y + s * 0.45f, t * 1.4f, c);
		line(r, x - s * 0.12f, y + s * 0.45f, x + s * 0.6f, y - s * 0.45f, t * 1.4f, c);
		break;
	case IBack:
		line(r, x - s * 0.55f, y, x + s * 0.55f, y, t * 1.4f, c);
		line(r, x - s * 0.55f, y, x - s * 0.1f, y - s * 0.45f, t * 1.4f, c);
		line(r, x - s * 0.55f, y, x - s * 0.1f, y + s * 0.45f, t * 1.4f, c);
		break;
	case IPause:
		roundRect(r, { x - s * 0.45f, y - s * 0.55f, s * 0.3f, s * 1.1f }, s * 0.08f, c);
		roundRect(r, { x + s * 0.15f, y - s * 0.55f, s * 0.3f, s * 1.1f }, s * 0.08f, c);
		break;
	case IPlay:
		tri(r, { x - s * 0.4f, y - s * 0.55f }, { x + s * 0.6f, y }, { x - s * 0.4f, y + s * 0.55f }, c);
		break;
	case ISkull:
		fillCircle(r, x, y - s * 0.12f, s * 0.6f, c);
		roundRect(r, { x - s * 0.36f, y + s * 0.2f, s * 0.72f, s * 0.45f }, s * 0.1f, c);
		fillCircle(r, x - s * 0.24f, y - s * 0.1f, s * 0.17f, rgba(10, 14, 30));
		fillCircle(r, x + s * 0.24f, y - s * 0.1f, s * 0.17f, rgba(10, 14, 30));
		tri(r, { x, y + s * 0.08f }, { x - s * 0.08f, y + s * 0.24f }, { x + s * 0.08f, y + s * 0.24f }, rgba(10, 14, 30));
		break;
	case IHeart:
		fillCircle(r, x - s * 0.3f, y - s * 0.18f, s * 0.34f, c);
		fillCircle(r, x + s * 0.3f, y - s * 0.18f, s * 0.34f, c);
		tri(r, { x - s * 0.62f, y - s * 0.08f }, { x + s * 0.62f, y - s * 0.08f }, { x, y + s * 0.65f }, c);
		break;
	case IClock:
		ring(r, x, y + s * 0.05f, s * 0.62f, t, c);
		line(r, x, y + s * 0.05f, x, y - s * 0.3f, t, c);
		line(r, x, y + s * 0.05f, x + s * 0.25f, y + s * 0.18f, t, c);
		roundRect(r, { x - s * 0.18f, y - s * 0.85f, s * 0.36f, s * 0.16f }, s * 0.05f, c);
		break;
	case IGear:
		for (int i = 0; i < 8; ++i)
		{
			float a = i * Pi / 4;
			line(r, x + std::cos(a) * s * 0.4f, y + std::sin(a) * s * 0.4f, x + std::cos(a) * s * 0.7f, y + std::sin(a) * s * 0.7f, t * 1.5f, c);
		}
		ring(r, x, y, s * 0.5f, t * 1.4f, c);
		break;
	case IExit:
		roundRectOutline(r, { x - s * 0.6f, y - s * 0.6f, s * 0.7f, s * 1.2f }, s * 0.08f, t, c);
		line(r, x - s * 0.1f, y, x + s * 0.7f, y, t, c);
		line(r, x + s * 0.7f, y, x + s * 0.4f, y - s * 0.3f, t, c);
		line(r, x + s * 0.7f, y, x + s * 0.4f, y + s * 0.3f, t, c);
		break;
	case IJoystick:
		ring(r, x, y + s * 0.2f, s * 0.6f, t, c);
		fillCircle(r, x, y - s * 0.25f, s * 0.3f, c);
		line(r, x, y - s * 0.1f, x, y + s * 0.2f, t, c);
		break;
	case IWeapon:
		roundRect(r, { x - s * 0.8f, y - s * 0.2f, s * 1.3f, s * 0.35f }, s * 0.08f, c);
		roundRect(r, { x - s * 0.5f, y + s * 0.05f, s * 0.3f, s * 0.55f }, s * 0.06f, c);
		break;
	case ICrown:
		tri(r, { x - s * 0.7f, y + s * 0.4f }, { x - s * 0.7f, y - s * 0.4f }, { x - s * 0.2f, y + s * 0.1f }, c);
		tri(r, { x - s * 0.35f, y + s * 0.4f }, { x, y - s * 0.55f }, { x + s * 0.35f, y + s * 0.4f }, c);
		tri(r, { x + s * 0.7f, y + s * 0.4f }, { x + s * 0.7f, y - s * 0.4f }, { x + s * 0.2f, y + s * 0.1f }, c);
		roundRect(r, { x - s * 0.7f, y + s * 0.2f, s * 1.4f, s * 0.3f }, s * 0.05f, c);
		break;
	case IDice:
		roundRectOutline(r, { x - s * 0.6f, y - s * 0.6f, s * 1.2f, s * 1.2f }, s * 0.2f, t, c);
		fillCircle(r, x - s * 0.25f, y - s * 0.25f, s * 0.12f, c);
		fillCircle(r, x, y, s * 0.12f, c);
		fillCircle(r, x + s * 0.25f, y + s * 0.25f, s * 0.12f, c);
		break;
	default:
		break;
	}
}

void actionButton(SDL_Renderer* r, float x, float y, float rad, SDL_Color base, int ic, bool down, float a)
{
	float k = down ? 1.0f : 0.8f;
	// outer neon glow
	ring(r, x, y, rad * 1.12f, rad * 0.12f, rgba(base.r, base.g, base.b, a * 0.25f * k));
	fillCircle(r, x, y, rad, rgba(base.r / 5, base.g / 5, base.b / 5, a * 0.75f));
	fillCircle(r, x, y, rad * 0.86f, rgba(base.r * 45 / 100, base.g * 45 / 100, base.b * 45 / 100, a * (down ? 0.95f : 0.7f)));
	fillCircle(r, x - rad * 0.1f, y - rad * 0.15f, rad * 0.62f, rgba(base.r * 6 / 10, base.g * 6 / 10, base.b * 6 / 10, a * 0.35f));
	ring(r, x, y, rad, std::max(3.0f, rad * 0.09f), rgba(std::min(255, base.r + 70), std::min(255, base.g + 70), std::min(255, base.b + 70), a));
	icon(r, ic, x, y, rad * 0.52f, rgba(std::min(255, base.r + 110), std::min(255, base.g + 110), std::min(255, base.b + 110), std::min(1.0f, a * 1.25f)));
}

} // namespace ui
} // namespace mobile
