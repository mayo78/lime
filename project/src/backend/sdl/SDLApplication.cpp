#include "SDLApplication.h"
#include "SDLGamepad.h"
#include "SDLJoystick.h"
#include <system/System.h>

#ifdef HX_MACOS
#include <unistd.h>
#endif

#ifdef EMSCRIPTEN
#include "emscripten.h"
#endif

#include <cmath>


namespace lime {


	AutoGCRoot* Application::callback = 0;
	SDLApplication* SDLApplication::currentApplication = 0;
	bool inBackground = false;


	const int analogAxisDeadZone = 1000;
	std::map<int, std::map<int, int> > gamepadsAxisMap;

	#ifndef LIME_SDL2
	SDL_SensorID accelerometerSensorID = -1;
	SDL_Sensor* accelerometerSensor = nullptr;


	SDL_SensorID gyroscopeSensorID = -1;
	SDL_Sensor* gyroscopeSensor = nullptr;
	#endif


	SDLApplication::SDLApplication () {

		#ifdef IPHONE
		SDL_SetHint (SDL_HINT_IOS_HIDE_HOME_INDICATOR, "3");
		#endif

		#ifndef LIME_SDL2
		initFlags = SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_SENSOR;
		#else
		initFlags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER | SDL_INIT_JOYSTICK;
		#endif

		#if defined(LIME_MOJOAL) || defined(LIME_OPENALSOFT)
		initFlags |= SDL_INIT_AUDIO;
		#endif

		if (!SDL_Init (initFlags)) {

			printf ("Could not initialize SDL LOL: %s.\n", SDL_GetError ());

		}

		#ifndef LIME_NX
		SDL_SetEventFilter (HandleAppLifecycleEvent, NULL);
		#endif

		currentApplication = this;

		#ifndef LIME_SDL2
		SDL_zero (frameTime);
		frameTime.target = (Uint64) std::llround (1e9 / 60.0);
		frameTime.previous = SDL_GetTicksNS ();

		active = false;
		#else
		framePeriod = 1000.0 / 60.0;

		currentUpdate = 0;
		lastUpdate = 0;
		nextUpdate = 0;

		ApplicationEvent applicationEvent;
		ClipboardEvent clipboardEvent;
		DropEvent dropEvent;
		GamepadEvent gamepadEvent;
		JoystickEvent joystickEvent;
		KeyEvent keyEvent;
		MouseEvent mouseEvent;
		RenderEvent renderEvent;
		SensorEvent sensorEvent;
		TextEvent textEvent;
		TouchEvent touchEvent;
		WindowEvent windowEvent;

		SDL_EventState (SDL_DROPFILE, SDL_ENABLE);
		SDLJoystick::Init ();
		#endif


		#ifndef LIME_NX
		InitializeSensors ();
		#endif

		#ifdef HX_MACOS
		const char *path = SDL_GetBasePath ();

		if (path) {

			chdir (path);

		}
		#endif

	}


	void SDLApplication::InitializeSensors () {

		#ifndef LIME_SDL2
		accelerometerSensorID = System::GetFirstAccelerometerSensorId ();

		if (accelerometerSensorID > 0) {

			accelerometerSensor = SDL_OpenSensor (accelerometerSensorID);

		}

		gyroscopeSensorID = System::GetFirstGyroscopeSensorId ();

		if (gyroscopeSensorID > 0) {

			gyroscopeSensor = SDL_OpenSensor (gyroscopeSensorID);

		}
		#endif

	}


	SDLApplication::~SDLApplication () {

		#ifndef LIME_SDL2
		if (gyroscopeSensor) {

			SDL_CloseSensor (gyroscopeSensor);
			gyroscopeSensor = nullptr;
			gyroscopeSensorID = -1;

		}

		if (accelerometerSensor) {

			SDL_CloseSensor (accelerometerSensor);
			accelerometerSensor = nullptr;
			accelerometerSensorID = -1;

		}
		#endif

	}


	int SDLApplication::Exec () {

		Init ();

		#ifdef EMSCRIPTEN
		emscripten_cancel_main_loop ();
		emscripten_set_main_loop (UpdateFrame, 0, 0);
		emscripten_set_main_loop_timing (EM_TIMING_RAF, 1);
		#endif

		#if defined(IPHONE) || defined(EMSCRIPTEN)

		return 0;

		#else

		while (active) {

			Update ();

		}

		return Quit ();

		#endif

	}


	void SDLApplication::HandleEvent (SDL_Event* event) {

		#if defined(IPHONE) || defined(EMSCRIPTEN)

		int top = 0;
		gc_set_top_of_stack(&top, false);

		#endif

		switch (event->type) {

			#ifdef LIME_SDL2
			case SDL_USEREVENT:

				if (!inBackground) {

					currentUpdate = SDL_GetTicks ();
					applicationEvent.type = UPDATE;
					applicationEvent.deltaTime = currentUpdate - lastUpdate;
					lastUpdate = currentUpdate;

					nextUpdate += framePeriod;

					while (nextUpdate <= currentUpdate) {

						nextUpdate += framePeriod;

					}

					ApplicationEvent::Dispatch (&applicationEvent);
					RenderEvent::Dispatch (&renderEvent);

				}

				break;

			case SDL_APP_WILLENTERBACKGROUND:

				inBackground = true;

				windowEvent.type = WINDOW_DEACTIVATE;
				WindowEvent::Dispatch (&windowEvent);
				break;

			case SDL_APP_WILLENTERFOREGROUND:

				break;

			case SDL_APP_DIDENTERFOREGROUND:

				windowEvent.type = WINDOW_ACTIVATE;
				WindowEvent::Dispatch (&windowEvent);

				inBackground = false;
				break;
			#endif

			#ifndef LIME_SDL2
			case SDL_EVENT_CLIPBOARD_UPDATE:
			#else
			case SDL_CLIPBOARDUPDATE:
			#endif

				ProcessClipboardEvent (event);
				break;

			#ifndef LIME_SDL2
			case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			case SDL_EVENT_GAMEPAD_BUTTON_UP:
			case SDL_EVENT_GAMEPAD_ADDED:
			case SDL_EVENT_GAMEPAD_REMOVED:
			#else
			case SDL_CONTROLLERAXISMOTION:
			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
			case SDL_CONTROLLERDEVICEADDED:
			case SDL_CONTROLLERDEVICEREMOVED:
			#endif

				ProcessGamepadEvent (event);
				break;

			#ifndef LIME_SDL2
			case SDL_EVENT_DROP_FILE:
			case SDL_EVENT_DROP_TEXT:
			case SDL_EVENT_DROP_BEGIN:
			case SDL_EVENT_DROP_COMPLETE:
			case SDL_EVENT_DROP_POSITION:
			#else
			case SDL_DROPFILE:
			#endif

				ProcessDropEvent (event);
				break;

			#ifndef LIME_SDL2
			case SDL_EVENT_FINGER_CANCELED:
			case SDL_EVENT_FINGER_MOTION:
			case SDL_EVENT_FINGER_DOWN:
			case SDL_EVENT_FINGER_UP:
			#else
			case SDL_FINGERMOTION:
			case SDL_FINGERDOWN:
			case SDL_FINGERUP:
			#endif

				ProcessTouchEvent (event);
				break;

			#ifndef LIME_SDL2
			case SDL_EVENT_JOYSTICK_AXIS_MOTION:
			#else
			case SDL_JOYAXISMOTION:
			#endif

				ProcessJoystickEvent (event);
				break;

			#ifndef LIME_SDL2
			case SDL_EVENT_JOYSTICK_BALL_MOTION:
			case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
			case SDL_EVENT_JOYSTICK_BUTTON_UP:
			case SDL_EVENT_JOYSTICK_HAT_MOTION:
			case SDL_EVENT_JOYSTICK_ADDED:
			case SDL_EVENT_JOYSTICK_REMOVED:
			#else
			case SDL_JOYBALLMOTION:
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
			case SDL_JOYHATMOTION:
			case SDL_JOYDEVICEADDED:
			case SDL_JOYDEVICEREMOVED:
			#endif

				ProcessJoystickEvent (event);
				break;

			#ifndef LIME_SDL2
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
			#else
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			#endif

				ProcessKeyEvent (event);
				break;

			#ifndef LIME_SDL2
			case SDL_EVENT_MOUSE_MOTION:
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			case SDL_EVENT_MOUSE_WHEEL:
			#else
			case SDL_MOUSEMOTION:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEWHEEL:
			#endif

				ProcessMouseEvent (event);
				break;

			#ifndef EMSCRIPTEN
			#ifndef LIME_SDL2
			case SDL_EVENT_RENDER_DEVICE_RESET:
			#else
			case SDL_RENDER_DEVICE_RESET:
			#endif

				renderEvent.type = RENDER_CONTEXT_LOST;
				RenderEvent::Dispatch (&renderEvent);

				renderEvent.type = RENDER_CONTEXT_RESTORED;
				RenderEvent::Dispatch (&renderEvent);

				#ifdef LIME_SDL2
				renderEvent.type = RENDER;
				#endif
				break;
			#endif

			#ifndef LIME_SDL2
			case SDL_EVENT_SENSOR_UPDATE:

				ProcessSensorEvent (event);
				break;

			#endif
			#ifndef LIME_SDL2
			case SDL_EVENT_TEXT_INPUT:
			case SDL_EVENT_TEXT_EDITING:
			#else
			case SDL_TEXTINPUT:
			case SDL_TEXTEDITING:
			#endif

				ProcessTextEvent (event);
				break;

			#ifndef LIME_SDL2
			case SDL_EVENT_WINDOW_MOUSE_ENTER:
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			case SDL_EVENT_WINDOW_SHOWN:
			case SDL_EVENT_WINDOW_HIDDEN:
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			case SDL_EVENT_WINDOW_FOCUS_LOST:
			case SDL_EVENT_WINDOW_MAXIMIZED:
			case SDL_EVENT_WINDOW_MINIMIZED:
			case SDL_EVENT_WINDOW_MOVED:
			case SDL_EVENT_WINDOW_RESTORED:
			case SDL_EVENT_WINDOW_EXPOSED:
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:

				ProcessWindowEvent(event);
				break;

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:

				ProcessWindowEvent(event);

				SDL_Event event;

				if (SDL_PollEvent(&event)) {

					if (event.type != SDL_EVENT_QUIT) {

						HandleEvent(&event);

					}

				}

				break;
			#else
			case SDL_WINDOWEVENT:

				switch (event->window.event) {

					case SDL_WINDOWEVENT_ENTER:
					case SDL_WINDOWEVENT_LEAVE:
					case SDL_WINDOWEVENT_SHOWN:
					case SDL_WINDOWEVENT_HIDDEN:
					case SDL_WINDOWEVENT_FOCUS_GAINED:
					case SDL_WINDOWEVENT_FOCUS_LOST:
					case SDL_WINDOWEVENT_MAXIMIZED:
					case SDL_WINDOWEVENT_MINIMIZED:
					case SDL_WINDOWEVENT_MOVED:
					case SDL_WINDOWEVENT_RESTORED:

						ProcessWindowEvent (event);
						break;

					case SDL_WINDOWEVENT_EXPOSED:

						ProcessWindowEvent (event);

						if (!inBackground) {

							RenderEvent::Dispatch (&renderEvent);

						}

						break;

					case SDL_WINDOWEVENT_SIZE_CHANGED:

						ProcessWindowEvent (event);

						if (!inBackground) {

							RenderEvent::Dispatch (&renderEvent);

						}

						break;

					case SDL_WINDOWEVENT_CLOSE:

						ProcessWindowEvent (event);

						// Avoid handling SDL_QUIT if in response to window.close
						SDL_Event event;

						if (SDL_PollEvent (&event)) {

							if (event.type != SDL_QUIT) {

								HandleEvent (&event);

							}

						}
						break;

				}

				break;
			#endif

			#ifndef LIME_SDL2
			case SDL_EVENT_QUIT:
			#else
			case SDL_QUIT:
			#endif

				active = false;
				break;

		}

	}


	void SDLApplication::Init () {

		active = true;
		#ifdef LIME_SDL2
		lastUpdate = SDL_GetTicks ();
		nextUpdate = lastUpdate;
		#endif

	}


	void SDLApplication::ProcessClipboardEvent (SDL_Event* event) {

		if (ClipboardEvent::callback) {

			clipboardEvent.type = CLIPBOARD_UPDATE;

			ClipboardEvent::Dispatch (&clipboardEvent);

		}

	}


	void SDLApplication::ProcessDropEvent (SDL_Event* event) {

		if (DropEvent::callback) {

			#ifndef LIME_SDL2
			switch (event->type) {

				case SDL_EVENT_DROP_FILE:
					dropEvent.type = DROP_FILE;
					break;

				case SDL_EVENT_DROP_TEXT:
					dropEvent.type = DROP_TEXT;
					break;

				case SDL_EVENT_DROP_BEGIN:
					dropEvent.type = DROP_BEGIN;
					break;

				case SDL_EVENT_DROP_COMPLETE:
					dropEvent.type = DROP_COMPLETE;
					break;

				case SDL_EVENT_DROP_POSITION:
					dropEvent.type = DROP_POSITION;
					break;

			}

			dropEvent.x = event->drop.x;
			dropEvent.y = event->drop.y;
			dropEvent.data = (vbyte*)event->drop.data;
			dropEvent.source = (vbyte*)event->drop.source;
			dropEvent.windowID = event->drop.windowID;
			DropEvent::Dispatch (&dropEvent);
			#else
			dropEvent.type = DROP_FILE;
			dropEvent.source = (vbyte*)event->drop.file;

			DropEvent::Dispatch (&dropEvent);
			SDL_free (dropEvent.source);
			#endif

		}

	}


	void SDLApplication::ProcessGamepadEvent (SDL_Event* event) {

		if (GamepadEvent::callback) {

			switch (event->type) {

				#ifdef LIME_SDL2
				case SDL_CONTROLLERAXISMOTION:

					if (gamepadsAxisMap[event->caxis.which].empty ()) {

						gamepadsAxisMap[event->caxis.which][event->caxis.axis] = event->caxis.value;

					} else if (gamepadsAxisMap[event->caxis.which][event->caxis.axis] == event->caxis.value) {

						break;

					}
				#else
				case SDL_EVENT_GAMEPAD_AXIS_MOTION:

					if (gamepadsAxisMap[event->gaxis.which].empty ()) {

						gamepadsAxisMap[event->gaxis.which][event->gaxis.axis] = event->gaxis.value;

					} else if (gamepadsAxisMap[event->gaxis.which][event->gaxis.axis] == event->gaxis.value) {

						break;

					}
				#endif

					gamepadEvent.type = GAMEPAD_AXIS_MOVE;
					#ifndef LIME_SDL2
					gamepadEvent.axis = event->gaxis.axis;
					gamepadEvent.id = event->gaxis.which;
					#else
					gamepadEvent.axis = event->caxis.axis;
					gamepadEvent.id = event->caxis.which;
					#endif

					#ifdef LIME_SDL2
					if (event->caxis.value > -analogAxisDeadZone && event->caxis.value < analogAxisDeadZone) {

						if (gamepadsAxisMap[event->caxis.which][event->caxis.axis] != 0) {

							gamepadsAxisMap[event->caxis.which][event->caxis.axis] = 0;
					#else
					if (event->gaxis.value > -analogAxisDeadZone && event->gaxis.value < analogAxisDeadZone) {

						if (gamepadsAxisMap[event->gaxis.which][event->gaxis.axis] != 0) {

							gamepadsAxisMap[event->gaxis.which][event->gaxis.axis] = 0;
					#endif
							gamepadEvent.axisValue = 0;
							GamepadEvent::Dispatch (&gamepadEvent);

						}

						break;

					}

					#ifndef LIME_SDL2
					gamepadsAxisMap[event->gaxis.which][event->gaxis.axis] = event->gaxis.value;
					gamepadEvent.axisValue = event->gaxis.value / (event->gaxis.value > 0 ? 32767.0 : 32768.0);
					gamepadEvent.timestamp = event->gaxis.timestamp;
					#else
					gamepadsAxisMap[event->caxis.which][event->caxis.axis] = event->caxis.value;
					gamepadEvent.axisValue = event->caxis.value / (event->caxis.value > 0 ? 32767.0 : 32768.0);
					#endif

					GamepadEvent::Dispatch (&gamepadEvent);
					break;

				#ifdef LIME_SDL2
				case SDL_CONTROLLERBUTTONDOWN:

					gamepadEvent.type = GAMEPAD_BUTTON_DOWN;
					gamepadEvent.button = event->cbutton.button;
					gamepadEvent.id = event->cbutton.which;
				#else
				case SDL_EVENT_GAMEPAD_BUTTON_DOWN:

					gamepadEvent.type = GAMEPAD_BUTTON_DOWN;
					gamepadEvent.button = event->gbutton.button;
					gamepadEvent.id = event->gbutton.which;
					gamepadEvent.timestamp = event->gbutton.timestamp;
				#endif

					GamepadEvent::Dispatch (&gamepadEvent);
					break;

				#ifdef LIME_SDL2
				case SDL_CONTROLLERBUTTONUP:

					gamepadEvent.type = GAMEPAD_BUTTON_UP;
					gamepadEvent.button = event->cbutton.button;
					gamepadEvent.id = event->cbutton.which;
				#else
				case SDL_EVENT_GAMEPAD_BUTTON_UP:

					gamepadEvent.type = GAMEPAD_BUTTON_UP;
					gamepadEvent.button = event->gbutton.button;
					gamepadEvent.id = event->gbutton.which;
					gamepadEvent.timestamp = event->gbutton.timestamp;
				#endif

					GamepadEvent::Dispatch (&gamepadEvent);
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_GAMEPAD_ADDED:
				#else
				case SDL_CONTROLLERDEVICEADDED:
				#endif

					if (SDLGamepad::Connect (event->cdevice.which)) {

						gamepadEvent.type = GAMEPAD_CONNECT;
						gamepadEvent.id = SDLGamepad::GetInstanceID (event->cdevice.which);
						#ifndef LIME_SDL2
						gamepadEvent.timestamp = event->cdevice.timestamp;
						#endif

						GamepadEvent::Dispatch (&gamepadEvent);

					}

					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_GAMEPAD_REMOVED: {
				#else
				case SDL_CONTROLLERDEVICEREMOVED: {
				#endif

					gamepadEvent.type = GAMEPAD_DISCONNECT;
					gamepadEvent.id = event->cdevice.which;
					#ifndef LIME_SDL2
					gamepadEvent.timestamp = event->cdevice.timestamp;
					#endif

					GamepadEvent::Dispatch (&gamepadEvent);
					SDLGamepad::Disconnect (event->cdevice.which);
					break;

				}

			}

		}

	}


	void SDLApplication::ProcessJoystickEvent (SDL_Event* event) {

		if (JoystickEvent::callback) {

			switch (event->type) {

				#ifndef LIME_SDL2
				case SDL_EVENT_JOYSTICK_AXIS_MOTION:
				#else
				case SDL_JOYAXISMOTION:
				#endif

					joystickEvent.type = JOYSTICK_AXIS_MOVE;
					joystickEvent.index = event->jaxis.axis;
					joystickEvent.x = event->jaxis.value / (event->jaxis.value > 0 ? 32767.0 : 32768.0);
					joystickEvent.id = event->jaxis.which;

					JoystickEvent::Dispatch (&joystickEvent);
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
				#else
				case SDL_JOYBUTTONDOWN:
				#endif

					joystickEvent.type = JOYSTICK_BUTTON_DOWN;
					joystickEvent.index = event->jbutton.button;
					joystickEvent.id = event->jbutton.which;

					JoystickEvent::Dispatch (&joystickEvent);
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_JOYSTICK_BUTTON_UP:
				#else
				case SDL_JOYBUTTONUP:
				#endif

					joystickEvent.type = JOYSTICK_BUTTON_UP;
					joystickEvent.index = event->jbutton.button;
					joystickEvent.id = event->jbutton.which;

					JoystickEvent::Dispatch (&joystickEvent);
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_JOYSTICK_HAT_MOTION:
				#else
				case SDL_JOYHATMOTION:
				#endif

					joystickEvent.type = JOYSTICK_HAT_MOVE;
					joystickEvent.index = event->jhat.hat;
					joystickEvent.eventValue = event->jhat.value;
					joystickEvent.id = event->jhat.which;

					JoystickEvent::Dispatch (&joystickEvent);
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_JOYSTICK_ADDED:
				#else
				case SDL_JOYDEVICEADDED:
				#endif

					joystickEvent.type = JOYSTICK_CONNECT;
					joystickEvent.id = SDLJoystick::GetInstanceID (event->jdevice.which);

					JoystickEvent::Dispatch (&joystickEvent);
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_JOYSTICK_REMOVED:
				#else
				case SDL_JOYDEVICEREMOVED:
				#endif

					joystickEvent.type = JOYSTICK_DISCONNECT;
					joystickEvent.id = event->jdevice.which;

					JoystickEvent::Dispatch (&joystickEvent);
					SDLJoystick::Disconnect (event->jdevice.which);
					break;

			}

		}

	}


	void SDLApplication::ProcessKeyEvent (SDL_Event* event) {

		if (KeyEvent::callback) {

			switch (event->type) {

				#ifndef LIME_SDL2
				case SDL_EVENT_KEY_DOWN: keyEvent.type = KEY_DOWN; break;
				case SDL_EVENT_KEY_UP: keyEvent.type = KEY_UP; break;
				#else
				case SDL_KEYDOWN: keyEvent.type = KEY_DOWN; break;
				case SDL_KEYUP: keyEvent.type = KEY_UP; break;
				#endif

			}

			#ifndef LIME_SDL2
			keyEvent.keyCode = event->key.key;
			keyEvent.modifier = event->key.mod;
			keyEvent.windowID = event->key.windowID;
			keyEvent.timestamp = event->key.timestamp;
			#else
			keyEvent.keyCode = event->key.keysym.sym;
			keyEvent.modifier = event->key.keysym.mod;
			keyEvent.windowID = event->key.windowID;
			#endif

			if (keyEvent.type == KEY_DOWN) {

				#ifndef LIME_SDL2
				if (keyEvent.keyCode == SDLK_CAPSLOCK) keyEvent.modifier |= SDL_KMOD_CAPS;
				if (keyEvent.keyCode == SDLK_LALT) keyEvent.modifier |= SDL_KMOD_LALT;
				if (keyEvent.keyCode == SDLK_LCTRL) keyEvent.modifier |= SDL_KMOD_LCTRL;
				if (keyEvent.keyCode == SDLK_LGUI) keyEvent.modifier |= SDL_KMOD_LGUI;
				if (keyEvent.keyCode == SDLK_LSHIFT) keyEvent.modifier |= SDL_KMOD_LSHIFT;
				if (keyEvent.keyCode == SDLK_MODE) keyEvent.modifier |= SDL_KMOD_MODE;
				if (keyEvent.keyCode == SDLK_NUMLOCKCLEAR) keyEvent.modifier |= SDL_KMOD_NUM;
				if (keyEvent.keyCode == SDLK_RALT) keyEvent.modifier |= SDL_KMOD_RALT;
				if (keyEvent.keyCode == SDLK_RCTRL) keyEvent.modifier |= SDL_KMOD_RCTRL;
				if (keyEvent.keyCode == SDLK_RGUI) keyEvent.modifier |= SDL_KMOD_RGUI;
				if (keyEvent.keyCode == SDLK_RSHIFT) keyEvent.modifier |= SDL_KMOD_RSHIFT;
				#else
				if (keyEvent.keyCode == SDLK_CAPSLOCK) keyEvent.modifier |= KMOD_CAPS;
				if (keyEvent.keyCode == SDLK_LALT) keyEvent.modifier |= KMOD_LALT;
				if (keyEvent.keyCode == SDLK_LCTRL) keyEvent.modifier |= KMOD_LCTRL;
				if (keyEvent.keyCode == SDLK_LGUI) keyEvent.modifier |= KMOD_LGUI;
				if (keyEvent.keyCode == SDLK_LSHIFT) keyEvent.modifier |= KMOD_LSHIFT;
				if (keyEvent.keyCode == SDLK_MODE) keyEvent.modifier |= KMOD_MODE;
				if (keyEvent.keyCode == SDLK_NUMLOCKCLEAR) keyEvent.modifier |= KMOD_NUM;
				if (keyEvent.keyCode == SDLK_RALT) keyEvent.modifier |= KMOD_RALT;
				if (keyEvent.keyCode == SDLK_RCTRL) keyEvent.modifier |= KMOD_RCTRL;
				if (keyEvent.keyCode == SDLK_RGUI) keyEvent.modifier |= KMOD_RGUI;
				if (keyEvent.keyCode == SDLK_RSHIFT) keyEvent.modifier |= KMOD_RSHIFT;
				#endif
			}

			KeyEvent::Dispatch (&keyEvent);

		}

	}


	void SDLApplication::ProcessMouseEvent (SDL_Event* event) {

		if (MouseEvent::callback) {

			#ifndef LIME_SDL2
			SDL_Window * sdlWindow = SDL_GetWindowFromID(event->window.windowID);
			float scale = SDL_GetWindowPixelDensity(sdlWindow) / SDL_GetWindowDisplayScale(sdlWindow);
			#else
			float scale = 1.0;
			#endif

			switch (event->type) {

				#ifndef LIME_SDL2
				case SDL_EVENT_MOUSE_MOTION:
				#else
				case SDL_MOUSEMOTION:
				#endif

					mouseEvent.type = MOUSE_MOVE;
					mouseEvent.x = event->motion.x * scale;
					mouseEvent.y = event->motion.y * scale;
					mouseEvent.movementX = event->motion.xrel * scale;
					mouseEvent.movementY = event->motion.yrel * scale;
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
				#else
				case SDL_MOUSEBUTTONDOWN:
				#endif

					#ifndef LIME_SDL2
					SDL_CaptureMouse (true);
					#else
					SDL_CaptureMouse (SDL_TRUE);
					#endif

					mouseEvent.type = MOUSE_DOWN;
					mouseEvent.button = event->button.button - 1;
					mouseEvent.x = event->button.x * scale;
					mouseEvent.y = event->button.y * scale;
					mouseEvent.clickCount = event->button.clicks;
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_MOUSE_BUTTON_UP:
				#else
				case SDL_MOUSEBUTTONUP:
				#endif

					#ifndef LIME_SDL2
					SDL_CaptureMouse (false);
					#else
					SDL_CaptureMouse (SDL_FALSE);
					#endif

					mouseEvent.type = MOUSE_UP;
					mouseEvent.button = event->button.button - 1;
					mouseEvent.x = event->button.x * scale;
					mouseEvent.y = event->button.y * scale;
					mouseEvent.clickCount = event->button.clicks;
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_MOUSE_WHEEL:
				#else
				case SDL_MOUSEWHEEL:
				#endif

					mouseEvent.type = MOUSE_WHEEL;

					if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {

						mouseEvent.x = -event->wheel.x;
						mouseEvent.y = -event->wheel.y;

					} else {

						mouseEvent.x = event->wheel.x;
						mouseEvent.y = event->wheel.y;

					}
					break;

			}

			mouseEvent.windowID = event->button.windowID;
			MouseEvent::Dispatch (&mouseEvent);

		}

	}


	void SDLApplication::ProcessSensorEvent(SDL_Event* event) {

		if (SensorEvent::callback) {

			#ifndef LIME_SDL2
			if (event->sensor.which == accelerometerSensorID) {

				sensorEvent.type = SENSOR_ACCELEROMETER;
				sensorEvent.id = event->sensor.which;
				sensorEvent.x = event->sensor.data[0];
				sensorEvent.y = event->sensor.data[1];
				sensorEvent.z = event->sensor.data[2];
				SensorEvent::Dispatch(&sensorEvent);

			} else if (event->sensor.which == gyroscopeSensorID) {

				sensorEvent.type = SENSOR_GYROSCOPE;
				sensorEvent.id = event->sensor.which;
				sensorEvent.x = event->sensor.data[0];
				sensorEvent.y = event->sensor.data[1];
				sensorEvent.z = event->sensor.data[2];
				SensorEvent::Dispatch(&sensorEvent);

			}
			#endif

		}

	}


	void SDLApplication::ProcessTextEvent (SDL_Event* event) {

		if (TextEvent::callback) {

			switch (event->type) {

				#ifndef LIME_SDL2
				case SDL_EVENT_TEXT_INPUT:
				#else
				case SDL_TEXTINPUT:
				#endif

					textEvent.type = TEXT_INPUT;
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_TEXT_EDITING:
				#else
				case SDL_TEXTEDITING:
				#endif

					textEvent.type = TEXT_EDIT;
					textEvent.start = event->edit.start;
					textEvent.length = event->edit.length;
					break;

			}

			if (textEvent.text) {

				free (textEvent.text);

			}

			textEvent.text = (vbyte*)malloc (strlen (event->text.text) + 1);
			strcpy ((char*)textEvent.text, event->text.text);

			textEvent.windowID = event->text.windowID;
			TextEvent::Dispatch (&textEvent);

		}

	}


	void SDLApplication::ProcessTouchEvent (SDL_Event* event) {

		if (TouchEvent::callback) {

			switch (event->type) {

				#ifndef LIME_SDL2
				case SDL_EVENT_FINGER_MOTION:
				#else
				case SDL_FINGERMOTION:
				#endif

					touchEvent.type = TOUCH_MOVE;
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_FINGER_DOWN:
				#else
				case SDL_FINGERDOWN:
				#endif

					touchEvent.type = TOUCH_START;
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_FINGER_CANCELED:
				case SDL_EVENT_FINGER_UP:
				#else
				case SDL_FINGERUP:
				#endif

					touchEvent.type = TOUCH_END;
					break;

			}

			// position values are in the range 0...1, so don't scale them
			touchEvent.x = event->tfinger.x;
			touchEvent.y = event->tfinger.y;
			#ifdef LIME_SDL2
			touchEvent.id = event->tfinger.fingerId;
			#else
			touchEvent.id = event->tfinger.fingerID;
			#endif
			touchEvent.dx = event->tfinger.dx;
			touchEvent.dy = event->tfinger.dy;
			touchEvent.pressure = event->tfinger.pressure;
			#ifndef LIME_SDL2
			touchEvent.device = event->tfinger.touchID;
			#else
			touchEvent.device = event->tfinger.touchId;
			#endif

			TouchEvent::Dispatch (&touchEvent);

		}

	}


	void SDLApplication::ProcessWindowEvent (SDL_Event* event) {

		if (WindowEvent::callback) {

			#ifdef LIME_SDL2
			switch (event->window.event) {

				case SDL_WINDOWEVENT_SHOWN: windowEvent.type = WINDOW_SHOW; break;
				case SDL_WINDOWEVENT_CLOSE: windowEvent.type = WINDOW_CLOSE; break;
				case SDL_WINDOWEVENT_HIDDEN: windowEvent.type = WINDOW_HIDE; break;
				case SDL_WINDOWEVENT_ENTER: windowEvent.type = WINDOW_ENTER; break;
				case SDL_WINDOWEVENT_FOCUS_GAINED: windowEvent.type = WINDOW_FOCUS_IN; break;
				case SDL_WINDOWEVENT_FOCUS_LOST: windowEvent.type = WINDOW_FOCUS_OUT; break;
				case SDL_WINDOWEVENT_LEAVE: windowEvent.type = WINDOW_LEAVE; break;
				case SDL_WINDOWEVENT_MAXIMIZED: windowEvent.type = WINDOW_MAXIMIZE; break;
				case SDL_WINDOWEVENT_MINIMIZED: windowEvent.type = WINDOW_MINIMIZE; break;
				case SDL_WINDOWEVENT_EXPOSED: windowEvent.type = WINDOW_EXPOSE; break;

				case SDL_WINDOWEVENT_MOVED:
			#else
			switch (event->type) {

				case SDL_EVENT_WINDOW_SHOWN: windowEvent.type = WINDOW_SHOW; break;
				case SDL_EVENT_WINDOW_CLOSE_REQUESTED: windowEvent.type = WINDOW_CLOSE; break;
				case SDL_EVENT_WINDOW_HIDDEN: windowEvent.type = WINDOW_HIDE; break;
				case SDL_EVENT_WINDOW_MOUSE_ENTER: windowEvent.type = WINDOW_ENTER; break;
				case SDL_EVENT_WINDOW_FOCUS_GAINED: windowEvent.type = WINDOW_FOCUS_IN; break;
				case SDL_EVENT_WINDOW_FOCUS_LOST: windowEvent.type = WINDOW_FOCUS_OUT; break;
				case SDL_EVENT_WINDOW_MOUSE_LEAVE: windowEvent.type = WINDOW_LEAVE; break;
				case SDL_EVENT_WINDOW_MAXIMIZED: windowEvent.type = WINDOW_MAXIMIZE; break;
				case SDL_EVENT_WINDOW_MINIMIZED: windowEvent.type = WINDOW_MINIMIZE; break;
				case SDL_EVENT_WINDOW_EXPOSED: windowEvent.type = WINDOW_EXPOSE; break;

				case SDL_EVENT_WINDOW_MOVED:
			#endif

					windowEvent.type = WINDOW_MOVE;
					windowEvent.x = event->window.data1;
					windowEvent.y = event->window.data2;
					break;

				#ifndef LIME_SDL2
				case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				#else
				case SDL_WINDOWEVENT_SIZE_CHANGED:
				#endif
				{
					#ifndef LIME_SDL2
					SDL_Window * sdlWindow = SDL_GetWindowFromID(event->window.windowID);
					float scale = SDL_GetWindowDisplayScale(sdlWindow);
					#else
					float scale = 1.0;
					#endif
					windowEvent.type = WINDOW_RESIZE;
					windowEvent.width = (int)(event->window.data1 / scale);
					windowEvent.height = (int)(event->window.data2 / scale);
					break;

				}
				#ifndef LIME_SDL2
				case SDL_EVENT_WINDOW_RESTORED: windowEvent.type = WINDOW_RESTORE; break;
				#else
				case SDL_WINDOWEVENT_RESTORED: windowEvent.type = WINDOW_RESTORE; break;
				#endif

			}

			windowEvent.windowID = event->window.windowID;
			WindowEvent::Dispatch (&windowEvent);

		}

	}


	int SDLApplication::Quit () {

		applicationEvent.type = EXIT;
		ApplicationEvent::Dispatch (&applicationEvent);

		#ifndef LIME_SDL2
		SDL_QuitSubSystem (initFlags);
		#endif

		SDL_Quit ();

		return 0;

	}


	void SDLApplication::RegisterWindow (SDLWindow *window) {

		#ifdef IPHONE
		SDL_SetiOSAnimationCallback (window->sdlWindow, 1, UpdateFrame, NULL);
		#endif

	}


	// new updating
	#ifndef LIME_SDL2
	void SDLApplication::SetFrameRate (double frameRate) {

		frameTime.target = frameRate < 1 ? 0 : (Uint64) std::llround (1e9 / frameRate);

	}


	bool SDLApplication::Update () {

		SDL_Event event;

		while (SDL_PollEvent (&event)) {

			HandleEvent (&event);

			if (!active)
				return active;

		}

		if (!inBackground) {

			applicationEvent.type = UPDATE;
			applicationEvent.deltaTime = std::fmax (0.0, (double)frameTime.frame / 1e6); // Use the duration of the *previous frame* for deltaTime
			ApplicationEvent::Dispatch (&applicationEvent);

			renderEvent.type = RENDER;
			RenderEvent::Dispatch (&renderEvent);

		}

		// Measure the total duration of the current frame (update + render)
		frameTime.current = SDL_GetTicksNS ();
		frameTime.frame = frameTime.current - frameTime.previous;
		frameTime.previous = frameTime.current;

		// If the frame was faster than the target frame time, delay to cap FPS
		if (frameTime.frame < frameTime.target) {

			// Pause for the remaining time to maintain a consistent frame rate
			SDL_DelayPrecise (frameTime.target - frameTime.frame);

			// Measure the actual time spent waiting and add it to frameTime
			frameTime.current = SDL_GetTicksNS ();
			frameTime.frame += frameTime.current - frameTime.previous;
			frameTime.previous = frameTime.current;

		}

		return active;

	}


	bool SDLApplication::HandleAppLifecycleEvent (void* userdata, SDL_Event* event) {

		#if defined(IPHONE) || defined(EMSCRIPTEN)

		int top = 0;
		gc_set_top_of_stack(&top, false);

		#endif

		switch (event->type) {

			case SDL_EVENT_TERMINATING:

				return false;

			case SDL_EVENT_LOW_MEMORY:

				return false;

			case SDL_EVENT_WILL_ENTER_BACKGROUND:

				return false;

			case SDL_EVENT_DID_ENTER_BACKGROUND:

				inBackground = true;
				currentApplication->windowEvent.type = WINDOW_DEACTIVATE;
				WindowEvent::Dispatch (&currentApplication->windowEvent);
				return false;

			case SDL_EVENT_WILL_ENTER_FOREGROUND:

				return false;

			case SDL_EVENT_DID_ENTER_FOREGROUND:

				currentApplication->windowEvent.type = WINDOW_ACTIVATE;
				WindowEvent::Dispatch (&currentApplication->windowEvent);
				inBackground = false;
				return false;

			default:

				return true;

		}

	}
	// old updating
	#else
	void SDLApplication::SetFrameRate (double frameRate) {

		if (frameRate > 0) {

			framePeriod = 1000.0 / frameRate;

		} else {

			framePeriod = 1000.0;

		}

	}


	static SDL_TimerID timerID = 0;
	bool timerActive = false;
	bool firstTime = true;

	Uint32 OnTimer (Uint32 interval, void *) {

		SDL_Event event;
		SDL_UserEvent userevent;
		userevent.type = SDL_USEREVENT;
		userevent.code = 0;
		userevent.data1 = NULL;
		userevent.data2 = NULL;
		event.type = SDL_USEREVENT;
		event.user = userevent;

		timerActive = false;
		timerID = 0;

		SDL_PushEvent (&event);

		return 0;

	}


	bool SDLApplication::Update () {

		SDL_Event event;
		event.type = -1;

		#if (!defined (IPHONE) && !defined (EMSCRIPTEN))

		if (active && (firstTime || WaitEvent (&event))) {

			firstTime = false;

			HandleEvent (&event);
			event.type = -1;
			if (!active)
				return active;

		#endif

			while (SDL_PollEvent (&event)) {

				HandleEvent (&event);
				event.type = -1;
				if (!active)
					return active;

			}

			currentUpdate = SDL_GetTicks ();

		#if defined (IPHONE) || defined (EMSCRIPTEN)

			if (currentUpdate >= nextUpdate) {

				event.type = SDL_USEREVENT;
				HandleEvent (&event);
				event.type = -1;

			}

		#else

			if (currentUpdate >= nextUpdate) {

				if (timerActive) SDL_RemoveTimer (timerID);
				OnTimer (0, 0);

			} else if (!timerActive) {

				timerActive = true;
				timerID = SDL_AddTimer (nextUpdate - currentUpdate, OnTimer, 0);

			}

		}

		#endif

		return active;

	}
	#endif


	void SDLApplication::UpdateFrame () {

		#ifdef EMSCRIPTEN
		System::GCTryExitBlocking ();
		#endif

		currentApplication->Update ();

		#ifdef EMSCRIPTEN
		System::GCTryEnterBlocking ();
		#endif

	}


	void SDLApplication::UpdateFrame (void*) {

		UpdateFrame ();

	}


	#ifdef LIME_SDL2
	int SDLApplication::WaitEvent (SDL_Event *event) {

		#if defined(HX_MACOS) || defined(ANDROID)

		System::GCEnterBlocking ();
		int result = SDL_WaitEvent (event);
		System::GCExitBlocking ();
		return result;

		#else

		bool isBlocking = false;

		for(;;) {

			SDL_PumpEvents ();

			switch (SDL_PeepEvents (event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {

				case -1:

					if (isBlocking) System::GCExitBlocking ();
					return 0;

				case 1:

					if (isBlocking) System::GCExitBlocking ();
					return 1;

				default:

					if (!isBlocking) System::GCEnterBlocking ();
					isBlocking = true;
					SDL_Delay (1);
					break;

			}

		}

		#endif

	}
	#endif


	Application* CreateApplication () {

		return new SDLApplication ();

	}


}


#ifdef ANDROID
int SDL_main (int argc, char *argv[]) { return 0; }
#endif
