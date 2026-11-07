#ifndef LIERO_MOBILE_TOUCH_HPP
#define LIERO_MOBILE_TOUCH_HPP

// Mobile input layer: on-screen touch controls + SDL GameController mapping.
//
// Everything here is translated into the *same* virtual key presses the PC
// version receives from a keyboard (DOS scan codes from the player's control
// settings), so the game rules/physics code is not touched at all.

#include <SDL.h>
#include <cstdint>
#include <deque>

struct Controller;

namespace mobile {

enum class Mode { Menu, Game };

struct TouchConfig
{
	float scale = 1.0f;      // button size multiplier
	float opacity = 0.6f;    // 0.1 .. 1.0
	bool leftHanded = false; // mirror layout (buttons left, stick right)
	bool visible = true;     // hidden while a gamepad is in use
	float deadZone = 0.22f;  // fraction of stick radius
	bool vibrate = true;
	bool digButton = true;
};

// Logical virtual inputs (merged from touch + gamepad).
enum VIn
{
	VUp, VDown, VLeft, VRight,
	VFire, VJump, VChange, VRope,
	VSwapTap,     // tap = next weapon (Change held + Right pulse)
	VEnter, VEscape,
	VDig,         // Left+Right together (original "dig")
	VPrevTap,     // previous weapon (Change held + Left pulse)
	VCount
};

struct Input
{
	void init(TouchConfig const& cfg);
	void layout(int winW, int winH, SDL_Rect const& gameRect);
	void relayout() { layout(w_, h_, game_); }
	void setMode(Mode m);
	Mode mode() const { return mode_; }

	// Returns true if the event was consumed.
	bool handleEvent(SDL_Event const& ev, Controller* controller);
	// Called once per frame (drives pulse sequences like weapon swap).
	void update(Controller* controller);
	void draw(SDL_Renderer* r);
	void releaseAll(Controller* controller);

	// Switch to weapon slot `target` (0..4) from `current` via the original
	// Change + Left/Right key sequence.
	void selectWeapon(int current, int target);

	// Horizontal free space between the stick and the action buttons (for the HUD).
	void hudBounds(float& left, float& right) const;

	TouchConfig cfg;

	// Layout queries (used by the automated screenshot test).
	bool buttonCenter(VIn in, float& x, float& y) const
	{
		if (in == VEscape && mode_ == Mode::Game) { x = pauseBtn.x; y = pauseBtn.y; return true; }
		for (int i = 0; i < buttonCount; ++i)
			if (buttons[i].in == in) { x = buttons[i].x; y = buttons[i].y; return true; }
		return false;
	}
	void stickCenter(float& x, float& y, float& r) const { x = stickX; y = stickY; r = stickR; }
	int width() const { return w_; }
	int height() const { return h_; }

private:
	struct Button
	{
		float x = 0, y = 0, r = 0;
		VIn in = VFire;
		uint8_t cr = 255, cg = 255, cb = 255;
		int icon = 0;
		bool down = false;
		SDL_FingerID finger = -1;
	};

	void applyKey(uint32_t dosKey, bool down, Controller* controller);
	void sync(Controller* controller);
	uint32_t wormKey(int control) const;
	void buzz();

	Mode mode_ = Mode::Menu;
	int w_ = 0, h_ = 0;
	SDL_Rect game_ {0, 0, 320, 200};

	// joystick (game) / d-pad (menu)
	float stickX = 0, stickY = 0, stickR = 0;
	SDL_FingerID stickFinger = -1;
	float knobDX = 0, knobDY = 0;
	float originX = 0, originY = 0; // stick origin for the current touch (floating)
	bool stickDir[4] = {}; // up, down, left, right

	static const int MaxButtons = 8;
	Button buttons[MaxButtons];
	int buttonCount = 0;
	Button pauseBtn;

	// gamepad state
	bool padBtn[VCount] = {};
	bool padDir[4] = {};
	bool usingPad = false;

	// resulting desired state of each virtual input + what was sent
	bool want[VCount] = {};
	bool sentDos[256] = {};
	uint32_t pressedAt[256] = {}; // frame a key went down (min. hold = 1 game frame)
	uint32_t frameNo = 1;

	// Scripted key sequence (weapon change). Bits: 1 = Change, 2 = Right, 4 = Left.
	std::deque<uint8_t> seq;
	uint8_t seqNow = 0;
	bool lastSwap = false, lastPrev = false;
	bool trigDown[2] = {};

	SDL_Haptic* haptic = nullptr;
	bool hapticTried = false;
};

extern Input input;

} // namespace mobile

#endif
