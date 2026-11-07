#include "touch.hpp"

#include "../gfx.hpp"
#include "../keys.hpp"
#include "../worm.hpp"
#include "../settings.hpp"
#include "../controller/controller.hpp"

#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdio>

namespace mobile {

Input input;

namespace {

enum Icon { IconNone, IconFire, IconJump, IconRope, IconSwap, IconOk, IconBack, IconPause };

const float Pi = 3.14159265f;

void fillCircle(SDL_Renderer* r, float cx, float cy, float rad, SDL_Color c)
{
	const int seg = 40;
	SDL_Vertex v[seg + 2];
	v[0].position = { cx, cy };
	v[0].color = c;
	for (int i = 0; i <= seg; ++i)
	{
		float a = (float)i / seg * 2 * Pi;
		v[i + 1].position = { cx + std::cos(a) * rad, cy + std::sin(a) * rad };
		v[i + 1].color = c;
	}
	int idx[seg * 3];
	for (int i = 0; i < seg; ++i)
	{
		idx[i * 3] = 0;
		idx[i * 3 + 1] = i + 1;
		idx[i * 3 + 2] = i + 2;
	}
	SDL_RenderGeometry(r, NULL, v, seg + 2, idx, seg * 3);
}

void ring(SDL_Renderer* r, float cx, float cy, float rad, float thick, SDL_Color c)
{
	const int seg = 48;
	std::vector<SDL_Vertex> v;
	std::vector<int> idx;
	for (int i = 0; i <= seg; ++i)
	{
		float a = (float)i / seg * 2 * Pi;
		float ca = std::cos(a), sa = std::sin(a);
		SDL_Vertex o, in;
		o.position = { cx + ca * rad, cy + sa * rad };
		in.position = { cx + ca * (rad - thick), cy + sa * (rad - thick) };
		o.color = in.color = c;
		o.tex_coord = in.tex_coord = { 0, 0 };
		v.push_back(o);
		v.push_back(in);
		if (i < seg)
		{
			int b = i * 2;
			idx.insert(idx.end(), { b, b + 1, b + 2, b + 1, b + 3, b + 2 });
		}
	}
	SDL_RenderGeometry(r, NULL, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

void tri(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_Color col)
{
	SDL_Vertex v[3];
	v[0].position = a; v[1].position = b; v[2].position = c;
	for (auto& x : v) { x.color = col; x.tex_coord = { 0, 0 }; }
	SDL_RenderGeometry(r, NULL, v, 3, NULL, 0);
}

void thickLine(SDL_Renderer* r, float x0, float y0, float x1, float y1, float t, SDL_Color c)
{
	float dx = x1 - x0, dy = y1 - y0;
	float len = std::sqrt(dx * dx + dy * dy);
	if (len < 0.001f) return;
	float nx = -dy / len * t * 0.5f, ny = dx / len * t * 0.5f;
	SDL_FPoint p0 = { x0 + nx, y0 + ny }, p1 = { x0 - nx, y0 - ny };
	SDL_FPoint p2 = { x1 - nx, y1 - ny }, p3 = { x1 + nx, y1 + ny };
	tri(r, p0, p1, p2, c);
	tri(r, p0, p2, p3, c);
}

void drawIcon(SDL_Renderer* r, int icon, float x, float y, float s, SDL_Color c)
{
	float t = std::max(2.0f, s * 0.16f);
	switch (icon)
	{
	case IconFire: // starburst
		for (int i = 0; i < 8; ++i)
		{
			float a = i * Pi / 4;
			float ri = (i % 2) ? s * 0.25f : s * 0.15f;
			float ro = (i % 2) ? s * 0.55f : s * 0.75f;
			thickLine(r, x + std::cos(a) * ri, y + std::sin(a) * ri, x + std::cos(a) * ro, y + std::sin(a) * ro, t, c);
		}
		fillCircle(r, x, y, s * 0.22f, c);
		break;
	case IconJump: // double chevron up
		for (int k = 0; k < 2; ++k)
		{
			float yy = y + s * 0.25f - k * s * 0.42f;
			thickLine(r, x - s * 0.5f, yy + s * 0.25f, x, yy - s * 0.2f, t * 1.2f, c);
			thickLine(r, x, yy - s * 0.2f, x + s * 0.5f, yy + s * 0.25f, t * 1.2f, c);
		}
		break;
	case IconRope: // hook + line
		thickLine(r, x - s * 0.55f, y + s * 0.55f, x + s * 0.2f, y - s * 0.2f, t, c);
		ring(r, x + s * 0.25f, y - s * 0.25f, s * 0.35f, t, c);
		break;
	case IconSwap: // two arrows in a circle
		ring(r, x, y, s * 0.55f, t, c);
		tri(r, { x + s * 0.55f - s * 0.25f, y - s * 0.05f }, { x + s * 0.55f + s * 0.25f, y - s * 0.05f }, { x + s * 0.55f, y + s * 0.3f }, c);
		tri(r, { x - s * 0.55f - s * 0.25f, y + s * 0.05f }, { x - s * 0.55f + s * 0.25f, y + s * 0.05f }, { x - s * 0.55f, y - s * 0.3f }, c);
		break;
	case IconOk: // check mark
		thickLine(r, x - s * 0.5f, y, x - s * 0.12f, y + s * 0.4f, t * 1.3f, c);
		thickLine(r, x - s * 0.12f, y + s * 0.4f, x + s * 0.55f, y - s * 0.4f, t * 1.3f, c);
		break;
	case IconBack: // left arrow
		thickLine(r, x - s * 0.5f, y, x + s * 0.5f, y, t * 1.3f, c);
		thickLine(r, x - s * 0.5f, y, x - s * 0.1f, y - s * 0.4f, t * 1.3f, c);
		thickLine(r, x - s * 0.5f, y, x - s * 0.1f, y + s * 0.4f, t * 1.3f, c);
		break;
	case IconPause:
		thickLine(r, x - s * 0.25f, y - s * 0.45f, x - s * 0.25f, y + s * 0.45f, t * 1.5f, c);
		thickLine(r, x + s * 0.25f, y - s * 0.45f, x + s * 0.25f, y + s * 0.45f, t * 1.5f, c);
		break;
	}
}

SDL_Color rgba(int r, int g, int b, float a)
{
	SDL_Color c;
	c.r = (Uint8)r; c.g = (Uint8)g; c.b = (Uint8)b;
	c.a = (Uint8)std::max(0.0f, std::min(255.0f, a * 255.0f));
	return c;
}

} // anon

void Input::init(TouchConfig const& c)
{
	cfg = c;
}

uint32_t Input::wormKey(int control) const
{
	// Player 1's configured keys (the same ones the PC version uses).
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
	swapPhase = 0; swapQueued = false;
	layout(w_, h_, game_);
}

void Input::layout(int winW, int winH, SDL_Rect const& gameRect)
{
	w_ = winW; h_ = winH; game_ = gameRect;
	if (w_ <= 0 || h_ <= 0)
		return;

	float u = std::min(h_ / 6.2f, w_ / 11.0f) * cfg.scale; // base button diameter
	bool L = cfg.leftHanded;
	auto X = [&](float fromLeft) { return L ? w_ - fromLeft : fromLeft; };
	auto XR = [&](float fromRight) { return L ? fromRight : w_ - fromRight; };

	stickR = u * 1.15f;
	stickX = X(std::max(stickR * 1.25f, game_.x * 0.5f));
	stickY = h_ - stickR * 1.35f;

	buttonCount = 0;
	auto add = [&](float x, float y, float r, VIn in, int icon, int cr, int cg, int cb) {
		Button& b = buttons[buttonCount++];
		b.x = x; b.y = y; b.r = r; b.in = in; b.icon = icon;
		b.cr = (uint8_t)cr; b.cg = (uint8_t)cg; b.cb = (uint8_t)cb;
		b.down = false; b.finger = -1;
	};

	float side = std::max(u * 1.5f, (float)game_.x * 0.5f);
	if (mode_ == Mode::Game)
	{
		add(XR(side), h_ - u * 1.45f, u * 0.78f, VFire, IconFire, 235, 70, 80);
		add(XR(side + u * 1.65f), h_ - u * 0.85f, u * 0.56f, VJump, IconJump, 60, 140, 245);
		add(XR(side - u * 0.15f), h_ - u * 3.05f, u * 0.56f, VRope, IconRope, 170, 90, 230);
		add(XR(side + u * 1.55f), h_ - u * 2.5f, u * 0.5f, VSwapTap, IconSwap, 245, 185, 40);
	}
	else
	{
		add(XR(side), h_ - u * 1.45f, u * 0.72f, VEnter, IconOk, 70, 200, 90);
		add(XR(side + u * 1.6f), h_ - u * 0.9f, u * 0.56f, VEscape, IconBack, 200, 80, 80);
	}

	pauseBtn.x = XR(std::max(u * 0.6f, (float)game_.x * 0.5f));
	pauseBtn.y = u * 0.65f;
	pauseBtn.r = u * 0.42f;
	pauseBtn.in = VEscape;
	pauseBtn.icon = IconPause;
	pauseBtn.cr = 230; pauseBtn.cg = 230; pauseBtn.cb = 240;

	// Machine-readable layout for the emulator test script.
	{
		char buf[512];
		int n = std::snprintf(buf, sizeof(buf), "liero-layout mode=%d win=%dx%d game=%d,%d,%d,%d stick=%d,%d,%d pause=%d,%d",
			(int)mode_, w_, h_, game_.x, game_.y, game_.w, game_.h, (int)stickX, (int)stickY, (int)stickR,
			(int)pauseBtn.x, (int)pauseBtn.y);
		for (int i = 0; i < buttonCount && n < (int)sizeof(buf) - 32; ++i)
			n += std::snprintf(buf + n, sizeof(buf) - n, " b%d=%d,%d", (int)buttons[i].in, (int)buttons[i].x, (int)buttons[i].y);
		SDL_Log("%s", buf);
	}
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
		bool swapping = swapPhase > 0;
		set(wormKey(WormSettings::Up), v[VUp]);
		set(wormKey(WormSettings::Down), v[VDown]);
		set(wormKey(WormSettings::Left), v[VLeft] && !swapping);
		set(wormKey(WormSettings::Right), (v[VRight] && !swapping) || swapPhase == 2);
		set(wormKey(WormSettings::Fire), v[VFire]);
		set(wormKey(WormSettings::Jump), v[VJump] || v[VRope]);
		set(wormKey(WormSettings::Change), v[VChange] || v[VRope] || swapping);
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

	// Rising edge of the swap tap queues a weapon-change pulse.
	bool swapNow = v[VSwapTap];
	if (swapNow && !lastSwap)
		swapQueued = true;
	lastSwap = swapNow;
}

void Input::releaseAll(Controller* controller)
{
	for (int i = 0; i < VCount; ++i) { want[i] = false; padBtn[i] = false; }
	for (int i = 0; i < 4; ++i) { stickDir[i] = false; padDir[i] = false; }
	for (int i = 0; i < buttonCount; ++i) { buttons[i].down = false; buttons[i].finger = -1; }
	pauseBtn.down = false; pauseBtn.finger = -1;
	stickFinger = -1; knobDX = knobDY = 0;
	swapPhase = 0; swapQueued = false;
	++frameNo;
	for (uint32_t k = 1; k < 177; ++k)
		applyKey(k, false, controller);
}

void Input::update(Controller* controller)
{
	if (mode_ == Mode::Game)
	{
		if (swapPhase > 0)
		{
			++swapPhase;
			if (swapPhase > 4)
				swapPhase = 0;
		}
		else if (swapQueued)
		{
			swapQueued = false;
			swapPhase = 1;
		}
	}
	else
	{
		swapPhase = 0;
		swapQueued = false;
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
			float dxp = x - pauseBtn.x, dyp = y - pauseBtn.y;
			float pr = pauseBtn.r * 1.3f;
			if (dxp * dxp + dyp * dyp <= pr * pr)
			{
				pauseBtn.down = true; pauseBtn.finger = id;
				want[VEscape] = true;
				sync(controller);
				return true;
			}
			// Buttons first (generous hit radius), nearest wins.
			int best = -1; float bestD = 1e9f;
			for (int i = 0; i < buttonCount; ++i)
			{
				Button& b = buttons[i];
				float dx = x - b.x, dy = y - b.y;
				float d = std::sqrt(dx * dx + dy * dy);
				if (d <= b.r * 1.35f && d < bestD && b.finger == -1) { best = i; bestD = d; }
			}
			if (best >= 0)
			{
				buttons[best].down = true;
				buttons[best].finger = id;
				want[buttons[best].in] = true;
				sync(controller);
				return true;
			}
			// Stick: anywhere on the stick half of the screen.
			bool stickSide = cfg.leftHanded ? (x > w_ * 0.5f) : (x < w_ * 0.5f);
			if (stickFinger == -1 && stickSide)
			{
				stickFinger = id;
			}
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
				float dead = stickR * (mode_ == Mode::Menu ? 0.35f : 0.22f);
				// 8-way with ~45 degree sectors (diagonals allowed, like holding two keys)
				bool up = false, down = false, left = false, right = false;
				if (len > dead)
				{
					float a = std::atan2(dy, dx); // -pi..pi, 0 = right, +y down
					float deg = a * 180.0f / Pi;
					if (mode_ == Mode::Menu)
					{
						// 4-way in menus to avoid accidental double moves
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
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: padBtn[VChange] = down; break;
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

	// Stick / d-pad
	fillCircle(r, stickX, stickY, stickR, rgba(20, 16, 40, a * 0.55f));
	ring(r, stickX, stickY, stickR, std::max(3.0f, stickR * 0.06f), rgba(200, 200, 230, a * 0.8f));
	float ar = stickR * 0.16f;
	SDL_Color arrowC = rgba(220, 220, 240, a * 0.85f);
	tri(r, { stickX, stickY - stickR * 0.85f }, { stickX - ar, stickY - stickR * 0.85f + ar }, { stickX + ar, stickY - stickR * 0.85f + ar }, arrowC);
	tri(r, { stickX, stickY + stickR * 0.85f }, { stickX + ar, stickY + stickR * 0.85f - ar }, { stickX - ar, stickY + stickR * 0.85f - ar }, arrowC);
	tri(r, { stickX - stickR * 0.85f, stickY }, { stickX - stickR * 0.85f + ar, stickY + ar }, { stickX - stickR * 0.85f + ar, stickY - ar }, arrowC);
	tri(r, { stickX + stickR * 0.85f, stickY }, { stickX + stickR * 0.85f - ar, stickY - ar }, { stickX + stickR * 0.85f - ar, stickY + ar }, arrowC);
	float kx = stickX + knobDX, ky = stickY + knobDY;
	fillCircle(r, kx, ky, stickR * 0.42f, rgba(170, 170, 190, a * (stickFinger != -1 ? 1.0f : 0.8f)));
	ring(r, kx, ky, stickR * 0.42f, std::max(2.0f, stickR * 0.04f), rgba(255, 255, 255, a));

	auto drawButton = [&](Button const& b) {
		float alpha = a * (b.down ? 1.0f : 0.75f);
		fillCircle(r, b.x, b.y, b.r, rgba(b.cr / 3, b.cg / 3, b.cb / 3, alpha * 0.7f));
		fillCircle(r, b.x, b.y, b.r * 0.86f, rgba(b.cr, b.cg, b.cb, alpha * (b.down ? 0.95f : 0.6f)));
		ring(r, b.x, b.y, b.r, std::max(3.0f, b.r * 0.08f), rgba(std::min(255, b.cr + 60), std::min(255, b.cg + 60), std::min(255, b.cb + 60), alpha));
		drawIcon(r, b.icon, b.x, b.y, b.r * 0.55f, rgba(255, 255, 255, std::min(1.0f, alpha * 1.3f)));
	};
	for (int i = 0; i < buttonCount; ++i)
		drawButton(buttons[i]);
	if (mode_ == Mode::Game)
		drawButton(pauseBtn);
}

} // namespace mobile
