#include "touch.hpp"
#include "ui.hpp"
#include "view.hpp"

#include "../gfx.hpp"
#include "../keys.hpp"
#include "../worm.hpp"
#include "../game.hpp"
#include "../settings.hpp"
#include "../controller/controller.hpp"

#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdio>

namespace mobile {

Input input;

namespace {
const float Pi = 3.14159265f;
}

void Input::init(TouchConfig const& c)
{
	cfg = c;
}

uint32_t Input::wormKey(int control) const
{
	if (!gfx.settings)
		return 0;
	WormSettings& ws = *gfx.settings->wormSettings[0];
	// Same lookup the game uses (Game::findControlForKey): extended controls
	// when extensions are on. Joystick-bound keys (>= 177) fall back to the
	// classic keyboard binding.
	uint32_t k = gfx.settings->extensions ? ws.controlsEx[control] : ws.controls[control];
	if (k == 0 || k >= 177)
		k = ws.controls[control];
	return k;
}

void Input::buzz()
{
	if (!cfg.vibrate)
		return;
	if (!hapticTried)
	{
		hapticTried = true;
		if (SDL_InitSubSystem(SDL_INIT_HAPTIC) == 0 && SDL_NumHaptics() > 0)
		{
			haptic = SDL_HapticOpen(0);
			if (haptic && SDL_HapticRumbleInit(haptic) != 0) { SDL_HapticClose(haptic); haptic = nullptr; }
		}
	}
	if (haptic)
		SDL_HapticRumblePlay(haptic, 0.35f, 12);
}

void Input::setMode(Mode m)
{
	if (m == mode_)
		return;
	mode_ = m;
	// Button sets differ per mode: drop held buttons (their fingers no longer
	// map to anything) but keep the stick.
	for (int i = 0; i < VCount; ++i)
		want[i] = false;
	pauseBtn.down = false; pauseBtn.finger = -1;
	seq.clear(); seqNow = 0;
	layout(w_, h_, game_);
}

void Input::layout(int winW, int winH, SDL_Rect const& gameRect)
{
	w_ = winW; h_ = winH; game_ = gameRect;
	if (w_ <= 0 || h_ <= 0)
		return;

	float u = std::min(h_ / 6.2f, w_ / 11.0f) * cfg.scale; // base size unit
	float s = h_ / 720.0f;
	bool L = cfg.leftHanded;
	auto X = [&](float fromLeft) { return L ? w_ - fromLeft : fromLeft; };

	stickR = u * 1.15f;
	stickX = X(stickR * 1.25f);
	stickY = h_ - stickR * 1.3f;

	buttonCount = 0;
	auto add = [&](float x, float y, float r, VIn in, int icon, int cr, int cg, int cb) {
		Button& b = buttons[buttonCount++];
		b.x = x; b.y = y; b.r = r; b.in = in; b.icon = icon;
		b.cr = (uint8_t)cr; b.cg = (uint8_t)cg; b.cb = (uint8_t)cb;
		b.down = false; b.finger = -1;
	};

	float m = 0.28f * u;
	float fr = (mode_ == Mode::Game) ? 0.85f * u : 0.75f * u;
	float fx = X(w_ - m - fr), fy = h_ - m - fr;
	if (mode_ == Mode::Game)
	{
		add(fx, fy, fr, VFire, ui::IRocket, 240, 70, 80);
		// fan of buttons around FIRE (angles measured towards the screen centre)
		float D = 2.0f * u;
		auto polar = [&](float deg, float rad, VIn in, int icon, int cr, int cg, int cb) {
			float a = deg * Pi / 180.0f;
			float dx = std::cos(a) * D, dy = -std::sin(a) * D;
			add(L ? fx - dx : fx + dx, fy + dy, rad, in, icon, cr, cg, cb);
		};
		if (cfg.digButton)
		{
			polar(188, 0.58f * u, VJump, ui::IJump, 60, 140, 250);
			polar(152, 0.5f * u, VDig, ui::IDig, 245, 185, 40);
			polar(118, 0.55f * u, VRope, ui::IRope, 175, 90, 240);
			polar(86, 0.5f * u, VSwapTap, ui::ISwap, 150, 160, 185);
		}
		else
		{
			polar(180, 0.58f * u, VJump, ui::IJump, 60, 140, 250);
			polar(125, 0.55f * u, VRope, ui::IRope, 175, 90, 240);
			polar(86, 0.5f * u, VSwapTap, ui::ISwap, 150, 160, 185);
		}
	}
	else
	{
		add(fx, fy, fr, VEnter, ui::IOk, 70, 200, 90);
		add(L ? fx + 1.75f * u : fx - 1.75f * u, h_ - m - 0.55f * u, 0.55f * u, VEscape, ui::IBack, 210, 80, 80);
	}

	// Pause: the HD HUD places it next to the minimap; default before first frame.
	pauseBtn.x = w_ - 330 * s; pauseBtn.y = 43 * s; pauseBtn.r = 29 * s;
	pauseBtn.in = VEscape;
	pauseBtn.icon = ui::IPause;

	char buf[512];
	int n = std::snprintf(buf, sizeof(buf), "liero-layout mode=%d win=%dx%d game=%d,%d,%d,%d stick=%d,%d,%d pause=%d,%d",
		(int)mode_, w_, h_, game_.x, game_.y, game_.w, game_.h, (int)stickX, (int)stickY, (int)stickR,
		(int)pauseBtn.x, (int)pauseBtn.y);
	for (int i = 0; i < buttonCount && n < (int)sizeof(buf) - 32; ++i)
		n += std::snprintf(buf + n, sizeof(buf) - n, " b%d=%d,%d", (int)buttons[i].in, (int)buttons[i].x, (int)buttons[i].y);
	SDL_Log("%s", buf);
}

void Input::hudBounds(float& left, float& right) const
{
	float a = stickX - stickR * 1.05f, b = stickX + stickR * 1.05f;
	float lo = 1e9f, hi = -1e9f;
	for (int i = 0; i < buttonCount; ++i)
	{
		lo = std::min(lo, buttons[i].x - buttons[i].r * 1.1f);
		hi = std::max(hi, buttons[i].x + buttons[i].r * 1.1f);
	}
	if (!cfg.leftHanded) { left = b; right = lo; }
	else { left = hi; right = a; }
	if (right - left < w_ * 0.3f) { left = w_ * 0.35f; right = w_ * 0.65f; }
}

void Input::applyKey(uint32_t dosKey, bool down, Controller* controller)
{
	if (dosKey == 0 || dosKey >= 177)
		return;
	if (sentDos[dosKey] == down)
		return;
	sentDos[dosKey] = down;
	if (down)
		pressedAt[dosKey] = frameNo;
	gfx.dosKeys[dosKey] = down;
	if (controller)
		controller->onKey(dosKey, down);
}

void Input::selectWeapon(int current, int target)
{
	if (target < 0 || target > 4 || target == current || mode_ != Mode::Game)
		return;
	int fwd = (target - current + 5) % 5, back = (current - target + 5) % 5;
	uint8_t dir = fwd <= back ? 2 : 4;
	int steps = std::min(fwd, back);
	seq.clear();
	seq.push_back(1);
	for (int i = 0; i < steps; ++i) { seq.push_back(1 | dir); seq.push_back(1); }
}

void Input::sync(Controller* controller)
{
	bool v[VCount];
	for (int i = 0; i < VCount; ++i)
		v[i] = want[i] || padBtn[i];
	bool dir[4];
	for (int i = 0; i < 4; ++i)
		dir[i] = stickDir[i] || padDir[i];
	v[VUp] |= dir[0]; v[VDown] |= dir[1]; v[VLeft] |= dir[2]; v[VRight] |= dir[3];

	bool desired[177] = {};
	auto set = [&](uint32_t k, bool on) { if (k && k < 177 && on) desired[k] = true; };

	if (mode_ == Mode::Game)
	{
		bool scripted = seqNow != 0;
		set(wormKey(WormSettings::Up), v[VUp]);
		set(wormKey(WormSettings::Down), v[VDown]);
		set(wormKey(WormSettings::Left), (scripted ? (seqNow & 4) != 0 : v[VLeft]) || v[VDig]);
		set(wormKey(WormSettings::Right), (scripted ? (seqNow & 2) != 0 : v[VRight]) || v[VDig]);
		set(wormKey(WormSettings::Fire), v[VFire]);
		set(wormKey(WormSettings::Jump), v[VJump] || v[VRope]);
		set(wormKey(WormSettings::Change), v[VChange] || v[VRope] || (seqNow & 1));
		set(DkEscape, v[VEscape]);
	}
	else
	{
		set(SDLToDOSKey(SDL_SCANCODE_UP), v[VUp]);
		set(SDLToDOSKey(SDL_SCANCODE_DOWN), v[VDown]);
		set(SDLToDOSKey(SDL_SCANCODE_LEFT), v[VLeft]);
		set(SDLToDOSKey(SDL_SCANCODE_RIGHT), v[VRight]);
		set(SDLToDOSKey(SDL_SCANCODE_RETURN), v[VEnter] || v[VJump] || v[VFire]);
		set(DkEscape, v[VEscape]);
	}

	for (uint32_t k = 1; k < 177; ++k)
	{
		// A quick tap (down+up within one frame) must still be seen by the
		// game for one frame, so delay such releases to the next frame.
		if (!desired[k] && sentDos[k] && pressedAt[k] == frameNo)
			continue;
		applyKey(k, desired[k], controller);
	}

	// Rising edge of the swap tap: next weapon.
	bool swapNow = v[VSwapTap];
	if (swapNow && !lastSwap && mode_ == Mode::Game && seq.empty() && seqNow == 0)
	{
		seq.push_back(1); seq.push_back(3); seq.push_back(1);
	}
	lastSwap = swapNow;
}

void Input::releaseAll(Controller* controller)
{
	for (int i = 0; i < VCount; ++i) { want[i] = false; padBtn[i] = false; }
	for (int i = 0; i < 4; ++i) { stickDir[i] = false; padDir[i] = false; }
	for (int i = 0; i < buttonCount; ++i) { buttons[i].down = false; buttons[i].finger = -1; }
	pauseBtn.down = false; pauseBtn.finger = -1;
	stickFinger = -1; knobDX = knobDY = 0;
	seq.clear(); seqNow = 0;
	++frameNo;
	for (uint32_t k = 1; k < 177; ++k)
		applyKey(k, false, controller);
}

void Input::update(Controller* controller)
{
	if (mode_ == Mode::Game)
	{
		if (!seq.empty()) { seqNow = seq.front(); seq.pop_front(); }
		else seqNow = 0;
	}
	else
	{
		seq.clear();
		seqNow = 0;
	}
	sync(controller);
	++frameNo; // the game frame that follows sees everything pressed so far
}

bool Input::handleEvent(SDL_Event const& ev, Controller* controller)
{
	switch (ev.type)
	{
	case SDL_FINGERDOWN:
	case SDL_FINGERMOTION:
	case SDL_FINGERUP:
	{
		if (usingPad)
		{
			usingPad = false; // touching the screen brings the controls back
			cfg.visible = true;
		}
		float x = ev.tfinger.x * w_, y = ev.tfinger.y * h_;
		SDL_FingerID id = ev.tfinger.fingerId;

		if (ev.type == SDL_FINGERDOWN)
		{
			if (mode_ == Mode::Game)
			{
				// Pause button (position comes from the HUD)
				SDL_FRect pr = view.pauseRect;
				if (pr.w > 0)
				{
					pauseBtn.x = pr.x + pr.w / 2; pauseBtn.y = pr.y + pr.h / 2; pauseBtn.r = pr.w / 2;
				}
				float dxp = x - pauseBtn.x, dyp = y - pauseBtn.y, rr = pauseBtn.r * 1.35f;
				if (dxp * dxp + dyp * dyp <= rr * rr)
				{
					pauseBtn.down = true; pauseBtn.finger = id;
					want[VEscape] = true;
					buzz();
					sync(controller);
					return true;
				}
				// Weapon bar slots
				if (view.active && view.game)
				{
					for (int i = 0; i < view.slotCount; ++i)
						if (ui::inside(view.slotRect[i], x, y))
						{
							Worm& me = *view.game->worms[0];
							selectWeapon(me.currentWeapon, i);
							buzz();
							return true;
						}
				}
			}
			// Buttons (generous hit radius), nearest wins.
			int best = -1; float bestD = 1e9f;
			for (int i = 0; i < buttonCount; ++i)
			{
				Button& b = buttons[i];
				float dx = x - b.x, dy = y - b.y;
				float d = std::sqrt(dx * dx + dy * dy);
				if (d <= b.r * 1.3f && d < bestD && b.finger == -1) { best = i; bestD = d; }
			}
			if (best >= 0)
			{
				buttons[best].down = true;
				buttons[best].finger = id;
				want[buttons[best].in] = true;
				buzz();
				sync(controller);
				return true;
			}
			// Stick: anywhere on the stick half of the screen (floating origin
			// if the touch starts away from the drawn stick).
			bool stickSide = cfg.leftHanded ? (x > w_ * 0.5f) : (x < w_ * 0.5f);
			if (stickFinger == -1 && stickSide)
				stickFinger = id;
			else
				return false;
		}

		if (id == pauseBtn.finger && ev.type == SDL_FINGERUP)
		{
			pauseBtn.down = false; pauseBtn.finger = -1;
			want[VEscape] = false;
			sync(controller);
			return true;
		}

		for (int i = 0; i < buttonCount; ++i)
		{
			Button& b = buttons[i];
			if (b.finger == id && ev.type == SDL_FINGERUP)
			{
				b.down = false; b.finger = -1;
				bool still = false;
				for (int j = 0; j < buttonCount; ++j)
					if (j != i && buttons[j].in == b.in && buttons[j].down) still = true;
				want[b.in] = still;
				sync(controller);
				return true;
			}
		}

		if (id == stickFinger)
		{
			if (ev.type == SDL_FINGERUP)
			{
				stickFinger = -1;
				knobDX = knobDY = 0;
				for (int i = 0; i < 4; ++i) stickDir[i] = false;
			}
			else
			{
				float dx = x - stickX, dy = y - stickY;
				float len = std::sqrt(dx * dx + dy * dy);
				float maxLen = stickR * 0.75f;
				if (len > maxLen) { knobDX = dx / len * maxLen; knobDY = dy / len * maxLen; }
				else { knobDX = dx; knobDY = dy; }
				float dead = stickR * (mode_ == Mode::Menu ? std::max(0.35f, cfg.deadZone) : cfg.deadZone);
				bool up = false, down = false, left = false, right = false;
				if (len > dead)
				{
					float deg = std::atan2(dy, dx) * 180.0f / Pi; // 0 = right, +y down
					if (mode_ == Mode::Menu)
					{
						if (deg > -45 && deg <= 45) right = true;
						else if (deg > 45 && deg <= 135) down = true;
						else if (deg > -135 && deg <= -45) up = true;
						else left = true;
					}
					else
					{
						right = deg > -67.5f && deg < 67.5f;
						left = deg > 112.5f || deg < -112.5f;
						down = deg > 22.5f && deg < 157.5f;
						up = deg < -22.5f && deg > -157.5f;
					}
				}
				stickDir[0] = up; stickDir[1] = down; stickDir[2] = left; stickDir[3] = right;
			}
			sync(controller);
			return true;
		}
		return false;
	}

	case SDL_CONTROLLERDEVICEADDED:
		SDL_GameControllerOpen(ev.cdevice.which);
		return false;

	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
	{
		bool down = ev.type == SDL_CONTROLLERBUTTONDOWN;
		if (down && !usingPad) { usingPad = true; cfg.visible = false; }
		switch (ev.cbutton.button)
		{
		case SDL_CONTROLLER_BUTTON_DPAD_UP: padDir[0] = down; break;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN: padDir[1] = down; break;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT: padDir[2] = down; break;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: padDir[3] = down; break;
		case SDL_CONTROLLER_BUTTON_A: padBtn[mode_ == Mode::Game ? VJump : VEnter] = down; if (!down) { padBtn[VJump] = padBtn[VEnter] = false; } break;
		case SDL_CONTROLLER_BUTTON_X: padBtn[VFire] = down; break;
		case SDL_CONTROLLER_BUTTON_Y: padBtn[VRope] = down; break;
		case SDL_CONTROLLER_BUTTON_B: padBtn[mode_ == Mode::Game ? VChange : VEscape] = down; if (!down) { padBtn[VChange] = padBtn[VEscape] = false; } break;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: padBtn[VSwapTap] = down; break;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: padBtn[VDig] = down; break;
		case SDL_CONTROLLER_BUTTON_START:
		case SDL_CONTROLLER_BUTTON_BACK: padBtn[VEscape] = down; break;
		default: break;
		}
		sync(controller);
		return true;
	}

	case SDL_CONTROLLERAXISMOTION:
	{
		const int th = 12000;
		int v = ev.caxis.value;
		switch (ev.caxis.axis)
		{
		case SDL_CONTROLLER_AXIS_LEFTX: padDir[2] = v < -th; padDir[3] = v > th; break;
		case SDL_CONTROLLER_AXIS_LEFTY: padDir[0] = v < -th; padDir[1] = v > th; break;
		case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: padBtn[VFire] = v > 8000; break;
		case SDL_CONTROLLER_AXIS_TRIGGERLEFT: padBtn[VRope] = v > 8000; break;
		default: return false;
		}
		if (std::abs(v) > th && !usingPad) { usingPad = true; cfg.visible = false; }
		sync(controller);
		return true;
	}
	default:
		return false;
	}
}

void Input::draw(SDL_Renderer* r)
{
	if (!cfg.visible || w_ <= 0)
		return;
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
	float a = cfg.opacity;

	// Stick (metal ring + knob, like the reference art)
	ui::fillCircle(r, stickX, stickY, stickR, ui::rgba(14, 16, 30, a * 0.6f));
	ui::ring(r, stickX, stickY, stickR, std::max(4.0f, stickR * 0.09f), ui::rgba(120, 125, 150, a * 0.9f));
	ui::ring(r, stickX, stickY, stickR * 0.93f, std::max(2.0f, stickR * 0.03f), ui::rgba(200, 205, 225, a * 0.5f));
	float ar = stickR * 0.13f, e = stickR * 0.78f;
	SDL_Color arrowC = ui::rgba(210, 212, 230, a * 0.85f);
	ui::tri(r, { stickX, stickY - e - ar * 0.4f }, { stickX - ar, stickY - e + ar * 0.8f }, { stickX + ar, stickY - e + ar * 0.8f }, arrowC);
	ui::tri(r, { stickX, stickY + e + ar * 0.4f }, { stickX + ar, stickY + e - ar * 0.8f }, { stickX - ar, stickY + e - ar * 0.8f }, arrowC);
	ui::tri(r, { stickX - e - ar * 0.4f, stickY }, { stickX - e + ar * 0.8f, stickY + ar }, { stickX - e + ar * 0.8f, stickY - ar }, arrowC);
	ui::tri(r, { stickX + e + ar * 0.4f, stickY }, { stickX + e - ar * 0.8f, stickY - ar }, { stickX + e - ar * 0.8f, stickY + ar }, arrowC);
	float kx = stickX + knobDX, ky = stickY + knobDY, kr = stickR * 0.4f;
	ui::fillCircle(r, kx, ky + kr * 0.08f, kr * 1.04f, ui::rgba(20, 20, 30, a * 0.7f));
	ui::fillCircle(r, kx, ky, kr, ui::rgba(150, 152, 165, a * (stickFinger != -1 ? 1.0f : 0.9f)));
	ui::fillCircle(r, kx - kr * 0.2f, ky - kr * 0.25f, kr * 0.62f, ui::rgba(200, 202, 215, a * 0.6f));
	ui::ring(r, kx, ky, kr, std::max(2.0f, kr * 0.07f), ui::rgba(235, 236, 245, a));

	for (int i = 0; i < buttonCount; ++i)
	{
		Button const& b = buttons[i];
		ui::actionButton(r, b.x, b.y, b.r, ui::rgba(b.cr, b.cg, b.cb), b.icon, b.down, a);
	}
	// Pause is drawn by the HUD in Game mode.
}

} // namespace mobile
