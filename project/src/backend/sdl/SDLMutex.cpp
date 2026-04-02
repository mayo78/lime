#include <system/Mutex.h>

#ifdef LIME_SDL2
#include <SDL.h>
#else
#include <SDL3/SDL.h>
#endif


namespace lime {


	Mutex::Mutex () {

		mutex = SDL_CreateMutex ();

	}


	Mutex::~Mutex () {

		if (mutex) {

			#ifndef LIME_SDL2
			SDL_DestroyMutex ((SDL_Mutex*)mutex);
			#else
			SDL_DestroyMutex ((SDL_mutex*)mutex);
			#endif

		}

	}


	bool Mutex::Lock () const {

		if (mutex) {

			#ifndef LIME_SDL2
			SDL_LockMutex ((SDL_Mutex*)mutex);
			return true;
			#else
			return SDL_LockMutex ((SDL_mutex*)mutex) == 0;
			#endif

		}

		return false;

	}


	bool Mutex::TryLock () const {

		if (mutex) {

			#ifndef LIME_SDL2
			return SDL_TryLockMutex ((SDL_Mutex*)mutex);
			#else
			return SDL_TryLockMutex ((SDL_mutex*)mutex) == 0;
			#endif

		}

		return false;

	}


	bool Mutex::Unlock () const {

		if (mutex) {

			#ifndef LIME_SDL2
			SDL_UnlockMutex ((SDL_Mutex*)mutex);
			return true;
			#else
			return SDL_UnlockMutex ((SDL_mutex*)mutex) == 0;
			#endif

		}

		return false;

	}


}