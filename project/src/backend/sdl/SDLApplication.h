#ifndef LIME_SDL_APPLICATION_H
#define LIME_SDL_APPLICATION_H



#ifdef LIME_SDL2
#include <SDL.h>
#else
#include <SDL3/SDL.h>
#endif
#include <app/Application.h>
#include <app/ApplicationEvent.h>
#include <graphics/RenderEvent.h>
#include <system/ClipboardEvent.h>
//#include <system/OrientationEvent.h>
#include <system/SensorEvent.h>
#include <ui/DropEvent.h>
#include <ui/GamepadEvent.h>
#include <ui/JoystickEvent.h>
#include <ui/KeyEvent.h>
#include <ui/MouseEvent.h>
#include <ui/TextEvent.h>
#include <ui/TouchEvent.h>
#include <ui/WindowEvent.h>
#include "SDLWindow.h"


namespace lime {

	#ifndef LIME_SDL2
	struct FrameTime {
		Uint64 current;
		Uint64 previous;
		Uint64 frame;
		Uint64 target;
	};
	#endif


	class SDLApplication : public Application {

		public:

			SDLApplication ();
			~SDLApplication ();

			virtual int Exec ();
			virtual void Init ();
			virtual int Quit ();
			virtual void SetFrameRate (double frameRate);
			virtual bool Update ();

			void RegisterWindow (SDLWindow *window);

		private:
			void InitializeSensors();

			void HandleEvent (SDL_Event* event);
			void ProcessClipboardEvent (SDL_Event* event);
			void ProcessDropEvent (SDL_Event* event);
			void ProcessGamepadEvent (SDL_Event* event);
			void ProcessJoystickEvent (SDL_Event* event);
			void ProcessKeyEvent (SDL_Event* event);
			void ProcessMouseEvent (SDL_Event* event);
			void ProcessSensorEvent (SDL_Event* event);
			void ProcessTextEvent (SDL_Event* event);
			void ProcessTouchEvent (SDL_Event* event);
			void ProcessWindowEvent (SDL_Event* event);
			#ifdef LIME_SDL2
			int WaitEvent (SDL_Event* event);
			#endif

			#ifndef LIME_SDL2
			static bool HandleAppLifecycleEvent (void* userdata, SDL_Event* event);
			#endif
			static void UpdateFrame ();
			static void UpdateFrame (void*);

			static SDLApplication* currentApplication;

			#ifndef LIME_SDL2
			FrameTime frameTime;
			#endif
			bool active;

			ApplicationEvent applicationEvent;
			ClipboardEvent clipboardEvent;
			Uint32 initFlags;
			#ifdef LIME_SDL2
			Uint32 currentUpdate;
			double framePeriod;
			#endif
			DropEvent dropEvent;
			GamepadEvent gamepadEvent;
			JoystickEvent joystickEvent;
			KeyEvent keyEvent;
			#ifdef LIME_SDL2
			Uint32 lastUpdate;
			#endif
			MouseEvent mouseEvent;
			#ifdef LIME_SDL2
			Uint32 nextUpdate;
			#endif
			RenderEvent renderEvent;
			SensorEvent sensorEvent;
			TextEvent textEvent;
			TouchEvent touchEvent;
			WindowEvent windowEvent;

	};


}


#endif
