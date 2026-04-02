#ifndef LIME_SDL_JOYSTICK_H
#define LIME_SDL_JOYSTICK_H



#ifdef LIME_SDL2
#include <SDL.h>
#else
#include <SDL3/SDL.h>
#endif
#include <ui/Joystick.h>
#include <map>


namespace lime {


	class SDLJoystick {

		public:

			static bool Connect (int id);
			static bool Disconnect (int id);
			static int GetInstanceID (int deviceID);
			#ifdef LIME_SDL2
			static void Init ();
			static bool IsAccelerometer (int id);
			#endif

	};


}


#endif