#include "SDLGamepad.h"


namespace lime {


	#ifndef LIME_SDL2
	std::map<int, SDL_Gamepad*> gameControllers = std::map<int, SDL_Gamepad*> ();
	#else
	std::map<int, SDL_GameController*> gameControllers = std::map<int, SDL_GameController*> ();
	#endif
	std::map<int, int> gameControllerIDs = std::map<int, int> ();


	bool SDLGamepad::Connect (int deviceID) {

		#ifndef LIME_SDL2
		if (SDL_IsGamepad (deviceID)) {
		#else
		if (SDL_IsGameController (deviceID)) {
		#endif

			#ifndef LIME_SDL2
			SDL_Gamepad *gameController = SDL_OpenGamepad (deviceID);
			#else
			SDL_GameController *gameController = SDL_GameControllerOpen (deviceID);
			#endif

			if (gameController) {

				#ifndef LIME_SDL2
				SDL_Joystick *joystick = SDL_GetGamepadJoystick (gameController);
				int id = SDL_GetJoystickID (joystick);
				#else
				SDL_Joystick *joystick = SDL_GameControllerGetJoystick (gameController);
				int id = SDL_JoystickInstanceID (joystick);
				#endif

				gameControllers[id] = gameController;
				gameControllerIDs[deviceID] = id;

				return true;

			}

		}

		return false;

	}


	bool SDLGamepad::Disconnect (int id) {

		if (gameControllers.find (id) != gameControllers.end ()) {

			#ifndef LIME_SDL2
			SDL_Gamepad *gameController = gameControllers[id];
			SDL_CloseGamepad (gameController);
			#else
			SDL_GameController *gameController = gameControllers[id];
			SDL_GameControllerClose (gameController);
			#endif
			gameControllers.erase (id);

			return true;

		}

		return false;

	}


	int SDLGamepad::GetInstanceID (int deviceID) {

		return gameControllerIDs[deviceID];

	}


	void Gamepad::AddMapping (const char* content) {

		#ifndef LIME_SDL2
		SDL_AddGamepadMapping (content);
		#else
		SDL_GameControllerAddMapping (content);
		#endif

	}


	const char* Gamepad::GetDeviceGUID (int id) {

		#ifndef LIME_SDL2
		SDL_Joystick* joystick = SDL_GetGamepadJoystick (gameControllers[id]);
		#else
		SDL_Joystick* joystick = SDL_GameControllerGetJoystick (gameControllers[id]);
		#endif

		if (joystick) {

			char* guid = new char[64];
			#ifndef LIME_SDL2
			SDL_GUIDToString (SDL_GetJoystickGUID (joystick), guid, 64);
			#else
			SDL_JoystickGetGUIDString (SDL_JoystickGetGUID (joystick), guid, 64);
			#endif
			return guid;

		}

		return 0;

	}


	const char* Gamepad::GetDeviceName (int id) {

		#ifndef LIME_SDL2
		return SDL_GetGamepadName (gameControllers[id]);
		#else
		return SDL_GameControllerName (gameControllers[id]);
		#endif

	}
}