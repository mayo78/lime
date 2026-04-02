


#ifdef LIME_SDL2
#include <SDL.h>
#else
#include <SDL3/SDL.h>
#endif
#include <ui/KeyCode.h>


namespace lime {


	int32_t KeyCode::FromScanCode (int32_t scanCode) {

		#ifndef LIME_SDL2
		return SDL_GetKeyFromScancode ((SDL_Scancode)scanCode, SDL_KMOD_NONE, false);
		#else
		return SDL_GetKeyFromScancode ((SDL_Scancode)scanCode);
		#endif

	}


	int32_t KeyCode::ToScanCode (int32_t keyCode) {

		#ifndef LIME_SDL2
		return SDL_GetScancodeFromKey ((SDL_Keycode)keyCode, SDL_KMOD_NONE);
		#else
		return SDL_GetScancodeFromKey ((SDL_Keycode)keyCode);
		#endif

	}


}