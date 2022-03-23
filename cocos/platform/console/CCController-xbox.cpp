#include "CCController-xbox.h"

using namespace cocos2d;
using namespace Windows::Foundation;
using namespace Windows::Gaming::Input;
using namespace Windows::UI::Xaml;

ControllerXBox::ControllerXBox(Gamepad^ gamepad) : m_gamepad(gamepad) {
	this->_deviceId = gamepad->GetHashCode();
	this->_deviceName = "xboxc_" + std::to_string(this->getDeviceId());
	this->setTag(this->getDeviceId());
	this->isSharedTriggerAxis = false;
}

ControllerXBox::~ControllerXBox() {
	this->m_pressedButtons.clear();
}

void ControllerXBox::pollActions() {
	//for some reason this can't be done at instance level, it's better to handle it this way also so if the gamepad
	//changes, you're always listening to any gamepad
	for (int i = 0; i < Gamepad::Gamepads->Size; i++) {
		auto gp = Gamepad::Gamepads->GetAt(i);
		auto reading = gp->GetCurrentReading();

		ControllerXBox* controller = (ControllerXBox*)Controller::getControllerByDeviceId(gp->GetHashCode());
		if (controller != nullptr) {
			//AXES
			controller->handleAxis(Controller::Key::JOYSTICK_LEFT_X, static_cast<float>(reading.LeftThumbstickX));
			controller->handleAxis(Controller::Key::JOYSTICK_LEFT_Y, static_cast<float>(-reading.LeftThumbstickY));
			controller->handleAxis(Controller::Key::JOYSTICK_RIGHT_X, static_cast<float>(reading.RightThumbstickX));
			controller->handleAxis(Controller::Key::JOYSTICK_RIGHT_Y, static_cast<float>(-reading.RightThumbstickY));

			//DPAD Buttons
			controller->handleButton(Controller::Key::BUTTON_DPAD_UP, reading.Buttons, GamepadButtons::DPadUp);
			controller->handleButton(Controller::Key::BUTTON_DPAD_RIGHT, reading.Buttons, GamepadButtons::DPadRight);
			controller->handleButton(Controller::Key::BUTTON_DPAD_DOWN, reading.Buttons, GamepadButtons::DPadDown);
			controller->handleButton(Controller::Key::BUTTON_DPAD_LEFT, reading.Buttons, GamepadButtons::DPadLeft);

			//CONTROL Buttons
			controller->handleButton(Controller::Key::BUTTON_START, reading.Buttons, GamepadButtons::Menu);
			controller->handleButton(Controller::Key::BUTTON_SELECT, reading.Buttons, GamepadButtons::View);

			//FACE Buttons
			controller->handleButton(Controller::Key::BUTTON_A, reading.Buttons, GamepadButtons::A);
			controller->handleButton(Controller::Key::BUTTON_B, reading.Buttons, GamepadButtons::B);
			controller->handleButton(Controller::Key::BUTTON_X, reading.Buttons, GamepadButtons::X);
			controller->handleButton(Controller::Key::BUTTON_Y, reading.Buttons, GamepadButtons::Y);

			//SHOULDERS and THUMBSTICKS
			controller->handleButton(Controller::Key::BUTTON_LEFT_SHOULDER, reading.Buttons, GamepadButtons::LeftShoulder);
			controller->handleButton(Controller::Key::BUTTON_LEFT_THUMBSTICK, reading.Buttons, GamepadButtons::LeftThumbstick);
			controller->handleButton(Controller::Key::BUTTON_RIGHT_SHOULDER, reading.Buttons, GamepadButtons::RightShoulder);
			controller->handleButton(Controller::Key::BUTTON_RIGHT_THUMBSTICK, reading.Buttons, GamepadButtons::RightThumbstick);

			//TRIGGERS
			controller->handleTrigger(Controller::Key::AXIS_LEFT_TRIGGER, static_cast<float>(reading.LeftTrigger));
			controller->handleTrigger(Controller::Key::AXIS_RIGHT_TRIGGER, static_cast<float>(reading.RightTrigger));
		}
	}
}

void ControllerXBox::handleAxis(Controller::Key key, float val) {
	if (this->m_axisValues.count(key) == 0 || std::abs(this->m_axisValues[key] - val) > AXIS_DEADZONE) {
		if (this->m_axisValues.count(key) > 0) {
			this->m_axisValues.erase(key);
		}
		if (std::abs(val) < AXIS_DEADZONE) {
			val = 0.f;
		}
		this->m_axisValues[key] = val;
		this->onAxisEvent(key, val, true);
	}
}

void ControllerXBox::handleTrigger(Controller::Key key, float val) {
	//For now we're handling triggers as buttons
	auto pressStored = this->m_pressedButtons.count(key) > 0;
	if (val > TRIGGER_DEADZONE) {
		if (!pressStored) {
			this->m_pressedButtons.insert(key);
			this->onButtonEvent(key, true, 1.f, false);
		}
	} else if (pressStored) {
		this->m_pressedButtons.erase(key);
		this->onButtonEvent(key, false, 0.f, false);
	}
}

void ControllerXBox::handleButton(Controller::Key key, GamepadButtons read, GamepadButtons compare) {
	auto pressStored = this->m_pressedButtons.count(key) > 0;
	if ((read & compare) == compare) {
		if (!pressStored) {
			this->m_pressedButtons.insert(key);
			this->onButtonEvent(key, true, 1.f, false);
		}
	} else if (pressStored) {
		this->m_pressedButtons.erase(key);
		this->onButtonEvent(key, false, 0.f, false);
	}
}


Controller::Controller()
	: _controllerTag(TAG_UNSET)
	, _impl(ref new ControllerImpl())
	, _connectEvent(nullptr)
	, _keyEvent(nullptr)
	, _axisEvent(nullptr) {
	init();
}

Controller::~Controller() {
	//I assume it's not needed when using ref new for the impl.
}

void Controller::startDiscoveryController() {
	auto cont = new cocos2d::Controller(); //We should improve this as it is keeping a "root" reference
	//to a controller with no events, just to keep the tracking implementation.
	//The rationale behind this for now is that the controllers are managed objects on the dispatching thread
	//and marshalling errors occur if this is not kept there
	Gamepad::GamepadAdded +=
		ref new EventHandler<Gamepad^>(cont->_impl, &ControllerImpl::OnGamepadAdded);

	Gamepad::GamepadRemoved +=
		ref new EventHandler<Gamepad^>(cont->_impl, &ControllerImpl::OnGamepadRemoved);
}

void ControllerImpl::OnGamepadAdded(Object^ sender, Gamepad^ gamepad) {

	auto controller = new cocos2d::ControllerXBox(gamepad);
	
	Controller::s_allController.push_back(controller);
	controller->onConnected();
	CCLOG("Gamepad added %d", controller->getDeviceId());
}

void ControllerImpl::OnGamepadRemoved(Object^ sender, Gamepad^ gamepad) {

	auto controller = Controller::getControllerByDeviceId(gamepad->GetHashCode());
	if (controller) {
		Controller::s_allController.erase(std::remove(Controller::s_allController.begin(), Controller::s_allController.end(), controller), Controller::s_allController.end());
		controller->onDisconnected();
		CCLOG("Gamepad removed %d", controller->getDeviceId());
	}
}

void Controller::stopDiscoveryController() {
	//Gamepad::GamepadAdded -= NULL;
}
