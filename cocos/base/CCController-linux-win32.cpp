/****************************************************************************
  Copyright (c) 2014 cocos2d-x.org
  Copyright (c) 2014-2016 Chukong Technologies Inc.
  Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
  Copyright (c) 2017 Wilson E. Alvarez <wilson.e.alvarez1@gmail.com>

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
 ****************************************************************************/

#include "base/CCController.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
#include <functional>
#include "base/ccMacros.h"
#include "base/CCDirector.h"
#include "base/CCScheduler.h"
#include "base/CCEventController.h"
#include "glfw3.h"

NS_CC_BEGIN

class CC_DLL ControllerImpl
{
	public:

		ControllerImpl()
		{
			//If extra controllers were added, make sure to add
			//them to the SDL Controller Database at
			//https://github.com/gabomdq/SDL_GameControllerDB
			//which GLFW uses for providing the new gamepad
			//API. We are going to map the GLFW game pad events to
			//Controller::Key key codes.

			// GENERIC MAPPING
			std::string deviceName = "SDL";
			std::unordered_map<int,int> buttonInputMap;
			std::unordered_map<int,int> axisInputMap;
			
			// Map the controller inputs to Controller::Key codes
			buttonInputMap[0]	= Controller::Key::BUTTON_A;
			buttonInputMap[1]	= Controller::Key::BUTTON_B;
			buttonInputMap[2]	= Controller::Key::BUTTON_X;
			buttonInputMap[3]	= Controller::Key::BUTTON_Y;
			buttonInputMap[4]	= Controller::Key::BUTTON_LEFT_SHOULDER;
			buttonInputMap[5]	= Controller::Key::BUTTON_RIGHT_SHOULDER;
			buttonInputMap[6]	= Controller::Key::BUTTON_SELECT;
			buttonInputMap[7]	= Controller::Key::BUTTON_START;
			buttonInputMap[8]	= Controller::Key::BUTTON_LEFT_THUMBSTICK;
			buttonInputMap[9]	= Controller::Key::BUTTON_RIGHT_THUMBSTICK;
			buttonInputMap[10]	= Controller::Key::BUTTON_DPAD_UP;
			buttonInputMap[11]	= Controller::Key::BUTTON_DPAD_RIGHT;
			buttonInputMap[12]	= Controller::Key::BUTTON_DPAD_DOWN;
			buttonInputMap[13]	= Controller::Key::BUTTON_DPAD_LEFT;

			axisInputMap[0]		= Controller::Key::JOYSTICK_LEFT_X;
			axisInputMap[1]		= Controller::Key::JOYSTICK_LEFT_Y;
			axisInputMap[2]		= Controller::Key::JOYSTICK_RIGHT_Y;
			axisInputMap[3]		= Controller::Key::JOYSTICK_RIGHT_X;
			axisInputMap[4]		= Controller::Key::AXIS_LEFT_TRIGGER;
			axisInputMap[5]		= Controller::Key::AXIS_RIGHT_TRIGGER;

			s_controllerProfiles.insert(std::make_pair(deviceName, std::make_pair(buttonInputMap, axisInputMap)));
		}


		~ControllerImpl()
		{
			// If there are any controllers connected that were not deleted at the end of execution, delete them.
			// This wil prevent any memory leaks showing up in valgrind
			for ( auto& controller : Controller::s_allController )
			{
				delete controller;
			}
			Controller::s_allController.clear();
		}

		static ControllerImpl* getInstance()
		{
			static ControllerImpl instance;
			return &instance;
		}

		static std::vector<Controller*>::iterator findController(int deviceId)
		{
			// When controllers are disconnected, GLFW cannot provide their names. We only get their deviceId.
			auto iter = std::find_if(Controller::s_allController.begin(), Controller::s_allController.end(),
									 [&](Controller* controller)
									 {
										 return deviceId == controller->_deviceId;
									 }
									);
			return iter;
		}

		static void onConnected(const std::string& deviceName, int deviceId)
		{
			std::string deviceSDL = "SDL";
                        
			// Check whether the controller is already registered
			auto iter = findController(deviceId);
			if (iter != Controller::s_allController.end())
				return;

			// It's a new controller being connected.
			auto controller = new cocos2d::Controller();
			controller->_deviceId = deviceId;
			controller->_deviceName = deviceName;
			Controller::s_allController.push_back(controller);

			// Check if we already have an available input controller profile. If so, attach it it to the controller.
			for (const auto & it : s_controllerProfiles)
			{
				if ( deviceSDL.compare(it.first) == 0 )
				{
					// Found controller profile. Attach it to the controller:
					CCLOG("ControllerImpl: Found input profile for controller: %s", deviceName.c_str());
					controller->_buttonInputMap = it.second.first;
					controller->_axisInputMap = it.second.second;


					// Show a one-time warning in debug mode for every button that's currently not matched in the input profile.
					// This will let the developers know that the mapping must be included in the constructor of ControllerImpl located above.
					#ifdef COCOS2D_DEBUG
					int count;
					glfwGetJoystickButtons(deviceId, &count);
					for ( int i = 0; i < count; ++i )
					{
						// Check the mapping of each button:
						const auto & it = controller->_buttonInputMap.find(i);
						if ( it == controller->_buttonInputMap.end() )
						{
							CCLOG("ControllerImpl: Could not find a button input mapping for controller \"%s\", and keyCode \"%d\". This keyCode will not match any from Controller::Key", controller->getDeviceName().c_str(), i);
						}
					}

					glfwGetJoystickAxes(deviceId, &count);
					for ( int i = 0; i < count; ++i )
					{
						// Check the mapping of each axis
						const auto & it = controller->_axisInputMap.find(i);
						if ( it == controller->_axisInputMap.end() )
						{
							CCLOG("ControllerImpl: Could not find an axis input mapping for controller \"%s\", and keyCode \"%d\". This keyCode will not match any from Controller::Key", controller->getDeviceName().c_str(), i);
						}
					}
					#endif

					break;
				}
			}

			// Show a warning if the controller input profile is non-existent:
			#ifdef COCOS2D_DEBUG
			if ( controller->_buttonInputMap.empty() )
			{
				CCLOG("ControllerImpl: Could not find a button input map for controller: %s", deviceName.c_str());
			}

			if ( controller->_axisInputMap.empty() )
			{
				CCLOG("ControllerImpl: Could not find an axis input map for controller: %s", deviceName.c_str());
			}
			#endif

			controller->onConnected();
		}

		static void onDisconnected(int deviceId)
		{
			// Check whether the controller is already registered
			auto iter = findController(deviceId);
			if (iter == Controller::s_allController.end())
			{
				CCLOGERROR("ControllerImpl Error: Could not find the controller!");
				return;
			}

			(*iter)->onDisconnected();
			Controller::s_allController.erase(iter);
		}

		static void onButtonEvent(int deviceId, int keyCode, bool isPressed, float value, bool isAnalog)
		{
			auto iter = findController(deviceId);
			if (iter == Controller::s_allController.end())
			{
				CCLOG("ControllerImpl::onButtonEvent: new controller detected. Registering...");
				onConnected(glfwGetJoystickName(deviceId), deviceId);
				iter = findController(deviceId);
			}

			(*iter)->onButtonEvent(keyCode, isPressed, value, isAnalog);
		}

		static void onAxisEvent(int deviceId, int axisCode, float value, bool isAnalog)
		{
			auto iter = findController(deviceId);
			if (iter == Controller::s_allController.end())
			{
				CCLOG("ControllerImpl::onAxisEvent: new controller detected. Registering...");
				onConnected(glfwGetJoystickName(deviceId), deviceId);
				iter = findController(deviceId);
			}

			(*iter)->onAxisEvent(axisCode, value, isAnalog);
		}

		static void handleConnectionsAndDisconnections(int deviceId, int event)
		{
			// glfw supports up to 16 game controllers
			// Handle game controller connections and disconnections
			if ( event == GLFW_CONNECTED )
			{
				onConnected(glfwGetJoystickName(deviceId), deviceId);
			}
			else if ( event == GLFW_DISCONNECTED )
			{
				ControllerImpl::getInstance()->onDisconnected(deviceId);
			}
			#ifdef COCOS2D_DEBUG
			else
			{
				CCLOG("ControllerImpl: Unhandled GLFW joystick event: %d", event);
			}
			#endif
		}

		void update(float /*dt*/)
		{
			for (int deviceId = GLFW_JOYSTICK_1;  deviceId <= GLFW_JOYSTICK_LAST;  ++deviceId)
			{
				if ( glfwJoystickPresent(deviceId) )
				{
					auto controller = Controller::getControllerByDeviceId(deviceId);

					// prevents weird crashes when glfw fails 
					//if (controller == NULL)
					//	continue;
					
					// Poll game controller button presses
					int count;
					const unsigned char* buttonArray = glfwGetJoystickButtons(deviceId, &count);
					for ( int i = 0; i < count; ++i )
					{
						// Debug gode to see the actual pressed button ids
						//if (buttonArray[i] == GLFW_PRESS)
						//	CCLOG("sdl button %i is down", i);
						
						// Map the button to the Controller:Key keys from the controller profile if it's available:
						int keyCode = i;
						const auto & it = controller->_buttonInputMap.find(keyCode);
						if ( it != controller->_buttonInputMap.end() )
						{
							keyCode = it->second;
						}
						ControllerImpl::onButtonEvent(deviceId, keyCode, buttonArray[i] == GLFW_PRESS, 0, false);
					}

					// Poll game controller joystick axis
					const float * axisArray = glfwGetJoystickAxes(deviceId, &count);
					for ( int i = 0; i < count; ++i )
					{
						// Map the axis to the Controller:Key keys from the controller profile if it's available:
						int keyCode = i;
						const auto & it = controller->_axisInputMap.find(keyCode);
						if ( it != controller->_axisInputMap.end() )
						{
							keyCode = it->second;
						}
						ControllerImpl::onAxisEvent(deviceId, keyCode, axisArray[i], true);
					}
				}
			}
		}

	private:
		//FIXME: Once GLFW 3.3 is bundled with cocos2d-x, remove these
		//controller profiles and all the related code.  We will only need to
		//provide a mapping from the GLFW gamepad key codes to the
		//Controller::Key keycodes. So far an std::unordered_map<int,int>
		//should suffice.
		static std::map<std::string, std::pair< std::unordered_map<int, int>, std::unordered_map<int, int> > > s_controllerProfiles;
};

std::map<std::string, std::pair< std::unordered_map<int, int>, std::unordered_map<int, int> > > ControllerImpl::s_controllerProfiles;

void Controller::startDiscoveryController()
{
	// make sure controller is initialized at this point
	ControllerImpl::getInstance();
    
	// Check for existing josyticks and register them as cocos2d::Controller:
	for (int deviceId = GLFW_JOYSTICK_1;  deviceId <= GLFW_JOYSTICK_LAST;  ++deviceId)
	{
		if (glfwJoystickPresent(deviceId))
		{
			int axis_count, button_count;
			glfwGetJoystickAxes(deviceId, &axis_count);
			glfwGetJoystickButtons(deviceId, &button_count);
			const char* name = glfwGetJoystickName(deviceId);
			ControllerImpl::onConnected(name, deviceId);
		}
	}

	// GFLW sends events when a joystick is connected and disconnected only.
	// These events need to be filtered:
	glfwSetJoystickCallback(ControllerImpl::handleConnectionsAndDisconnections);

	// Poll the joystick axis and buttons
	Director::getInstance()->getScheduler()->scheduleUpdate(ControllerImpl::getInstance(), 0, false);
}

void Controller::stopDiscoveryController()
{
	Director::getInstance()->getScheduler()->unscheduleUpdate(ControllerImpl::getInstance());
	glfwSetJoystickCallback(nullptr);

	// Also remove all the connected controllers:
	for ( auto& controller : Controller::s_allController )
	{
		delete controller;
	}
	Controller::s_allController.clear();
}

void Controller::registerListeners()
{
}

bool Controller::isConnected() const
{
	// If there is a controller instance, it means that the controller is connected.
	// If a controller is disconnected, the instance will be destroyed.
	// Thus, always returns true for this method.
	return true;
}

Controller::Controller()
	: _controllerTag(TAG_UNSET)
	, _impl(nullptr)
	, _connectEvent(nullptr)
	, _keyEvent(nullptr)
	  , _axisEvent(nullptr)
{
	init();
}

Controller::~Controller()
{
	delete _connectEvent;
	delete _keyEvent;
	delete _axisEvent;
}


NS_CC_END

#endif // #if (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
