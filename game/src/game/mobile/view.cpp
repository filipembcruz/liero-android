#include "view.hpp"
#include "ui.hpp"
#include "touch.hpp"
#include "config.hpp"

#include "../game.hpp"
#include "../worm.hpp"
#include "../weapon.hpp"
#include "../common.hpp"
#include "../gfx.hpp"
#include "../settings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace mobile {

View view;

namespace {

SDL_Color wormColor(Worm const& w)
{
	return ui::rgba(w.settings->rgb[0] * 4 + 3, w.settings->rgb[1] * 4 + 3, w.settings->rgb[2] * 4 + 3);
}

std::string modeName(int mode)
{
	switch (mode)
	{
	case Settings::GMGameOfTag: return "Pega-pega";
	case Settings::GMHoldazone: return "Domine a zona";
	case Settings::GMScalesOfJustice: return "Balança da justiça";
	default: return "Mata-mata";
	}
}

// Scale2x / EPX (Andrea Mazzoleni): smooths diagonal pixel edges without blur.
void scale2x(uint32_t const* src, int w, int h, uint32_t* dst)
{
	for (int y = 0; y < h; ++y)
	{
		uint32_t const* row = src + y * w;
		uint32_t const* up = src + (y > 0 ? y - 1 : y) * w;
		uint32_t const* dn = src + (y < h - 1 ? y + 1 : y) * w;
		uint32_t* d0 = dst + (2 * y) * (2 * w);
		uint32_t* d1 = d0 + 2 * w;
		for (int x = 0; x < w; ++x)
		{
			uint32_t P = row[x];
			uint32_t A = up[x], D = dn[x];
			uint32_t C = row[x > 0 ? x - 1 : x], B = row[x < w - 1 ? x + 1 : x];
			uint32_t e0 = P, e1 = P, e2 = P, e3 = P;
			if (C == A && C != D && A != B) e0 = A;
			if (A == B && A != C && B != D) e1 = B;
			if (D == C && D != B && C != A) e2 = C;
			if (B == D && B != A && D != C) e3 = D;
			d0[2 * x] = e0; d0[2 * x + 1] = e1;
			d1[2 * x] = e2; d1[2 * x + 1] = e3;
		}
	}
}

} // anon

void View::setup(SDL_Renderer* r, int winW, int winH)
{
	winW_ = winW; winH_ = winH;
	s_ = winH / 720.0f;
	(void)r;
	if (game)
		newGame(*game);
}

void View::newGame(Game& g)
{
	game = &g;
	levelW_ = g.level.width; levelH_ = g.level.height;
	if (winH_ <= 0 || levelW_ <= 0)
		return;
	// The level is only 504x350, so on wide phones the camera can never be
	// wider than 504 px. Pick the scale so the image always fills the screen:
	//  0 "Perto":  integer scale for ~160 px of height (bigger worms)
	//  1 "Amplo":  smallest integer scale that still fills the width
	//  2 "Máximo": fractional scale, the whole level width (bilinear)
	int kmin = std::max(1, (winW_ + levelW_ - 1) / levelW_);
	kmin = std::max(kmin, (winH_ + levelH_ - 1) / levelH_);
	fractional_ = false;
	if (config.fov >= 2)
	{
		float k = std::max(winW_ / (float)levelW_, winH_ / (float)levelH_);
		viewW_ = std::min(levelW_, (int)std::ceil(winW_ / k));
		viewH_ = std::min(levelH_, (int)std::ceil(winH_ / k));
		scale_ = 0;
		fractional_ = true;
		dst_.w = winW_; dst_.h = winH_; dst_.x = 0; dst_.y = 0;
		fscale_ = k;
	}
	else
	{
		int want = std::max(1, (int)std::floor(winH_ / 160.0f + 0.5f));
		scale_ = config.fov == 0 ? std::max(kmin, want) : kmin;
		viewH_ = std::min(levelH_, (winH_ + scale_ - 1) / scale_);
		viewW_ = std::min(levelW_, (winW_ + scale_ - 1) / scale_);
		dst_.w = viewW_ * scale_; dst_.h = viewH_ * scale_;
		dst_.x = (winW_ - dst_.w) / 2; dst_.y = (winH_ - dst_.h) / 2;
		fscale_ = (float)scale_;
	}

	renderer_.init(viewW_, viewH_);
	renderer_.loadPalette(*g.common);
	g.focus(renderer_);
	vp_.reset(new Viewport(gvl::rect(0, 0, viewW_, viewH_), 0, levelW_, levelH_));
	vp_->hud = false;
	minimapAge_ = 1000;
	SDL_Log("liero-view win=%dx%d view=%dx%d scale=%d fov=%d", winW_, winH_, viewW_, viewH_, scale_, config.fov);
}

void View::update(Game& g)
{
	if (game != &g || !vp_ || g.level.width != levelW_ || g.level.height != levelH_)
		newGame(g);
	if (!vp_)
		return;
	if (!g.viewports.empty())
	{
		vp_->shake = g.viewports[0]->shake;
		vp_->bannerY = g.viewports[0]->bannerY;
	}
	vp_->process(g);
	renderer_.clear();
	vp_->draw(g, renderer_, StateGame, false);

	// Palette effects, exactly as Game::draw does for the PC renderer.
	Common& common = *g.common;
	renderer_.pal = renderer_.origpal;
	for (int w = 0; w < 4; ++w)
		renderer_.pal.rotateFrom(renderer_.origpal, common.colorAnim[w].from, common.colorAnim[w].to, g.cycles >> 3);
	renderer_.fadeValue = gfx.playRenderer.fadeValue;
	renderer_.pal.fade(renderer_.fadeValue);
	if (g.screenFlash > 0)
		renderer_.pal.lightUp(g.screenFlash);
}

void View::unfade(Game& g)
{
	if (!vp_)
		return;
	Common& common = *g.common;
	renderer_.pal = renderer_.origpal;
	for (int w = 0; w < 4; ++w)
		renderer_.pal.rotateFrom(renderer_.origpal, common.colorAnim[w].from, common.colorAnim[w].to, g.cycles >> 3);
}

void View::ensureIcons(SDL_Renderer* r, Common& common)
{
	if (iconCommon_ == &common && !weaponIcons_.empty())
		return;
	for (auto* t : weaponIcons_) if (t) SDL_DestroyTexture(t);
	weaponIcons_.assign(common.weapons.size(), nullptr);
	iconCommon_ = &common;
	Color pal[256];
	Palette p = renderer_.origpal;
	p.activate(pal);
	for (std::size_t i = 0; i < common.weapons.size(); ++i)
	{
		Weapon const& w = common.weapons[i];
		uint32_t px[7 * 7];
		bool any = false;
		if (w.startFrame >= 0 && w.startFrame < common.smallSprites.count)
		{
			int f = w.startFrame + (w.shotType == 2 ? 4 : 0);
			if (f >= common.smallSprites.count) f = w.startFrame;
			PalIdx* sp = common.smallSprites.spritePtr(f);
			for (int k = 0; k < 49; ++k)
			{
				PalIdx c = sp[k];
				px[k] = c ? (0xFF000000u | (pal[c].r << 16) | (pal[c].g << 8) | pal[c].b) : 0;
				any |= c != 0;
			}
		}
		if (!any)
		{
			// bullet-type weapons: a small burst of their bullet colour
			int c = w.colorBullets > 0 && w.colorBullets < 256 ? w.colorBullets : 50;
			uint32_t col = 0xFF000000u | (pal[c].r << 16) | (pal[c].g << 8) | pal[c].b;
			for (int k = 0; k < 49; ++k) px[k] = 0;
			int pts[][2] = { {1,3},{3,3},{5,3},{2,1},{4,5},{2,5},{4,1},{3,2},{3,4} };
			for (auto& q : pts) px[q[1] * 7 + q[0]] = col;
		}
		SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 7, 7);
		if (t)
		{
			SDL_UpdateTexture(t, NULL, px, 7 * 4);
			SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
			SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest);
		}
		weaponIcons_[i] = t;
	}
}

void View::present(SDL_Renderer* r, Game& g, bool hud)
{
	if (!vp_)
		return;
	int smooth = (config.smooth || fractional_) ? 1 : 0;
	int tw = smooth ? viewW_ * 2 : viewW_, th = smooth ? viewH_ * 2 : viewH_;
	if (!tex_ || texW_ != tw || texH_ != th || smoothTex_ != smooth)
	{
		if (tex_) SDL_DestroyTexture(tex_);
		tex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, tw, th);
		SDL_SetTextureScaleMode(tex_, smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
		texW_ = tw; texH_ = th; smoothTex_ = smooth;
	}
	Color pal[256];
	renderer_.pal.activate(pal);
	uint32_t pal32[256];
	for (int i = 0; i < 256; ++i)
		pal32[i] = 0xFF000000u | (pal[i].r << 16) | (pal[i].g << 8) | pal[i].b;
	buf_.resize(std::size_t(viewW_) * viewH_);
	for (int y = 0; y < viewH_; ++y)
	{
		PalIdx const* src = renderer_.bmp.pixels + y * renderer_.bmp.pitch;
		uint32_t* d = &buf_[std::size_t(y) * viewW_];
		for (int x = 0; x < viewW_; ++x)
			d[x] = pal32[src[x]];
	}
	if (smooth)
	{
		buf2_.resize(std::size_t(tw) * th);
		scale2x(buf_.data(), viewW_, viewH_, buf2_.data());
		SDL_UpdateTexture(tex_, NULL, buf2_.data(), tw * 4);
	}
	else
		SDL_UpdateTexture(tex_, NULL, buf_.data(), tw * 4);

	SDL_RenderCopy(r, tex_, NULL, &dst_);
	if (hud)
		drawHud(r, g);
}

void View::drawMinimap(SDL_Renderer* r, Game& g, SDL_FRect area)
{
	int mw = levelW_ / 2, mh = levelH_ / 2;
	if (!minimap_)
	{
		minimap_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, mw, mh);
		SDL_SetTextureScaleMode(minimap_, SDL_ScaleModeLinear);
		minimapAge_ = 1000;
	}
	if (++minimapAge_ > 12)
	{
		minimapAge_ = 0;
		Color pal[256];
		renderer_.pal.activate(pal);
		std::vector<uint32_t> px(std::size_t(mw) * mh);
		for (int y = 0; y < mh; ++y)
			for (int x = 0; x < mw; ++x)
			{
				int c = g.level.data[(y * 2) * levelW_ + x * 2];
				Color cc = pal[c];
				// darken background (air) so tunnels read clearly
				bool bg = g.level.materials[(y * 2) * levelW_ + x * 2].background();
				int k = bg ? 45 : 100;
				px[std::size_t(y) * mw + x] = 0xFF000000u | ((cc.r * k / 100) << 16) | ((cc.g * k / 100) << 8) | (cc.b * k / 100);
			}
		SDL_UpdateTexture(minimap_, NULL, px.data(), mw * 4);
	}
	SDL_RenderCopyF(r, minimap_, NULL, &area);
	float kx = area.w / levelW_, ky = area.h / levelH_;
	// camera rectangle
	SDL_FRect cam = { area.x + vp_->x * kx, area.y + vp_->y * ky, viewW_ * kx, viewH_ * ky };
	ui::roundRectOutline(r, cam, 3 * s_, 2 * s_, ui::rgba(255, 220, 90, 0.95f));
	for (auto* wp : g.worms)
	{
		Worm const& w = *wp;
		if (!w.visible) continue;
		float x = area.x + ftoi(w.pos.x) * kx, y = area.y + ftoi(w.pos.y) * ky;
		ui::fillCircle(r, x, y, 5.5f * s_, ui::rgba(0, 0, 0, 0.7f));
		ui::fillCircle(r, x, y, 4.2f * s_, wormColor(w));
	}
}

void View::drawOffscreen(SDL_Renderer* r, Game& g)
{
	for (auto* wp : g.worms)
	{
		Worm const& w = *wp;
		if (w.index == 0 || !w.visible) continue;
		int wx = ftoi(w.pos.x), wy = ftoi(w.pos.y);
		int sx = dst_.x + int((wx - vp_->x) * fscale_), sy = dst_.y + int((wy - vp_->y) * fscale_);
		if (sx >= 0 && sx < winW_ && sy >= 0 && sy < winH_)
			continue;
		Worm const& me = *g.worms[0];
		float dx = float(wx - ftoi(me.pos.x)), dy = float(wy - ftoi(me.pos.y));
		float len = std::sqrt(dx * dx + dy * dy);
		if (len < 1) continue;
		float cx = winW_ / 2.0f, cy = winH_ / 2.0f;
		float ux = dx / len, uy = dy / len;
		float mx = winW_ / 2.0f - 70 * s_, my = winH_ / 2.0f - 70 * s_;
		float t = std::min(std::fabs(ux) > 1e-3f ? mx / std::fabs(ux) : 1e9f, std::fabs(uy) > 1e-3f ? my / std::fabs(uy) : 1e9f);
		float ax = cx + ux * t, ay = cy + uy * t;
		SDL_Color col = wormColor(w);
		float a = 22 * s_;
		SDL_FPoint tip = { ax + ux * a, ay + uy * a };
		SDL_FPoint l = { ax - uy * a * 0.7f, ay + ux * a * 0.7f };
		SDL_FPoint rr = { ax + uy * a * 0.7f, ay - ux * a * 0.7f };
		ui::tri(r, tip, l, rr, ui::rgba(col.r, col.g, col.b, 0.95f));
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%d m", (int)(len / 4));
		ui::wormHead(r, ax - ux * 28 * s_, ay - uy * 28 * s_ - 4 * s_, 13 * s_, col);
		ui::text(r, buf, ax - ux * 28 * s_, ay - uy * 28 * s_ + 20 * s_, 20 * s_, ui::rgba(255, 255, 255), ui::Center);
	}
}

void View::drawHud(SDL_Renderer* r, Game& g)
{
	float s = s_;
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
	if (!ui::ready() || g.worms.size() < 2)
		return;
	Common& common = *g.common;
	ensureIcons(r, common);

	// --- Scoreboard (top-left)
	std::vector<Worm*> order(g.worms.begin(), g.worms.end());
	bool lives = g.settings->gameMode == Settings::GMKillEmAll || g.settings->gameMode == Settings::GMScalesOfJustice;
	std::stable_sort(order.begin(), order.end(), [&](Worm* a, Worm* b) {
		return lives ? (a->lives * 1000 + a->kills) > (b->lives * 1000 + b->kills) : a->kills > b->kills;
	});
	SDL_FRect sb = { 12 * s, 10 * s, 330 * s, 22 * s + order.size() * 44 * s };
	ui::panel(r, sb, s);
	for (std::size_t i = 0; i < order.size(); ++i)
	{
		Worm& w = *order[i];
		float y = sb.y + 33 * s + i * 44 * s;
		if (i == 0 && (w.kills > 0 || lives))
			ui::icon(r, ui::ICrown, sb.x + 20 * s, y - 12 * s, 11 * s, ui::rgba(255, 205, 60));
		ui::wormHead(r, sb.x + 46 * s, y, 16 * s, wormColor(w));
		std::string nm = w.settings->name + (w.index == 0 ? "" : "");
		ui::text(r, nm, sb.x + 70 * s, y - 7 * s, 21 * s, ui::rgba(255, 255, 255));
		float hp = w.visible ? std::max(0, w.health) / float(std::max(1, w.settings->health)) : 0.0f;
		ui::bar(r, { sb.x + 70 * s, y + 9 * s, 150 * s, 8 * s }, hp, ui::rgba(130, 240, 90), ui::rgba(40, 170, 40));
		ui::icon(r, ui::ISkull, sb.x + 240 * s, y - 2 * s, 12 * s, ui::rgba(235, 235, 245));
		ui::text(r, std::to_string(w.kills), sb.x + 256 * s, y, 22 * s, ui::rgba(255, 255, 255));
		if (lives)
		{
			ui::icon(r, ui::IHeart, sb.x + 290 * s, y - 2 * s, 11 * s, ui::rgba(255, 90, 110));
			ui::text(r, std::to_string(w.lives), sb.x + 305 * s, y, 22 * s, ui::rgba(255, 255, 255));
		}
	}

	// --- Timer + mode banner (top centre)
	int secs = g.cycles / 70;
	char tbuf[16];
	std::snprintf(tbuf, sizeof(tbuf), "%d:%02d", secs / 60, secs % 60);
	SDL_FRect tp = { sb.x + sb.w + 14 * s, 10 * s, 128 * s, 54 * s };
	ui::panel(r, tp, s);
	ui::icon(r, ui::IClock, tp.x + 28 * s, tp.y + tp.h / 2, 15 * s, ui::rgba(255, 255, 255));
	ui::text(r, tbuf, tp.x + 52 * s, tp.y + tp.h / 2, 28 * s, ui::rgba(255, 255, 255));

	std::string sub = lives ? (std::to_string(g.settings->lives) + " vidas cada") : "";
	SDL_FRect bp = { tp.x + tp.w + 14 * s, 10 * s, 250 * s, 64 * s };
	ui::panel(r, bp, s);
	ui::text(r, modeName(g.settings->gameMode), bp.x + bp.w / 2, bp.y + 22 * s, 27 * s, ui::rgba(255, 255, 255), ui::Center);
	if (!sub.empty())
	{
		ui::icon(r, ui::ISkull, bp.x + bp.w / 2 - ui::textWidth(sub, 19 * s) / 2 - 14 * s, bp.y + 47 * s, 9 * s, ui::rgba(255, 255, 255));
		ui::text(r, sub, bp.x + bp.w / 2 + 6 * s, bp.y + 47 * s, 19 * s, ui::rgba(220, 225, 255), ui::Center);
	}

	// --- Minimap (top-right) + pause button
	float mmw = 270 * s, mmh = mmw * levelH_ / std::max(1, levelW_);
	SDL_FRect mm = { winW_ - mmw - 16 * s, 12 * s, mmw, mmh };
	if (config.minimap)
	{
		SDL_FRect mp = { mm.x - 6 * s, mm.y - 6 * s, mm.w + 12 * s, mm.h + 12 * s };
		ui::panel(r, mp, s);
		drawMinimap(r, g, mm);
		pauseRect = { mp.x - 70 * s, 14 * s, 58 * s, 58 * s };
	}
	else
		pauseRect = { winW_ - 80 * s, 14 * s, 58 * s, 58 * s };
	{
		float cx = pauseRect.x + pauseRect.w / 2, cy = pauseRect.y + pauseRect.h / 2;
		ui::actionButton(r, cx, cy, pauseRect.w / 2, ui::rgba(150, 160, 190), ui::IPause, false, 0.9f);
	}

	drawOffscreen(r, g);

	// --- Bottom: player strip + weapon bar, between stick and action buttons
	Worm& me = *g.worms[0];
	float left = 0, right = winW_;
	input.hudBounds(left, right);
	float avail = right - left;
	float slotW = std::min(118 * s, (avail - 20 * s) / 5);
	float barW = slotW * 5 + 16 * s;
	float barH = std::min(100 * s, slotW * 0.95f);
	SDL_FRect wb = { left + (avail - barW) / 2, winH_ - barH - 14 * s, barW, barH };
	ui::panel(r, wb, s, 0.78f);
	slotCount = 5;
	for (int i = 0; i < 5; ++i)
	{
		SDL_FRect sr = { wb.x + 8 * s + i * slotW + 3 * s, wb.y + 8 * s, slotW - 6 * s, barH - 16 * s };
		slotRect[i] = sr;
		bool cur = i == me.currentWeapon;
		WormWeapon const& ww = me.weapons[i];
		if (cur)
		{
			ui::roundRectV(r, sr, 10 * s, ui::rgba(255, 210, 70, 0.95f), ui::rgba(230, 150, 20, 0.95f));
			ui::roundRectOutline(r, sr, 10 * s, 3 * s, ui::rgba(255, 240, 170));
		}
		else
		{
			ui::roundRectV(r, sr, 10 * s, ui::rgba(50, 64, 100, 0.9f), ui::rgba(30, 40, 70, 0.9f));
			ui::roundRectOutline(r, sr, 10 * s, 2 * s, ui::rgba(110, 140, 200, 0.8f));
		}
		if (!ww.type) continue;
		int widx = int(ww.type - &common.weapons[0]);
		if (widx >= 0 && widx < (int)weaponIcons_.size() && weaponIcons_[widx])
		{
			float isz = std::min(sr.w * 0.6f, sr.h * 0.5f);
			SDL_FRect ir = { sr.x + (sr.w - isz) / 2, sr.y + sr.h * 0.1f, isz, isz };
			SDL_RenderCopyF(r, weaponIcons_[widx], NULL, &ir);
		}
		std::string name = ww.type->name;
		float px = std::min(17 * s, (sr.w - 10 * s) / std::max(1.0f, ui::textWidth(name, 1.0f)));
		ui::text(r, name, sr.x + sr.w / 2, sr.y + sr.h * 0.8f, px, cur ? ui::rgba(60, 30, 10) : ui::rgba(255, 255, 255), ui::Center, !cur);
		// ammo / reload indicator
		float frac;
		if (ww.available())
			frac = ww.type->ammo > 0 ? ww.ammo / float(ww.type->ammo) : 1.0f;
		else
		{
			int lt = ww.type->computedLoadingTime(*g.settings);
			frac = lt > 0 ? 1.0f - ww.loadingLeft / float(lt) : 0.0f;
		}
		SDL_FRect ab = { sr.x + 6 * s, sr.y + sr.h - 7 * s, sr.w - 12 * s, 4 * s };
		ui::bar(r, ab, frac, ww.available() ? ui::rgba(120, 210, 255) : ui::rgba(255, 120, 80), ww.available() ? ui::rgba(60, 140, 230) : ui::rgba(220, 60, 40));
	}

	// Player strip above the bar: head, name, health, current weapon state
	SDL_FRect ps = { wb.x, wb.y - 54 * s, std::min(barW, 420 * s), 46 * s };
	ui::panel(r, ps, s, 0.78f);
	ui::wormHead(r, ps.x + 28 * s, ps.y + ps.h / 2, 16 * s, wormColor(me));
	ui::text(r, me.settings->name, ps.x + 52 * s, ps.y + 15 * s, 18 * s, ui::rgba(255, 255, 255));
	ui::text(r, "(Você)", ps.x + 58 * s + ui::textWidth(me.settings->name, 18 * s), ps.y + 15 * s, 18 * s, ui::rgba(120, 190, 255));
	float hp = me.visible ? std::max(0, me.health) / float(std::max(1, me.settings->health)) : 0.0f;
	SDL_FRect hb = { ps.x + 52 * s, ps.y + 28 * s, ps.w - 150 * s, 10 * s };
	ui::bar(r, hb, hp, ui::rgba(130, 240, 90), ui::rgba(40, 170, 40));
	char hbuf[24];
	std::snprintf(hbuf, sizeof(hbuf), "%d/%d", std::max(0, me.visible ? me.health : 0), me.settings->health);
	ui::text(r, hbuf, ps.x + ps.w - 12 * s, ps.y + 31 * s, 19 * s, ui::rgba(255, 255, 255), ui::Right);
	WormWeapon const& cw = me.weapons[me.currentWeapon];
	if (cw.type && !cw.available() && (g.cycles % 30) < 20)
		ui::text(r, "Recarregando…", ps.x + ps.w - 12 * s, ps.y + 13 * s, 16 * s, ui::rgba(255, 170, 90), ui::Right);
}

} // namespace mobile
