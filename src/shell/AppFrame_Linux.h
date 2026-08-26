// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// Linux APPFRAME — plain SDL3 shell. No overrides today; lives as a
// named subclass so X11 / Wayland-specific hooks have a natural home
// when we need them (clipboard integration, IME config, etc.).

#ifndef RUBIDIUM_SHELL_APPFRAME_NATIVE_H
#define RUBIDIUM_SHELL_APPFRAME_NATIVE_H

#include "AppFrame_SDL.h"

namespace RUBIDIUM {

class APPFRAME_NATIVE : public APPFRAME_SDL
{
public:
   using APPFRAME_SDL::APPFRAME_SDL;
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_SHELL_APPFRAME_NATIVE_H
