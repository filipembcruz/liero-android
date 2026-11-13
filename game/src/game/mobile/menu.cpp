#include "menu.hpp"
#include "ui.hpp"
#include "view.hpp"
#include "touch.hpp"
#include "config.hpp"
#include "platform.hpp"
#include "hd.hpp"

#include "../gfx.hpp"
#include "../game.hpp"
#include "../worm.hpp"
#include "../weapon.hpp"
#include "../common.hpp"
#include "../settings.hpp"
#include "../sfx.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include <cstdio>

namespace mobile {

bool weaponsPreselected = false;

namespace {

// ---------------------------------------------------------------------------
// Tiny immediate-mode framework: pointer clicks + gamepad/keyboard focus.

struct Frame
{
	SDL_Renderer* r = nullptr;
	int w = 0, h = 0;
	float s = 1;
	// pointer
	bool tapped = false; float tx = 0, ty = 0;  // released this frame (at tx,ty)
	bool held = false; float hx = 0, hy = 0, px0 = 0, py0 = 0; // currently down
	// navigation
	int navY = 0, navX = 0; bool activate = false, back = false;
	bool quit = false;
	int padPressed = -1; // controller button (or 100/101 trigger) pressed this frame
	bool trig[2] = {};
	// focus (gamepad)
	int focus = 0, focusCount = 0;
	bool usingPad = false;
};

Frame F;

void pump()
{
	F.tapped = false; F.navY = F.navX = 0; F.activate = F.back = false; F.padPressed = -1;
	SDL_Event ev;
	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
		case SDL_QUIT: F.quit = true; break;
		case SDL_APP_WILLENTERBACKGROUND: case SDL_APP_TERMINATING: saveNow(); saveConfig(); break;
		case SDL_FINGERDOWN:
			F.held = true; F.usingPad = false;
			F.hx = F.px0 = ev.tfinger.x * F.w; F.hy = F.py0 = ev.tfinger.y * F.h;
			break;
		case SDL_FINGERMOTION:
			F.hx = ev.tfinger.x * F.w; F.hy = ev.tfinger.y * F.h;
			break;
		case SDL_FINGERUP:
			F.held = false;
			F.tapped = true; F.tx = ev.tfinger.x * F.w; F.ty = ev.tfinger.y * F.h;
			break;
		case SDL_KEYDOWN:
			F.usingPad = true;
			switch (ev.key.keysym.scancode)
			{
			case SDL_SCANCODE_UP: F.navY = -1; break;
			case SDL_SCANCODE_DOWN: F.navY = 1; break;
			case SDL_SCANCODE_LEFT: F.navX = -1; break;
			case SDL_SCANCODE_RIGHT: F.navX = 1; break;
			case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: case SDL_SCANCODE_SPACE: F.activate = true; break;
			case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_AC_BACK: F.back = true; break;
			default: break;
			}
			break;
		case SDL_CONTROLLERDEVICEADDED:
			SDL_GameControllerOpen(ev.cdevice.which);
			break;
		case SDL_CONTROLLERAXISMOTION:
			if (ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
			{
				int t = ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? 0 : 1;
				bool d = ev.caxis.value > 8000;
				if (d && !F.trig[t]) F.padPressed = 100 + t;
				F.trig[t] = d;
			}
			else if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
			{
				static int last = 0;
				int v = ev.caxis.value > 16000 ? 1 : ev.caxis.value < -16000 ? -1 : 0;
				if (v && v != last) { F.navY = v; F.usingPad = true; }
				last = v;
			}
			break;
		case SDL_CONTROLLERBUTTONDOWN:
			F.usingPad = true;
			F.padPressed = ev.cbutton.button;
			switch (ev.cbutton.button)
			{
			case SDL_CONTROLLER_BUTTON_DPAD_UP: F.navY = -1; break;
			case SDL_CONTROLLER_BUTTON_DPAD_DOWN: F.navY = 1; break;
			case SDL_CONTROLLER_BUTTON_DPAD_LEFT: F.navX = -1; break;
			case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: F.navX = 1; break;
			case SDL_CONTROLLER_BUTTON_A: case SDL_CONTROLLER_BUTTON_START: F.activate = true; break;
			case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_BACK: F.back = true; break;
			default: break;
			}
			break;
		case SDL_WINDOWEVENT:
			if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				gfx.onWindowResize(ev.window.windowID);
			break;
		default: break;
		}
	}
	SDL_GetRendererOutputSize(F.r, &F.w, &F.h);
	F.s = F.h / 720.0f;
	if (F.navY && F.focusCount > 0)
		F.focus = (F.focus + F.navY + F.focusCount) % F.focusCount;
	F.focusCount = 0;
}

// Registers a focusable widget; returns true if it has gamepad focus.
bool focusable()
{
	int id = F.focusCount++;
	return F.usingPad && id == F.focus;
}

bool clicked(SDL_FRect rc)
{
	return F.tapped && ui::inside(rc, F.tx, F.ty) && ui::inside(rc, F.px0, F.py0);
}
bool pressedNow(SDL_FRect rc) { return F.held && ui::inside(rc, F.hx, F.hy) && ui::inside(rc, F.px0, F.py0); }

void click() { if (gfx.common) sfx.play(*gfx.common, 27); }
void tick() { if (gfx.common) sfx.play(*gfx.common, 26); }

bool button(SDL_FRect rc, std::string const& label, bool primary, int icon = ui::INone, bool enabled = true)
{
	bool foc = focusable();
	bool hit = enabled && (clicked(rc) || (foc && F.activate));
	ui::button(F.r, rc, F.s, label, primary, pressedNow(rc), icon, enabled);
	if (foc)
		ui::roundRectOutline(F.r, { rc.x - 5 * F.s, rc.y - 5 * F.s, rc.w + 10 * F.s, rc.h + 10 * F.s }, 20 * F.s, 3 * F.s, ui::rgba(255, 255, 255, 0.9f));
	if (hit) click();
	return hit;
}

void background(Game* game, float dim, bool hud = true)
{
	SDL_SetRenderDrawColor(F.r, 8, 6, 20, 255);
	SDL_RenderClear(F.r);
	if (game && view.game == game)
	{
		view.unfade(*game);
		view.suppressFade = true;
		view.present(F.r, *game, hud && game->cycles > 0);
		view.suppressFade = false;
	}
	SDL_SetRenderDrawBlendMode(F.r, SDL_BLENDMODE_BLEND);
	ui::roundRectV(F.r, { -10, -10, (float)F.w + 20, (float)F.h + 20 }, 1, ui::rgba(10, 6, 30, dim), ui::rgba(30, 10, 50, dim));
}

void present()
{
	if (gfx.mobileFrameHook)
		gfx.mobileFrameHook(F.r);
	SDL_RenderPresent(F.r);
	SDL_Delay(14);
}

// Setting row: label, value, and < > steppers. Returns -1/0/+1.
int stepper(SDL_FRect row, std::string const& label, std::string const& value, std::string const& hint = "")
{
	bool foc = focusable();
	float s = F.s;
	ui::roundRect(F.r, row, 12 * s, ui::rgba(40, 50, 90, foc ? 0.95f : 0.7f));
	if (foc) ui::roundRectOutline(F.r, row, 12 * s, 3 * s, ui::rgba(255, 220, 90));
	ui::text(F.r, label, row.x + 20 * s, row.y + row.h * (hint.empty() ? 0.5f : 0.38f), 24 * s, ui::rgba(255, 255, 255));
	if (!hint.empty())
		ui::text(F.r, hint, row.x + 20 * s, row.y + row.h * 0.74f, 16 * s, ui::rgba(170, 180, 230), ui::Left, false);
	float bw = row.h * 0.8f;
	SDL_FRect minus = { row.x + row.w - bw * 2 - 230 * s, row.y + row.h * 0.1f, bw, row.h * 0.8f };
	SDL_FRect plus = { row.x + row.w - bw - 12 * s, row.y + row.h * 0.1f, bw, row.h * 0.8f };
	int d = 0;
	auto arrow = [&](SDL_FRect b, int dir) {
		bool p = pressedNow(b);
		ui::roundRectV(F.r, b, 10 * s, p ? ui::rgba(255, 220, 90) : ui::rgba(96, 72, 190), p ? ui::rgba(240, 160, 20) : ui::rgba(64, 44, 140));
		ui::roundRectOutline(F.r, b, 10 * s, 2 * s, ui::rgba(170, 150, 255));
		float cx = b.x + b.w / 2, cy = b.y + b.h / 2, a = b.h * 0.22f;
		if (dir < 0) ui::tri(F.r, { cx - a, cy }, { cx + a * 0.7f, cy - a }, { cx + a * 0.7f, cy + a }, ui::rgba(255, 255, 255));
		else ui::tri(F.r, { cx + a, cy }, { cx - a * 0.7f, cy - a }, { cx - a * 0.7f, cy + a }, ui::rgba(255, 255, 255));
		if (clicked(b)) d = dir;
	};
	arrow(minus, -1);
	arrow(plus, 1);
	ui::text(F.r, value, (minus.x + minus.w + plus.x) / 2, row.y + row.h / 2, 24 * s, ui::rgba(255, 215, 90), ui::Center);
	if (foc && F.navX) d = F.navX;
	if (d) tick();
	return d;
}

std::string pct(int v) { return std::to_string(v) + "%"; }

// Weapon icon fitted into a box: HD sprite if the HD visual is on, else the original pixels.
void drawWeaponIcon(Common& common, int w, SDL_FRect box)
{
	int iw = 0, ih = 0;
	SDL_Texture* t = config.smooth == 2 ? hd::weaponIcon(common, w, &iw, &ih) : nullptr;
	if (t)
	{
		float k = std::min(box.w / iw, box.h / ih);
		SDL_FRect d = { box.x + (box.w - iw * k) / 2, box.y + (box.h - ih * k) / 2, iw * k, ih * k };
		SDL_RenderCopyF(F.r, t, NULL, &d);
	}
	else if (SDL_Texture* p = view.weaponIcon(F.r, common, w))
	{
		float sz = std::min(box.w, box.h);
		SDL_FRect d = { box.x + (box.w - sz) / 2, box.y + (box.h - sz) / 2, sz, sz };
		SDL_RenderCopyF(F.r, p, NULL, &d);
	}
}

void applyLive()
{
	TouchConfig tc = input.cfg;
	tc.scale = config.touchScale / 100.0f;
	tc.opacity = config.touchOpacity / 100.0f;
	tc.leftHanded = config.lefty != 0;
	tc.deadZone = config.deadZone / 100.0f;
	tc.vibrate = config.vibrate != 0;
	tc.digButton = config.digButton != 0;
	input.init(tc);
	input.relayout();
	if (gfx.settings)
	{
		gfx.settings->loadingTime = config.loadingTime;
		gfx.settings->blood = config.blood;
		gfx.settings->regenerateLevel = config.newMap != 0;
	}
}

template<typename T> T cycle(T v, int d, std::vector<T> const& opts)
{
	auto it = std::find(opts.begin(), opts.end(), v);
	int i = it == opts.end() ? 0 : int(it - opts.begin());
	i = std::max(0, std::min((int)opts.size() - 1, i + d));
	return opts[i];
}

// ---------------------------------------------------------------------------

// Modal: wait for a controller button/trigger. Returns code, or -1 if cancelled.
int captureButton(Game* game, char const* action)
{
	SDL_FlushEvents(SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERBUTTONUP);
	Uint32 t0 = SDL_GetTicks();
	while (true)
	{
		pump();
		if (F.quit) return -1;
		background(game, 0.85f);
		float s = F.s;
		SDL_FRect p = { F.w / 2.0f - 380 * s, F.h / 2.0f - 130 * s, 760 * s, 260 * s };
		ui::panel(F.r, p, s, 0.96f);
		ui::icon(F.r, ui::IJoystick, p.x + p.w / 2, p.y + 58 * s, 26 * s, ui::rgba(255, 215, 90));
		ui::text(F.r, std::string("Aperte o botão para: ") + action, p.x + p.w / 2, p.y + 120 * s, 30 * s, ui::rgba(255, 255, 255), ui::Center);
		ui::text(F.r, "Toque na tela para cancelar", p.x + p.w / 2, p.y + 170 * s, 20 * s, ui::rgba(170, 185, 240), ui::Center, false);
		int secs = 10 - (int)((SDL_GetTicks() - t0) / 1000);
		ui::text(F.r, std::to_string(std::max(0, secs)), p.x + p.w / 2, p.y + 215 * s, 24 * s, ui::rgba(255, 215, 90), ui::Center);
		present();
		if (F.padPressed >= 0 && F.padPressed < SDL_CONTROLLER_BUTTON_DPAD_UP) return F.padPressed;
		if (F.padPressed >= 100) return F.padPressed;
		if (F.padPressed > SDL_CONTROLLER_BUTTON_DPAD_RIGHT) return F.padPressed;
		if (F.tapped || secs < 0 || F.back) return -1;
	}
}

void settingsScreen(Game* game)
{
	int tab = 0;
	int prevFov = config.fov;
	F.focus = 0;
	while (true)
	{
		pump();
		if (F.quit) break;
		background(game, 0.78f);
		float s = F.s;
		SDL_FRect pnl = { F.w * 0.5f - 560 * s, 30 * s, 1120 * s, F.h - 60 * s };
		ui::panel(F.r, pnl, s, 0.92f);
		ui::icon(F.r, ui::IGear, pnl.x + 44 * s, pnl.y + 44 * s, 20 * s, ui::rgba(255, 215, 90));
		ui::text(F.r, "CONFIGURAÇÕES", pnl.x + 76 * s, pnl.y + 44 * s, 34 * s, ui::rgba(255, 255, 255));

		static const char* tabs[4] = { "Toque", "Controle", "Jogo", "Visual" };
		for (int i = 0; i < 4; ++i)
		{
			SDL_FRect t = { pnl.x + 400 * s + i * 176 * s, pnl.y + 18 * s, 166 * s, 54 * s };
			bool sel = tab == i;
			ui::roundRectV(F.r, t, 14 * s, sel ? ui::rgba(255, 214, 70) : ui::rgba(60, 50, 120), sel ? ui::rgba(240, 160, 20) : ui::rgba(40, 32, 90));
			ui::text(F.r, tabs[i], t.x + t.w / 2, t.y + t.h / 2, 24 * s, sel ? ui::rgba(60, 30, 10) : ui::rgba(255, 255, 255), ui::Center, !sel);
			if (clicked(t)) { tab = i; F.focus = 0; tick(); }
		}
		if (F.usingPad && F.focus == 0 && F.navX && false) {}

		float y = pnl.y + 90 * s, rh = 62 * s, gap = 6 * s;
		auto row = [&]() { SDL_FRect r = { pnl.x + 28 * s, y, pnl.w - 56 * s, rh }; y += rh + gap; return r; };
		int d;
		if (tab == 0)
		{
			if ((d = stepper(row(), "Tamanho dos botões", pct(config.touchScale)))) { config.touchScale = std::max(50, std::min(180, config.touchScale + d * 10)); applyLive(); }
			if ((d = stepper(row(), "Opacidade", pct(config.touchOpacity)))) { config.touchOpacity = std::max(10, std::min(100, config.touchOpacity + d * 10)); applyLive(); }
			if ((d = stepper(row(), "Zona morta do direcional", pct(config.deadZone), "Menor = responde com toque mais curto"))) { config.deadZone = std::max(5, std::min(60, config.deadZone + d * 5)); applyLive(); }
			if ((d = stepper(row(), "Lado do direcional", config.lefty ? "Direita" : "Esquerda"))) { config.lefty = d > 0 ? 1 : 0; applyLive(); }
			if ((d = stepper(row(), "Botão Cavar", config.digButton ? "Ligado" : "Desligado", "Cava túneis (= esquerda + direita juntos)"))) { config.digButton = d > 0 ? 1 : 0; applyLive(); }
			static const char* ts[3] = { "Automático", "Sempre", "Ocultos" };
			if ((d = stepper(row(), "Controles na tela", ts[config.touchShow], "Automático: somem quando você usa um controle externo"))) { config.touchShow = std::max(0, std::min(2, config.touchShow + d)); input.cfg.visible = config.touchShow != 2; applyLive(); }
		}
		else if (tab == 1)
		{
			// Gamepad mapping: two columns of 4 actions
			ui::text(F.r, "Toque numa função e aperte o botão do controle. O direcional (D-pad/analógico) é fixo.",
				pnl.x + 30 * s, y + 4 * s, 18 * s, ui::rgba(170, 185, 240), ui::Left, false);
			y += 30 * s;
			float colW = (pnl.w - 70 * s) / 2;
			for (int a = 0; a < Config::PadCount; ++a)
			{
				SDL_FRect rr = { pnl.x + 28 * s + (a / 4) * (colW + 14 * s), y + (a % 4) * (rh + gap), colW, rh };
				bool foc = focusable();
				bool p = pressedNow(rr);
				ui::roundRect(F.r, rr, 12 * s, ui::rgba(40, 50, 90, foc || p ? 0.95f : 0.7f));
				if (foc) ui::roundRectOutline(F.r, rr, 12 * s, 3 * s, ui::rgba(255, 220, 90));
				ui::text(F.r, padActionName(a), rr.x + 20 * s, rr.y + rr.h / 2, 23 * s, ui::rgba(255, 255, 255));
				SDL_FRect chip = { rr.x + rr.w - 210 * s, rr.y + rr.h * 0.18f, 190 * s, rr.h * 0.64f };
				ui::roundRectV(F.r, chip, 10 * s, ui::rgba(96, 72, 190), ui::rgba(64, 44, 140));
				ui::roundRectOutline(F.r, chip, 10 * s, 2 * s, ui::rgba(170, 150, 255));
				ui::text(F.r, padButtonName(config.pad[a]), chip.x + chip.w / 2, chip.y + chip.h / 2, 21 * s, ui::rgba(255, 215, 90), ui::Center);
				if (clicked(rr) || (foc && F.activate))
				{
					click();
					int code = captureButton(game, padActionName(a));
					if (code >= 0)
					{
						for (int b = 0; b < Config::PadCount; ++b)
							if (b != a && config.pad[b] == code) config.pad[b] = config.pad[a]; // swap
						config.pad[a] = code;
					}
				}
			}
			y += 4 * (rh + gap);
			if ((d = stepper({ pnl.x + 28 * s, y, pnl.w - 56 * s, rh }, "Vibração ao tocar", config.vibrate ? "Ligada" : "Desligada"))) { config.vibrate = d > 0 ? 1 : 0; applyLive(); }
			y += rh + gap;
			SDL_FRect rs = { pnl.x + 28 * s, y + 4 * s, 330 * s, 56 * s };
			if (button(rs, "PADRÃO", false, ui::IRestart))
			{
				Config d;
				for (int a = 0; a < Config::PadCount; ++a) config.pad[a] = d.pad[a];
			}
		}
		else if (tab == 2)
		{
			std::vector<int> lt = { 0, 25, 50, 75, 100, 150, 200, 300, 500 };
			if ((d = stepper(row(), "Tempo de carregamento", pct(config.loadingTime), "Recarga das armas (original: LOADING TIMES)"))) { config.loadingTime = cycle(config.loadingTime, d, lt); applyLive(); }
			std::vector<int> lv = { 1, 3, 5, 10, 15, 20, 30, 50, 99 };
			if ((d = stepper(row(), "Vidas", std::to_string(config.lives), "Vale a partir da próxima partida"))) { config.lives = cycle(config.lives, d, lv); }
			static const char* dn[3] = { "Fácil", "Médio", "Difícil" };
			if ((d = stepper(row(), "Dificuldade da CPU", dn[config.difficulty], "Fácil = CPU original · próxima partida"))) { config.difficulty = std::max(0, std::min(2, config.difficulty + d)); }
			std::vector<int> bl = { 0, 50, 100, 200, 300, 500 };
			if ((d = stepper(row(), "Quantidade de sangue", pct(config.blood)))) { config.blood = cycle(config.blood, d, bl); applyLive(); }
			if ((d = stepper(row(), "Mapa novo a cada partida", config.newMap ? "Sim" : "Não"))) { config.newMap = d > 0 ? 1 : 0; applyLive(); }
		}
		else
		{
			static const char* fv[3] = { "Perto", "Amplo", "Máximo" };
			if ((d = stepper(row(), "Campo de visão", fv[config.fov], "Amplo/Máximo mostram mais do mapa"))) { config.fov = std::max(0, std::min(2, config.fov + d)); }
			static const char* vn[3] = { "Clássico", "Suave", "HD" };
			if ((d = stepper(row(), "Visual", vn[std::max(0, std::min(2, config.smooth))], "HD: vermes, armas, explosões, terreno e fundo novos"))) { config.smooth = std::max(0, std::min(2, config.smooth + d)); if (game) view.newGame(*game); }
			static const char* tn[3] = { "Caverna", "Vulcão", "Gelo" };
			if ((d = stepper(row(), "Tema (modo HD)", tn[config.theme], "Terreno e fundo do mapa"))) { config.theme = std::max(0, std::min(2, config.theme + d)); if (game) view.newGame(*game); }
			if ((d = stepper(row(), "Minimapa", config.minimap ? "Ligado" : "Desligado"))) { config.minimap = d > 0 ? 1 : 0; }
			if ((d = stepper(row(), "Opacidade do HUD", pct(config.hudOpacity), "Placar, minimapa e barra de armas"))) { config.hudOpacity = std::max(20, std::min(100, config.hudOpacity + d * 10)); }
			if ((d = stepper(row(), "HUD transparente sobre você", config.hudAutoFade ? "Ligado" : "Desligado", "O painel some quando o seu verme passa por baixo"))) { config.hudAutoFade = d > 0 ? 1 : 0; }
			if ((d = stepper(row(), "Seta sobre o seu verme", config.marker ? "Ligada" : "Desligada"))) { config.marker = d > 0 ? 1 : 0; }
		}
		if (config.fov != prevFov && game)
		{
			prevFov = config.fov;
			view.newGame(*game);
			if (game->viewports.size()) { /* camera recentres on next frame */ }
		}

		SDL_FRect ok = { pnl.x + pnl.w - 280 * s, pnl.y + pnl.h - 72 * s, 250 * s, 58 * s };
		if (button(ok, "VOLTAR", true, ui::IBack) || F.back)
			break;
		present();
	}
	applyConfigToSettings(*gfx.settings);
	saveConfig();
	saveNow();
}

} // anon

MenuResult pauseMenu(Game* game, bool gameRunning, char lastResult)
{
	F.r = gfx.sdlRenderer;
	F.focus = 0;
	F.held = false;
	// swallow the key/finger that opened the menu
	SDL_FlushEvents(SDL_FINGERDOWN, SDL_FINGERMOTION);
	while (true)
	{
		pump();
		if (F.quit) return MenuQuit;
		background(game, gameRunning ? 0.62f : 0.72f);
		float s = F.s;
		float pw = 560 * s, ph = (gameRunning ? 520 : 470) * s;
		SDL_FRect pnl = { (F.w - pw) / 2, (F.h - ph) / 2, pw, ph };
		ui::panel(F.r, pnl, s, 0.92f);

		std::string title = gameRunning ? "PAUSA" : (lastResult == 'W' ? "VITÓRIA!" : lastResult == 'L' ? "DERROTA" : lastResult == 'D' ? "EMPATE" : "LIERO");
		SDL_Color tc = lastResult == 'W' && !gameRunning ? ui::rgba(255, 215, 80) : lastResult == 'L' && !gameRunning ? ui::rgba(255, 110, 110) : ui::rgba(255, 255, 255);
		ui::text(F.r, title, pnl.x + pw / 2, pnl.y + 52 * s, 46 * s, tc, ui::Center);
		if (game && game->worms.size() >= 2)
		{
			char buf[96];
			std::snprintf(buf, sizeof(buf), "%s  %d  ×  %d  CPU", game->worms[0]->settings->name.c_str(), game->worms[0]->kills, game->worms[1]->kills);
			ui::text(F.r, buf, pnl.x + pw / 2, pnl.y + 100 * s, 22 * s, ui::rgba(180, 200, 255), ui::Center);
		}

		float bx = pnl.x + 50 * s, bw = pw - 100 * s, bh = 66 * s, y = pnl.y + 136 * s, gap = 14 * s;
		auto next = [&]() { SDL_FRect r = { bx, y, bw, bh }; y += bh + gap; return r; };
		if (gameRunning)
		{
			if (button(next(), "CONTINUAR", true, ui::IPlay) || F.back) return MenuResume;
			if (button(next(), "NOVA PARTIDA", false, ui::IRestart)) return MenuNewGame;
		}
		else
		{
			if (button(next(), "JOGAR DE NOVO", true, ui::IPlay)) return MenuNewGame;
		}
		if (button(next(), "CONFIGURAÇÕES", false, ui::IGear)) { settingsScreen(game); F.focus = 0; }
		SDL_FRect half = next();
		SDL_FRect a = { half.x, half.y, half.w / 2 - 7 * s, half.h }, b = { half.x + half.w / 2 + 7 * s, half.y, half.w / 2 - 7 * s, half.h };
		if (button(a, "AVANÇADO", false, ui::IWeapon)) return MenuOriginal;
		if (button(b, "SAIR", false, ui::IExit)) return MenuQuit;
		if (!gameRunning && F.back) return MenuQuit;
		ui::text(F.r, "Avançado = menu original do Liero (inglês)", pnl.x + pw / 2, pnl.y + ph - 24 * s, 16 * s, ui::rgba(150, 160, 210), ui::Center, false);
		present();
	}
}

bool weaponPicker(Game& game)
{
	F.r = gfx.sdlRenderer;
	F.focus = 0;
	F.held = false;
	Common& common = *game.common;
	Settings& st = *game.settings;
	WormSettings& ws = *st.wormSettings[0];
	view.newGame(game);
	int nW = (int)common.weapons.size();
	// current picks (as weapon indices)
	int pick[5];
	auto enabled = [&](int w) { return st.weapTable[w] <= 0; };
	for (int j = 0; j < 5; ++j)
	{
		int v = (int)ws.weapons[j];
		pick[j] = (v >= 1 && v <= nW) ? common.weapOrder[v - 1] : common.weapOrder[0];
	}
	int slot = 0;
	auto randomize = [&]() {
		std::vector<int> pool;
		for (int k = 0; k < nW; ++k) if (enabled(common.weapOrder[k])) pool.push_back(common.weapOrder[k]);
		for (int j = 0; j < 5 && !pool.empty(); ++j)
		{
			int i = gfx.rand(0, (int)pool.size());
			pick[j] = pool[i];
			if (pool.size() > 5) pool.erase(pool.begin() + i);
		}
	};
	SDL_FlushEvents(SDL_FINGERDOWN, SDL_FINGERMOTION);
	float scroll = 0;
	while (true)
	{
		pump();
		if (F.quit) return false;
		background(&game, 0.8f, false);
		float s = F.s;
		ui::icon(F.r, ui::IWeapon, 46 * s, 44 * s, 22 * s, ui::rgba(255, 215, 90));
		ui::text(F.r, "ESCOLHA SUAS 5 ARMAS", 84 * s, 44 * s, 34 * s, ui::rgba(255, 255, 255));
		ui::text(F.r, "Toque num espaço embaixo e depois numa arma", 86 * s, 82 * s, 19 * s, ui::rgba(170, 185, 240), ui::Left, false);

		// Grid of weapons (in Liero's menu order)
		int cols = 8;
		float gx = 30 * s, gy = 108 * s, gw = F.w - 60 * s;
		float cw = gw / cols, ch = 84 * s;
		float gridBottom = F.h - 200 * s;
		int rows = (nW + cols - 1) / cols;
		float maxScroll = std::max(0.0f, rows * ch - (gridBottom - gy));
		if (F.held && std::abs(F.hy - F.py0) > 12 * s && ui::inside({ gx, gy, gw, gridBottom - gy }, F.px0, F.py0))
		{
			scroll = std::max(0.0f, std::min(maxScroll, scroll - (F.hy - F.py0)));
			F.py0 = F.hy;
		}
		SDL_Rect clip = { (int)gx, (int)gy, (int)gw, (int)(gridBottom - gy) };
		SDL_RenderSetClipRect(F.r, &clip);
		for (int k = 0; k < nW; ++k)
		{
			int w = common.weapOrder[k];
			SDL_FRect c = { gx + (k % cols) * cw + 4 * s, gy + (k / cols) * ch + 4 * s - scroll, cw - 8 * s, ch - 8 * s };
			bool chosen = std::find(pick, pick + 5, w) != pick + 5;
			bool en = enabled(w);
			bool foc = focusable();
			ui::roundRectV(F.r, c, 10 * s, chosen ? ui::rgba(90, 70, 30, 0.95f) : ui::rgba(40, 52, 90, en ? 0.92f : 0.4f),
				chosen ? ui::rgba(70, 50, 20, 0.95f) : ui::rgba(26, 34, 64, en ? 0.92f : 0.4f));
			ui::roundRectOutline(F.r, c, 10 * s, (foc ? 3.5f : 2) * s, foc ? ui::rgba(255, 255, 255) : chosen ? ui::rgba(255, 200, 60) : ui::rgba(100, 130, 200, 0.7f));
			drawWeaponIcon(common, w, { c.x + 8 * s, c.y + c.h * 0.2f, c.h * 0.6f, c.h * 0.6f });
			std::string nm = common.weapons[w].name;
			float px = std::min(19 * s, (c.w - c.h * 0.5f - 26 * s) / std::max(1.0f, ui::textWidth(nm, 1.0f)));
			ui::text(F.r, nm, c.x + c.h * 0.5f + 18 * s, c.y + c.h / 2, px, en ? ui::rgba(255, 255, 255) : ui::rgba(150, 150, 170), ui::Left);
			if (en && c.y + c.h > gy && c.y < gridBottom && (clicked(c) || (foc && F.activate)) && std::abs(F.ty - F.py0) < 30 * s + 1e6f)
			{
				// if already in another slot, swap
				for (int j = 0; j < 5; ++j) if (pick[j] == w && j != slot) pick[j] = pick[slot];
				pick[slot] = w;
				slot = (slot + 1) % 5;
				tick();
			}
		}
		SDL_RenderSetClipRect(F.r, NULL);

		// Selected slots bar
		float sw = std::min(200 * s, (F.w - 620 * s) / 5);
		SDL_FRect bar = { 30 * s, F.h - 176 * s, sw * 5 + 16 * s, 150 * s };
		ui::panel(F.r, bar, s);
		for (int j = 0; j < 5; ++j)
		{
			SDL_FRect c = { bar.x + 8 * s + j * sw + 4 * s, bar.y + 10 * s, sw - 8 * s, bar.h - 20 * s };
			bool sel = j == slot;
			ui::roundRectV(F.r, c, 12 * s, sel ? ui::rgba(255, 214, 70) : ui::rgba(50, 64, 100), sel ? ui::rgba(240, 160, 20) : ui::rgba(30, 40, 70));
			ui::roundRectOutline(F.r, c, 12 * s, 3 * s, sel ? ui::rgba(255, 240, 170) : ui::rgba(110, 140, 200));
			ui::text(F.r, std::to_string(j + 1), c.x + 16 * s, c.y + 20 * s, 20 * s, sel ? ui::rgba(60, 30, 10) : ui::rgba(170, 190, 240), ui::Left, false);
			drawWeaponIcon(common, pick[j], { c.x + (c.w - c.h * 0.5f) / 2, c.y + 12 * s, c.h * 0.5f, c.h * 0.45f });
			std::string nm = common.weapons[pick[j]].name;
			float px = std::min(19 * s, (c.w - 12 * s) / std::max(1.0f, ui::textWidth(nm, 1.0f)));
			ui::text(F.r, nm, c.x + c.w / 2, c.y + c.h * 0.8f, px, sel ? ui::rgba(60, 30, 10) : ui::rgba(255, 255, 255), ui::Center, !sel);
			if (clicked(c)) { slot = j; tick(); }
		}

		float bx = bar.x + bar.w + 24 * s, bw = F.w - bx - 30 * s;
		SDL_FRect rnd = { bx, bar.y + 4 * s, bw, 64 * s };
		SDL_FRect go = { bx, bar.y + 80 * s, bw, 70 * s };
		if (button(rnd, "ALEATÓRIO", false, ui::IDice)) randomize();
		if (button(go, "JOGAR", true, ui::IPlay))
		{
			for (int j = 0; j < 5; ++j)
			{
				int ord = 0;
				for (int k = 0; k < nW; ++k) if (common.weapOrder[k] == pick[j]) ord = k;
				ws.weapons[j] = ord + 1;
			}
			weaponsPreselected = true;
			saveNow();
			return true;
		}
		if (F.back) return false;
		present();
	}
}

} // namespace mobile
