#if (CC_TARGET_PLATFORM == CC_PLATFORM_WINRT)
#ifndef cocos2d_libs_CCController_xbox_h
#define cocos2d_libs_CCController_xbox_h
#include "base/CCController.h"
#include <functional>

using namespace Windows::Gaming::Input;
using namespace Windows::UI::Core;


namespace cocos2d {

	constexpr float AXIS_DEADZONE = 0.15f;
	constexpr float TRIGGER_DEADZONE = 0.5f;

	class CC_DLL ControllerXBox : public Controller {
	private:

		//const ButtonMapping* buttonMapping = NULL;
		/**
		 * xbox 360 left/right trigger share same axis
		 */
		bool isSharedTriggerAxis = true;
		Gamepad^ m_gamepad;

		std::set<Controller::Key> m_pressedButtons;

		void handleAxis(Controller::Key key, float val);
		void handleTrigger(Controller::Key key, float val);
		void handleButton(Controller::Key key, GamepadButtons read, GamepadButtons compare);
	public:
		ControllerXBox(Gamepad^ gamepad);
		~ControllerXBox();
		//void setButtonMapping(const ButtonMapping* b);
		static void pollActions();
		friend ref class ControllerImpl;
	};

	ref class ControllerImpl {

	public:
		void OnGamepadAdded(_In_ Object^ sender, _In_ Gamepad^ gamepad);
		void OnGamepadRemoved(_In_ Object^ sender, _In_ Gamepad^ gamepad);
	};

}

#endif
#endif
