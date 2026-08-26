// Adapted from RmlUi's Backends/RmlUi_Platform_SDL.h
// Original: https://github.com/mikke89/RmlUi (MIT License)
// Only the RmlSDL namespace (input/key conversion) is used by Rubidium.
// SystemInterface_SDL is included but NOT installed as the global system
// interface -- Sneeze owns that.

#pragma once

#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/Types.h>

#ifndef RMLUI_SDL_VERSION_MAJOR
   #define RMLUI_SDL_VERSION_MAJOR 3
#endif

#if RMLUI_SDL_VERSION_MAJOR == 3
	#include <SDL3/SDL.h>
#elif RMLUI_SDL_VERSION_MAJOR == 2
	#include <SDL.h>
#else
	#error "Unspecified RMLUI_SDL_VERSION_MAJOR."
#endif

class SystemInterface_SDL : public Rml::SystemInterface {
public:
	SystemInterface_SDL();
	~SystemInterface_SDL();

	void SetWindow(SDL_Window* window);

	double GetElapsedTime() override;

	void SetMouseCursor(const Rml::String& cursor_name) override;

	void SetClipboardText(const Rml::String& text) override;
	void GetClipboardText(Rml::String& text) override;

	void ActivateKeyboard(Rml::Vector2f caret_position, float line_height) override;
	void DeactivateKeyboard() override;

private:
	SDL_Window* window = nullptr;

	SDL_Cursor* cursor_default = nullptr;
	SDL_Cursor* cursor_move = nullptr;
	SDL_Cursor* cursor_pointer = nullptr;
	SDL_Cursor* cursor_resize = nullptr;
	SDL_Cursor* cursor_cross = nullptr;
	SDL_Cursor* cursor_text = nullptr;
	SDL_Cursor* cursor_unavailable = nullptr;
};

namespace RmlSDL {

bool InputEventHandler(Rml::Context* context, SDL_Window* window, SDL_Event& ev);

Rml::Input::KeyIdentifier ConvertKey(int sdl_key);

int ConvertMouseButton(int sdl_mouse_button);

int GetKeyModifierState();

} // namespace RmlSDL

class LOGGER;

namespace RUBIDIUM
{
   // Replaces Sneeze's clipboard-less RmlUi system stub after SDL_Init so text
   // fields (URL bar, inspector filters) support Ctrl+C / Ctrl+V.
   void RubidiumRmlSystem_Install (LOGGER* pLogger);
}
