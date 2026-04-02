#include "SDLWindow.h"
#include "SDLCursor.h"
#include "SDLApplication.h"
#include "system/System.h"
#include "../../graphics/opengl/OpenGLBindings.h"

#include <vector>
#include <cstring>

namespace lime {


	static Cursor currentCursor = DEFAULT;

	SDL_Cursor* SDLCursor::arrowCursor = 0;
	SDL_Cursor* SDLCursor::crosshairCursor = 0;
	SDL_Cursor* SDLCursor::moveCursor = 0;
	SDL_Cursor* SDLCursor::pointerCursor = 0;
	SDL_Cursor* SDLCursor::resizeNESWCursor = 0;
	SDL_Cursor* SDLCursor::resizeNSCursor = 0;
	SDL_Cursor* SDLCursor::resizeNWSECursor = 0;
	SDL_Cursor* SDLCursor::resizeWECursor = 0;
	SDL_Cursor* SDLCursor::textCursor = 0;
	SDL_Cursor* SDLCursor::waitCursor = 0;
	SDL_Cursor* SDLCursor::waitArrowCursor = 0;

	#ifdef LIME_SDL2
	static bool displayModeSet = false;
	#endif


	SDLWindow::SDLWindow (Application* application, int width, int height, int flags, const char* title) {

		sdlTexture = 0;
		sdlRenderer = 0;
		context = 0;

		contextWidth = 0;
		contextHeight = 0;

		currentApplication = application;
		this->flags = flags;

		int sdlWindowFlags = 0;

		#ifndef LIME_SDL2
		if (flags & WINDOW_FLAG_FULLSCREEN) sdlWindowFlags |= SDL_WINDOW_FULLSCREEN;
		#else
		if (flags & WINDOW_FLAG_FULLSCREEN) sdlWindowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		#endif
		if (flags & WINDOW_FLAG_RESIZABLE) sdlWindowFlags |= SDL_WINDOW_RESIZABLE;
		#ifndef LIME_SDL2
		if (flags & WINDOW_FLAG_TRANSPARENT) sdlWindowFlags |= SDL_WINDOW_TRANSPARENT;
		#endif
		if (flags & WINDOW_FLAG_BORDERLESS) sdlWindowFlags |= SDL_WINDOW_BORDERLESS;
		#ifndef LIME_SDL2
		if (flags & WINDOW_FLAG_ALLOW_HIGHDPI) sdlWindowFlags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
		#endif
		if (flags & WINDOW_FLAG_HIDDEN) sdlWindowFlags |= SDL_WINDOW_HIDDEN;
		if (flags & WINDOW_FLAG_MINIMIZED) sdlWindowFlags |= SDL_WINDOW_MINIMIZED;
		if (flags & WINDOW_FLAG_MAXIMIZED) sdlWindowFlags |= SDL_WINDOW_MAXIMIZED;

		#ifndef EMSCRIPTEN
		if (flags & WINDOW_FLAG_ALWAYS_ON_TOP) sdlWindowFlags |= SDL_WINDOW_ALWAYS_ON_TOP;
		#endif

		if (flags & WINDOW_FLAG_HARDWARE) {

			sdlWindowFlags |= SDL_WINDOW_OPENGL;

			#ifdef LIME_OPENGL_GLES2
			SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
			SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute (SDL_GL_CONTEXT_MINOR_VERSION, 0);
			#endif

			#ifdef LIME_OPENGL_GL
			// TODO: Use OpenGL 3.3 Core on Desktop
			#endif

			if (flags & WINDOW_FLAG_DEPTH_BUFFER) {

				SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 32 - ((flags & WINDOW_FLAG_STENCIL_BUFFER) ? 8 : 0));

			}

			if (flags & WINDOW_FLAG_STENCIL_BUFFER) {

				SDL_GL_SetAttribute (SDL_GL_STENCIL_SIZE, 8);

			}

			if (flags & WINDOW_FLAG_HW_AA_HIRES) {

				SDL_GL_SetAttribute (SDL_GL_MULTISAMPLEBUFFERS, true);
				SDL_GL_SetAttribute (SDL_GL_MULTISAMPLESAMPLES, 4);

			} else if (flags & WINDOW_FLAG_HW_AA) {

				SDL_GL_SetAttribute (SDL_GL_MULTISAMPLEBUFFERS, true);
				SDL_GL_SetAttribute (SDL_GL_MULTISAMPLESAMPLES, 2);

			}

			if (flags & WINDOW_FLAG_COLOR_DEPTH_32_BIT) {

				SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 8);
				SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 8);
				SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 8);
				SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, 8);

			} else {

				SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 5);
				SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 6);
				SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 5);

			}

		}

		#ifndef LIME_SDL2
		float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
		sdlWindow = SDL_CreateWindow (title, (int)(width * scale), (int)(height * scale), sdlWindowFlags);
		#else
		float scale = 1.0;
		sdlWindow = SDL_CreateWindow (title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, sdlWindowFlags);
		#endif

		if (!sdlWindow) {

			printf ("Could not create SDL window: %s.\n", SDL_GetError ());
			return;

		}
		#ifdef LIME_SDL2
		int sdlRendererFlags = 0;
		#endif

		if (flags & WINDOW_FLAG_HARDWARE) {
			#ifdef LIME_SDL2
			sdlRendererFlags |= SDL_RENDERER_ACCELERATED;
			#endif

			context = SDL_GL_CreateContext (sdlWindow);

			#ifdef LIME_SDL2
			if (context && SDL_GL_MakeCurrent (sdlWindow, context)) {
			#else
			if (context && SDL_GL_MakeCurrent (sdlWindow, context) == 0) {
			#endif

				#ifndef LIME_SDL2
				if (flags & WINDOW_FLAG_VSYNC) {

					SetVSync (WINDOW_VSYNC_ON);

				} else {

					SetVSync (WINDOW_VSYNC_OFF);

				}
				#else
				if (flags & WINDOW_FLAG_VSYNC) {

					SDL_GL_SetSwapInterval (1);

				} else {

					SDL_GL_SetSwapInterval (0);

				}
				#endif

				OpenGLBindings::Init ();

				#if !defined(LIME_GLES) && defined(LIME_SDL2)

				int version = 0;
				glGetIntegerv (GL_MAJOR_VERSION, &version);

				if (version == 0) {

					float versionScan = 0;
					sscanf ((const char*)glGetString (GL_VERSION), "%f", &versionScan);
					version = versionScan;

				}

				if (version < 2 && !strstr ((const char*)glGetString (GL_VERSION), "OpenGL ES")) {

					SDL_GL_DeleteContext (context);
					context = 0;

				}
				#endif
				#if defined(IPHONE) || defined(APPLETV)
				SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
				OpenGLBindings::defaultFramebuffer = (int)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER, 0);
				OpenGLBindings::defaultRenderbuffer = (int)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_UIKIT_OPENGL_RENDERBUFFER_NUMBER, 0);
				#endif

			} else {

				#ifndef LIME_SDL2
				SDL_GL_DestroyContext (context);
				#else
				SDL_GL_DeleteContext (context);
				#endif
				context = NULL;

			}

		}

		if (!context) {

			#ifndef LIME_SDL2
			sdlRenderer = SDL_CreateRenderer (sdlWindow, SDL_SOFTWARE_RENDERER);
			#else
			sdlRendererFlags &= ~SDL_RENDERER_ACCELERATED;
			sdlRendererFlags &= ~SDL_RENDERER_PRESENTVSYNC;

			sdlRendererFlags |= SDL_RENDERER_SOFTWARE;

			sdlRenderer = SDL_CreateRenderer (sdlWindow, -1, sdlRendererFlags);

			#endif

		}

		if (context || sdlRenderer) {

			((SDLApplication*)currentApplication)->RegisterWindow (this);

		} else {

			printf ("Could not create SDL renderer: %s.\n", SDL_GetError ());

		}

	}


	SDLWindow::~SDLWindow () {

		if (sdlWindow) {

			SDL_DestroyWindow (sdlWindow);
			sdlWindow = 0;

		}

		if (sdlRenderer) {

			SDL_DestroyRenderer (sdlRenderer);

		} else if (context) {

			#ifndef LIME_SDL2
			SDL_GL_DestroyContext (context);
			#else
			SDL_GL_DeleteContext (context);
			#endif

		}

	}


	int SDLWindow::Alert (int type, const char* message, const char* title, const char** buttons, int count) {

		#ifdef LIME_SDL2
		if (message) {

			SDL_ShowSimpleMessageBox (SDL_MESSAGEBOX_INFORMATION, title, message, sdlWindow);

		}

		return 0;
		#else
		SDL_MessageBoxFlags flags = SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;

		switch (type)
		{
			case 0:
				flags |= SDL_MESSAGEBOX_ERROR;
				break;

			case 1:
				flags |= SDL_MESSAGEBOX_WARNING;
				break;

			case 2:
				flags |= SDL_MESSAGEBOX_INFORMATION;
				break;

		}

		SDL_MessageBoxData data;
		SDL_zero (data);
		data.flags = flags;
		data.title = title;
		data.message = message;
		data.window = sdlWindow;

		std::vector<SDL_MessageBoxButtonData> sdlButtons;

		sdlButtons.reserve (count);

		for (int i = 0; i < count; ++i) {

			SDL_MessageBoxButtonData button;
			SDL_zero (button);
			button.buttonID = i;
			button.text = buttons[i];
			sdlButtons.push_back (button);

		}

		data.numbuttons = sdlButtons.size ();
		data.buttons = sdlButtons.data ();

		int buttonID;

		if (!SDL_ShowMessageBox (&data, &buttonID)) {

			buttonID = -1;

		}

		return buttonID;
		#endif
	}


	bool SDLWindow::SetVSync (int mode) {
		int res = SDL_GL_SetSwapInterval (mode);
		return res == mode || res == 0; // 0 sometimes means a success on some contexts?
	}


	void SDLWindow::Close () {

		if (sdlWindow) {

			SDL_DestroyWindow (sdlWindow);
			sdlWindow = 0;

		}

	}


	bool SDLWindow::SetVisible (bool visible) {

		if (visible) {

			SDL_ShowWindow (sdlWindow);

		} else {

			SDL_HideWindow (sdlWindow);

		}

		#ifndef LIME_SDL2
		return !(SDL_GetWindowFlags (sdlWindow) & SDL_WINDOW_HIDDEN);
		#else
		return (SDL_GetWindowFlags (sdlWindow) & SDL_WINDOW_SHOWN);
		#endif

	}


	void SDLWindow::ContextFlip () {

		#ifndef LIME_SDL2
		if (context) {
		#else
		if (context && !sdlRenderer) {
		#endif

			SDL_GL_SwapWindow (sdlWindow);

		} else if (sdlRenderer) {

			SDL_RenderPresent (sdlRenderer);

		}

	}


	void* SDLWindow::ContextLock (bool useCFFIValue) {

		if (sdlRenderer) {

			int width;
			int height;

			#ifndef LIME_SDL2
			SDL_GetCurrentRenderOutputSize (sdlRenderer, &width, &height);
			#else
			SDL_GetRendererOutputSize (sdlRenderer, &width, &height);
			#endif

			if (width != contextWidth || height != contextHeight) {

				if (sdlTexture) {

					SDL_DestroyTexture (sdlTexture);

				}

				sdlTexture = SDL_CreateTexture (sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);

				contextWidth = width;
				contextHeight = height;

			}

			void *pixels;
			int pitch;

			if (useCFFIValue) {

				#ifndef LIME_SDL2
				if (SDL_LockTexture (sdlTexture, NULL, &pixels, &pitch)) {
				#else
				if (SDL_LockTexture (sdlTexture, NULL, &pixels, &pitch) == 0) {
				#endif

					value result = alloc_empty_object ();
					alloc_field (result, val_id ("width"), alloc_int (contextWidth));
					alloc_field (result, val_id ("height"), alloc_int (contextHeight));
					alloc_field (result, val_id ("pixels"), alloc_float ((uintptr_t)pixels));
					alloc_field (result, val_id ("pitch"), alloc_int (pitch));
					return result;

				} else {

					return alloc_null ();

				}

			} else {

				const int id_width = hl_hash_utf8 ("width");
				const int id_height = hl_hash_utf8 ("height");
				const int id_pixels = hl_hash_utf8 ("pixels");
				const int id_pitch = hl_hash_utf8 ("pitch");

				#ifndef LIME_SDL2
				if (SDL_LockTexture (sdlTexture, NULL, &pixels, &pitch)) {
				#else
				if (SDL_LockTexture (sdlTexture, NULL, &pixels, &pitch) == 0) {
				#endif

					vdynamic* result = (vdynamic*)hl_alloc_dynobj();
					hl_dyn_seti (result, id_width, &hlt_i32, contextWidth);
					hl_dyn_seti (result, id_height, &hlt_i32, contextHeight);
					hl_dyn_setd (result, id_pixels, (uintptr_t)pixels);
					hl_dyn_seti (result, id_pitch, &hlt_i32, pitch);
					return result;

				} else {

					return 0;

				}

			}

		} else {

			if (useCFFIValue) {

				return alloc_null ();

			} else {

				return 0;

			}

		}

	}


	void SDLWindow::ContextMakeCurrent () {

		if (sdlWindow && context) {

			SDL_GL_MakeCurrent (sdlWindow, context);

		}

	}


	void SDLWindow::ContextUnlock () {

		if (sdlTexture) {

			SDL_UnlockTexture (sdlTexture);
			SDL_RenderClear (sdlRenderer);
			#ifndef LIME_SDL2
			SDL_RenderTexture (sdlRenderer, sdlTexture, NULL, NULL);
			#else
			SDL_RenderCopy (sdlRenderer, sdlTexture, NULL, NULL);
			#endif

		}

	}


	void SDLWindow::Focus () {

		SDL_RaiseWindow (sdlWindow);

	}

	void* SDLWindow::GetHandle () {

		#ifdef LIME_SDL2
		return nullptr;
		#else
		SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);

		#if defined(SDL_VIDEO_DRIVER_WINDOWS)
			void* hwnd = nullptr;
			SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, &hwnd);
			return hwnd;
		#elif defined(SDL_VIDEO_DRIVER_X11)
			unsigned long x11win = 0;
			SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, &x11win);
			return (void*)(uintptr_t)x11win;
		#elif defined(SDL_VIDEO_DRIVER_WAYLAND)
			void* wlSurface = nullptr;
			SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, &wlSurface);
			return wlSurface;
		#elif defined(SDL_VIDEO_DRIVER_ANDROID)
			void* native = nullptr;
			SDL_GetPointerProperty(props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, &native);
			return native;
		#elif defined(SDL_VIDEO_DRIVER_COCOA)
			void* cocoa = nullptr;
			SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, &cocoa);
			return cocoa;
		#else
			return nullptr;
		#endif
		#endif

	}

	void* SDLWindow::GetContext () {

		return context;

	}


	const char* SDLWindow::GetContextType () {

		if (context) {

			return "opengl";

		} else if (sdlRenderer) {

			#ifdef LIME_SDL2
			SDL_RendererInfo info;
			SDL_GetRendererInfo (sdlRenderer, &info);

			printf ("[SDLWindow] renderer: %s\n", info.name);

			if (info.flags & SDL_RENDERER_SOFTWARE) {

				return "software";

			} else {

				return "opengl";

			}
			#else
			const char *name = SDL_GetRendererName (sdlRenderer);

			if (name && std::strcmp (name, SDL_SOFTWARE_RENDERER)) {

				return "software";

			} else {

				return "opengl";

			}
			#endif

		}

		return "none";

	}


	int SDLWindow::GetDisplay () {

		#ifndef LIME_SDL2
		return SDL_GetDisplayForWindow (sdlWindow);
		#else
		return SDL_GetWindowDisplayIndex (sdlWindow);
		#endif

	}


	void SDLWindow::GetDisplayMode (DisplayMode* displayMode) {

		#ifndef LIME_SDL2
		const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode (SDL_GetDisplayForWindow (sdlWindow));

		displayMode->width = mode->w;
		displayMode->height = mode->h;

		switch (mode->format) {
		#else
		SDL_DisplayMode mode;
		SDL_GetWindowDisplayMode (sdlWindow, &mode);

		displayMode->width = mode.w;
		displayMode->height = mode.h;

		switch (mode.format) {
		#endif

			case SDL_PIXELFORMAT_ARGB8888:

				displayMode->pixelFormat = ARGB32;
				break;

			case SDL_PIXELFORMAT_BGRA8888:
			case SDL_PIXELFORMAT_BGRX8888:

				displayMode->pixelFormat = BGRA32;
				break;

			default:

				displayMode->pixelFormat = RGBA32;

		}

		#ifndef LIME_SDL2
		displayMode->refreshRate = mode->refresh_rate;
		#else
		displayMode->refreshRate = mode.refresh_rate;
		#endif

	}


	int SDLWindow::GetHeight () {

		int width;
		int height;

		#ifndef LIME_SDL2
		SDL_GetWindowSizeInPixels (sdlWindow, &width, &height);

		float scale = SDL_GetWindowDisplayScale(sdlWindow);
		return (int)(height / scale);
		#else
		SDL_GetWindowSize (sdlWindow, &width, &height);

		return height;
		#endif

	}


	uint32_t SDLWindow::GetID () {

		return SDL_GetWindowID (sdlWindow);

	}


	bool SDLWindow::GetMouseLock () {

		#ifndef LIME_SDL2
		return SDL_GetWindowRelativeMouseMode (sdlWindow);
		#else
		return SDL_GetRelativeMouseMode ();
		#endif

	}


	float SDLWindow::GetOpacity () {

		#ifndef LIME_SDL2
		return SDL_GetWindowOpacity (sdlWindow);
		#else
		float opacity = 1.0f;

		SDL_GetWindowOpacity (sdlWindow, &opacity);

		return opacity;
		#endif

	}


	double SDLWindow::GetScale () {

		#ifndef LIME_SDL2
		return SDL_GetWindowDisplayScale(sdlWindow);
		#else
		if (sdlRenderer) {

			int outputWidth;
			int outputHeight;

			SDL_GetRendererOutputSize (sdlRenderer, &outputWidth, &outputHeight);

			int width;
			int height;

			SDL_GetWindowSize (sdlWindow, &width, &height);

			double scale = double (outputWidth) / width;
			return scale;

		} else if (context) {

			int outputWidth;
			int outputHeight;

			SDL_GL_GetDrawableSize (sdlWindow, &outputWidth, &outputHeight);

			int width;
			int height;

			SDL_GetWindowSize (sdlWindow, &width, &height);

			double scale = double (outputWidth) / width;
			return scale;

		}

		return 1;
		#endif

	}


	bool SDLWindow::GetTextInputEnabled () {

		#ifndef LIME_SDL2
		return SDL_TextInputActive (sdlWindow);
		#else
		return SDL_IsTextInputActive ();
		#endif

	}


	int SDLWindow::GetWidth () {

		int width;
		int height;

		#ifndef LIME_SDL2
		SDL_GetWindowSizeInPixels (sdlWindow, &width, &height);

		float scale = SDL_GetWindowDisplayScale(sdlWindow);
		return (int)(width / scale);
		#else
		SDL_GetWindowSize (sdlWindow, &width, &height);

		return width;
		#endif

	}


	int SDLWindow::GetX () {

		int x;
		int y;

		SDL_GetWindowPosition (sdlWindow, &x, &y);

		return x;

	}


	int SDLWindow::GetY () {

		int x;
		int y;

		SDL_GetWindowPosition (sdlWindow, &x, &y);

		return y;

	}


	void SDLWindow::Move (int x, int y) {

		SDL_SetWindowPosition (sdlWindow, x, y);

	}


	void SDLWindow::ReadPixels (ImageBuffer *buffer, Rectangle *rect) {

		if (sdlRenderer) {

			SDL_Rect bounds = { 0, 0, 0, 0 };

			if (rect) {

				bounds.x = rect->x;
				bounds.y = rect->y;
				bounds.w = rect->width;
				bounds.h = rect->height;

			} else {

				#ifndef LIME_SDL2
				SDL_GetWindowSizeInPixels (sdlWindow, &bounds.w, &bounds.h);
				#else
				SDL_GetWindowSize (sdlWindow, &bounds.w, &bounds.h);
				#endif

			}

			buffer->Resize (bounds.w, bounds.h, 32);

			#ifndef LIME_SDL2
			buffer->data->buffer->b = (unsigned char *)(SDL_RenderReadPixels (sdlRenderer, &bounds)->pixels);
			#else
			SDL_RenderReadPixels (sdlRenderer, &bounds, SDL_PIXELFORMAT_ABGR8888, buffer->data->buffer->b, buffer->Stride ());
			#endif

		} else if (context) {

			// TODO

		}

	}


	void SDLWindow::Resize (int width, int height) {

		SDL_SetWindowSize (sdlWindow, width, height);

	}


	void SDLWindow::SetMinimumSize (int width, int height) {

		#ifndef LIME_SDL2
		float scale = SDL_GetWindowDisplayScale(sdlWindow);
		SDL_SetWindowSize (sdlWindow, (int)(width * scale), (int)(height * scale));
		#else
		SDL_SetWindowMinimumSize (sdlWindow, width, height);
		#endif

	}


	void SDLWindow::SetMaximumSize (int width, int height) {

		SDL_SetWindowMaximumSize (sdlWindow, width, height);

	}


	bool SDLWindow::SetBorderless (bool borderless) {

		#ifndef LIME_SDL2
		SDL_SetWindowBordered (sdlWindow, !borderless);
		#else
		if (borderless) {

			SDL_SetWindowBordered (sdlWindow, SDL_FALSE);

		} else {

			SDL_SetWindowBordered (sdlWindow, SDL_TRUE);

		}
		#endif

		return borderless;

	}


	void SDLWindow::SetCursor (Cursor cursor) {

		if (cursor != currentCursor) {

			if (currentCursor == HIDDEN) {

				#ifndef LIME_SDL2
				SDL_ShowCursor ();
				#else
				SDL_ShowCursor (SDL_ENABLE);
				#endif

			}

			switch (cursor) {

				case HIDDEN:

					#ifndef LIME_SDL2
					SDL_HideCursor ();
					#else
					SDL_ShowCursor (SDL_DISABLE);
					#endif

				case CROSSHAIR:

					if (!SDLCursor::crosshairCursor) {

						SDLCursor::crosshairCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_CROSSHAIR);

					}

					SDL_SetCursor (SDLCursor::crosshairCursor);
					break;

				case MOVE:

					if (!SDLCursor::moveCursor) {

						#ifndef LIME_SDL2
						SDLCursor::moveCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_MOVE);
						#else
						SDLCursor::moveCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_SIZEALL);
						#endif

					}

					SDL_SetCursor (SDLCursor::moveCursor);
					break;

				case POINTER:

					if (!SDLCursor::pointerCursor) {

						#ifndef LIME_SDL2
						SDLCursor::pointerCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_POINTER);
						#else
						SDLCursor::pointerCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_HAND);
						#endif

					}

					SDL_SetCursor (SDLCursor::pointerCursor);
					break;

				case RESIZE_NESW:

					if (!SDLCursor::resizeNESWCursor) {

						#ifndef LIME_SDL2
						SDLCursor::resizeNESWCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_NESW_RESIZE);
						#else
						SDLCursor::resizeNESWCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_SIZENESW);
						#endif

					}

					SDL_SetCursor (SDLCursor::resizeNESWCursor);
					break;

				case RESIZE_NS:

					if (!SDLCursor::resizeNSCursor) {

						#ifndef LIME_SDL2
						SDLCursor::resizeNSCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_NS_RESIZE);
						#else
						SDLCursor::resizeNSCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_SIZENS);
						#endif

					}

					SDL_SetCursor (SDLCursor::resizeNSCursor);
					break;

				case RESIZE_NWSE:

					if (!SDLCursor::resizeNWSECursor) {

						#ifndef LIME_SDL2
						SDLCursor::resizeNWSECursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_NWSE_RESIZE);
						#else
						SDLCursor::resizeNWSECursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_SIZENWSE);
						#endif

					}

					SDL_SetCursor (SDLCursor::resizeNWSECursor);
					break;

				case RESIZE_WE:

					if (!SDLCursor::resizeWECursor) {

						#ifndef LIME_SDL2
						SDLCursor::resizeWECursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_EW_RESIZE);
						#else
						SDLCursor::resizeWECursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_SIZEWE);
						#endif

					}

					SDL_SetCursor (SDLCursor::resizeWECursor);
					break;

				case TEXT:

					if (!SDLCursor::textCursor) {

						#ifndef LIME_SDL2
						SDLCursor::textCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_TEXT);
						#else
						SDLCursor::textCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_IBEAM);
						#endif

					}

					SDL_SetCursor (SDLCursor::textCursor);
					break;

				case WAIT:

					if (!SDLCursor::waitCursor) {

						SDLCursor::waitCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_WAIT);

					}

					SDL_SetCursor (SDLCursor::waitCursor);
					break;

				case WAIT_ARROW:

					if (!SDLCursor::waitArrowCursor) {

						#ifndef LIME_SDL2
						SDLCursor::waitArrowCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_PROGRESS);
						#else
						SDLCursor::waitArrowCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_WAITARROW);
						#endif

					}

					SDL_SetCursor (SDLCursor::waitArrowCursor);
					break;

				default:

					if (!SDLCursor::arrowCursor) {

						#ifndef LIME_SDL2
						SDLCursor::arrowCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_DEFAULT);
						#else
						SDLCursor::arrowCursor = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_ARROW);
						#endif

					}

					SDL_SetCursor (SDLCursor::arrowCursor);
					break;

			}

			currentCursor = cursor;

		}

	}


	void SDLWindow::SetDisplayMode (DisplayMode* displayMode) {

		#ifndef LIME_SDL2
		SDL_PixelFormat pixelFormat;
		#else
		Uint32 pixelFormat = 0;
		#endif

		switch (displayMode->pixelFormat) {

			case ARGB32:

				pixelFormat = SDL_PIXELFORMAT_ARGB8888;
				break;

			case BGRA32:

				pixelFormat = SDL_PIXELFORMAT_BGRA8888;
				break;

			default:

				pixelFormat = SDL_PIXELFORMAT_RGBA8888;

		}

		#ifndef LIME_SDL2
		SDL_DisplayMode mode = { static_cast<SDL_DisplayID>(GetDisplay()), pixelFormat, displayMode->width, displayMode->height, (float)(SDL_GetDesktopDisplayMode(1)->pixel_density), (float)(displayMode->refreshRate), 0, 0 };

		if (SDL_SetWindowFullscreenMode (sdlWindow, &mode)) {

			if (SDL_GetWindowFlags (sdlWindow) & SDL_WINDOW_FULLSCREEN) {

				SDL_SetWindowFullscreen (sdlWindow, true);

			}

		}
		#else
		SDL_DisplayMode mode = { pixelFormat, displayMode->width, displayMode->height, displayMode->refreshRate, 0 };

		if (SDL_SetWindowDisplayMode (sdlWindow, &mode) == 0) {

			displayModeSet = true;

			if (SDL_GetWindowFlags (sdlWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP) {

				SDL_SetWindowFullscreen (sdlWindow, SDL_WINDOW_FULLSCREEN);

			}

		}
		#endif

	}


	bool SDLWindow::SetFullscreen (bool fullscreen) {

		#ifndef LIME_SDL2
		SDL_SetWindowFullscreen (sdlWindow, fullscreen);
		#else
		if (fullscreen) {

			if (displayModeSet) {

				SDL_SetWindowFullscreen (sdlWindow, SDL_WINDOW_FULLSCREEN);

			} else {

				SDL_SetWindowFullscreen (sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);

			}

		} else {

			SDL_SetWindowFullscreen (sdlWindow, 0);

		}
		#endif

		return fullscreen;

	}


	void SDLWindow::SetIcon (ImageBuffer *imageBuffer) {

		#ifndef LIME_SDL2
		SDL_PixelFormat format = SDL_GetPixelFormatForMasks (imageBuffer->bitsPerPixel, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

		SDL_Surface *surface = SDL_CreateSurfaceFrom(imageBuffer->width, imageBuffer->height, format, imageBuffer->data->buffer->b, imageBuffer->Stride ());
		#else
		SDL_Surface *surface = SDL_CreateRGBSurfaceFrom (imageBuffer->data->buffer->b, imageBuffer->width, imageBuffer->height, imageBuffer->bitsPerPixel, imageBuffer->Stride (), 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
		#endif

		if (surface) {

			SDL_SetWindowIcon (sdlWindow, surface);
			#ifndef LIME_SDL2
			SDL_DestroySurface (surface);
			#else
			SDL_FreeSurface (surface);
			#endif

		}

	}


	bool SDLWindow::SetMaximized (bool maximized) {

		if (maximized) {

			SDL_MaximizeWindow (sdlWindow);

		} else {

			SDL_RestoreWindow (sdlWindow);

		}

		return maximized;

	}


	bool SDLWindow::SetMinimized (bool minimized) {

		if (minimized) {

			SDL_MinimizeWindow (sdlWindow);

		} else {

			SDL_RestoreWindow (sdlWindow);

		}

		return minimized;

	}


	void SDLWindow::SetMouseLock (bool mouseLock) {

		if (mouseLock) {

			#ifndef LIME_SDL2
			SDL_SetWindowRelativeMouseMode (sdlWindow, true);
			#else
			SDL_SetRelativeMouseMode (SDL_TRUE);
			#endif

		} else {

			#ifndef LIME_SDL2
			SDL_SetWindowRelativeMouseMode (sdlWindow, false);
			#else
			SDL_SetRelativeMouseMode (SDL_FALSE);
			#endif

		}

	}


	void SDLWindow::SetOpacity (float opacity) {

		SDL_SetWindowOpacity (sdlWindow, opacity);

	}


	bool SDLWindow::SetResizable (bool resizable) {

		#ifndef EMSCRIPTEN

		if (resizable) {

			#ifndef LIME_SDL2
			SDL_SetWindowResizable (sdlWindow, true);
			#else
			SDL_SetWindowResizable (sdlWindow, SDL_TRUE);
			#endif

		} else {

			#ifndef LIME_SDL2
			SDL_SetWindowResizable (sdlWindow, false);
			#else
			SDL_SetWindowResizable (sdlWindow, SDL_FALSE);
			#endif

		}

		return (SDL_GetWindowFlags (sdlWindow) & SDL_WINDOW_RESIZABLE);

		#else

		return resizable;

		#endif

	}


	void SDLWindow::SetTextInputEnabled (bool enabled) {

		if (enabled) {

			#ifndef LIME_SDL2
			SDL_StartTextInput (sdlWindow);
			#else
			SDL_StartTextInput ();
			#endif

		} else {

			#ifndef LIME_SDL2
			SDL_StopTextInput (sdlWindow);
			#else
			SDL_StopTextInput ();
			#endif

		}

	}


	void SDLWindow::SetTextInputRect (Rectangle * rect) {

		SDL_Rect bounds = { 0, 0, 0, 0 };

		if (rect) {

			bounds.x = rect->x;
			bounds.y = rect->y;
			bounds.w = rect->width;
			bounds.h = rect->height;

		}

		#ifndef LIME_SDL2
		SDL_SetTextInputArea(sdlWindow, &bounds, 0);
		#else
		SDL_SetTextInputRect(&bounds);
		#endif
	}


	const char* SDLWindow::SetTitle (const char* title) {

		SDL_SetWindowTitle (sdlWindow, title);

		return title;

	}


	void SDLWindow::WarpMouse (int x, int y) {

		SDL_WarpMouseInWindow (sdlWindow, x, y);

	}


	Window* MakeWindow (Application* application, int width, int height, int flags, const char* title) {

		return new SDLWindow (application, width, height, flags, title);

	}


}
