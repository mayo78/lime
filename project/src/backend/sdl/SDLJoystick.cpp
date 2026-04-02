#include "SDLJoystick.h"


namespace lime {


	#ifdef LIME_SDL2
	static SDL_Joystick* accelerometer = 0;
	static SDL_JoystickID accelerometerID = -1;
	#endif
	std::map<int, int> joystickIDs = std::map<int, int> ();
	std::map<int, SDL_Joystick*> joysticks = std::map<int, SDL_Joystick*> ();


	bool SDLJoystick::Connect (int deviceID) {

		#ifndef LIME_SDL2
		SDL_Joystick* joystick = SDL_OpenJoystick (deviceID);
		int id = SDL_GetJoystickID (joystick);
		#else
		SDL_Joystick* joystick = SDL_JoystickOpen (deviceID);
		int id = SDL_JoystickInstanceID (joystick);

		if (deviceID == accelerometerID) {
			return false;
		}
		#endif

		if (joystick) {

			joysticks[id] = joystick;
			joystickIDs[deviceID] = id;
			return true;

		}

		return false;

	}


	bool SDLJoystick::Disconnect (int id) {

		if (joysticks.find (id) != joysticks.end ()) {

			SDL_Joystick* joystick = joysticks[id];
			#ifndef LIME_SDL2
			SDL_CloseJoystick (joystick);
			#else
			SDL_JoystickClose (joystick);
			#endif
			joysticks.erase (id);
			return true;

		}

		return false;

	}


	int SDLJoystick::GetInstanceID (int deviceID) {

		return joystickIDs[deviceID];

	}


	#ifdef LIME_SDL2
	void SDLJoystick::Init () {

		#if defined(IPHONE) || defined(ANDROID) || defined(TVOS)
		for (int i = 0; i < SDL_NumJoysticks (); i++) {

			if (strstr (SDL_JoystickNameForIndex (i), "Accelerometer")) {

				accelerometer = SDL_JoystickOpen (i);
				accelerometerID = SDL_JoystickInstanceID (accelerometer);

			}

		}
		#endif

	}


	bool SDLJoystick::IsAccelerometer (int id) {

		return (id == accelerometerID);

	}
	#endif


	const char* Joystick::GetDeviceGUID (int id) {

		char* guid = new char[64];
		#ifndef LIME_SDL2
		SDL_GUIDToString (SDL_GetJoystickGUID (joysticks[id]), guid, 64);
		#else
		SDL_JoystickGetGUIDString (SDL_JoystickGetGUID (joysticks[id]), guid, 64);
		#endif
		return guid;

	}


	const char* Joystick::GetDeviceName (int id) {

		#ifndef LIME_SDL2
		return SDL_GetJoystickName (joysticks[id]);
		#else
		return SDL_JoystickName (joysticks[id]);
		#endif

	}


	int Joystick::GetNumAxes (int id) {

		#ifndef LIME_SDL2
		return SDL_GetNumJoystickAxes (joysticks[id]);
		#else
		return SDL_JoystickNumAxes (joysticks[id]);
		#endif

	}


	int Joystick::GetNumButtons (int id) {

		#ifndef LIME_SDL2
		return SDL_GetNumJoystickButtons (joysticks[id]);
		#else
		return SDL_JoystickNumButtons (joysticks[id]);
		#endif

	}


	int Joystick::GetNumHats (int id) {

		#ifndef LIME_SDL2
		return SDL_GetNumJoystickHats (joysticks[id]);
		#else
		return SDL_JoystickNumHats (joysticks[id]);
		#endif

	}


}
