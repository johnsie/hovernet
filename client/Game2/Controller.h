// Controller.h
//
// This file contains the definition for the HoverRace::Client::Controller
// class.  It contains all the code to control input devices; joysticks, 
// keyboards, and the like.

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <vector>

#include "OIS/OIS.h"
#include "OIS/OISInputManager.h"
#include "OIS/OISException.h"
#include "OIS/OISKeyboard.h"
#include "OIS/OISMouse.h"
#include "OIS/OISJoyStick.h"
#include "OIS/OISEvents.h"
#include "../../engine/Util/Config.h"

#define	CTL_MOTOR_ON	1
#define CTL_LEFT		2
#define CTL_RIGHT		3
#define CTL_JUMP		4
#define CTL_BRAKE		5
#define CTL_FIRE		6
#define CTL_WEAPON		7
#define CTL_LOOKBACK	8

#define SET_CONTROL		(WM_USER + 1)

// maybe later... TODO
//#include "OIS/OISForceFeedback.h"

using namespace OIS;

namespace HoverRace {
namespace Client {
namespace Control {

/***
 * Contains information on the current control state.  Eventually, its members should
 * be made analog instead of digital (well, the ones that can, at least).
 */
struct ControlState {
	// TODO: make these inputs analog, not digital
	bool motorOn;
	bool jump;
	bool brake;
	bool fire;
	bool weapon;
	bool lookBack;
	bool right;
	bool left;
};

/***
 * The InputControl struct contains key/mouse/joystick bindings for one particular
 * control.
 */
struct InputControl {
	int inputType; /// OISKeyboard, OISMouse, OISJoyStick

	// keyboard
	int kbdBinding; // see OIS KeyCode class

	// mouse or joystick
	int button;
	int axis;
	int pov; // joystick only
	int slider; // joystick only
	int sensitivity; // defines how quickly we get to the maximum value
};

/***
 * The HoverRace::Client::Control::Controller class handles all the input of HoverRace.
 * It tracks all of the input devices, loads key mapping configuration, and returns the
 * current control state when it is asked for.  It is meant to replace Richard's old
 * system that was pretty ugly.
 */
class Controller {
	public:
		Controller(HWND mainWindow);
		~Controller();

		void poll();
		ControlState getState(int player) const;
		void captureNextInput(int control, int player, HWND hwnd);
		void disableInput(int control, int player);
		bool controlsUpdated();
		std::string toString(HoverRace::Util::Config::cfg_controls_t::cfg_control_t control);

	private:
		void InitInputManager(HWND mainWindow);
		void LoadControllerConfig();

		/// OIS input manager does most of the work for us
		InputManager *mgr;

		// now the input devices
		Keyboard *kbd;
		Mouse    *mouse;

		int numJoys;
		JoyStick **joys; /// we can have over 9000 joysticks, depending on how much RAM we have

		// of course... I have to write my own freaking handler... argh

		/***
		 * The Controller::EventHandler class is an internal class that listens for any 
		 * events.  When an event is received, it modifies the current ControlState, which
		 * is returned when the getControlState() function is called.
		 */
		class EventHandler : public KeyListener, public MouseListener, public JoyStickListener {
			public:
				EventHandler();
				~EventHandler();

				bool keyPressed(const KeyEvent &arg);
				bool keyReleased(const KeyEvent &arg);
				bool mouseMoved(const MouseEvent &arg);
				bool mousePressed(const MouseEvent &arg, MouseButtonID id);
				bool mouseReleased(const MouseEvent &arg, MouseButtonID id);
				bool buttonPressed(const JoyStickEvent &arg, int button);
				bool buttonReleased(const JoyStickEvent &arg, int button);
				bool axisMoved(const JoyStickEvent &arg, int axis);
				bool povMoved(const JoyStickEvent &arg, int pov);
				bool vector3Moved(const JoyStickEvent &arg, int index);

				// for polling
				ControlState getControlState(int player) const;
				void updateControlState(int keyCode, bool pressed); // accepts only keycodes for now

				void setControls(HoverRace::Util::Config::cfg_controls_t *controls);
				void captureNextInput(int control, int player, HWND hwnd);
				void disableControl(int control, int player);
				bool controlsUpdated();

			private:
				ControlState curState[HoverRace::Util::Config::MAX_PLAYERS];

				// kept locally to make things a bit quicker
				InputControl brake[HoverRace::Util::Config::MAX_PLAYERS];
				InputControl fire[HoverRace::Util::Config::MAX_PLAYERS];
				InputControl jump[HoverRace::Util::Config::MAX_PLAYERS];
				InputControl left[HoverRace::Util::Config::MAX_PLAYERS];
				InputControl lookBack[HoverRace::Util::Config::MAX_PLAYERS];
				InputControl motorOn[HoverRace::Util::Config::MAX_PLAYERS];
				InputControl right[HoverRace::Util::Config::MAX_PLAYERS];
				InputControl weapon[HoverRace::Util::Config::MAX_PLAYERS];

				InputControl toInputControl(HoverRace::Util::Config::cfg_controls_t::cfg_control_t control);

				bool captureNext;
				int captureControl;
				int capturePlayerId;
				HWND captureHwnd;
				bool updated;
		};

		EventHandler handler;
};

} // namespace Control
} // namespace Client
} // namespace HoverRace

#endif