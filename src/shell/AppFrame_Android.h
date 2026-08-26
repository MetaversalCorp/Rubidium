// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// Android APPFRAME — plain SDL3 shell over SDLActivity. Placeholder
// subclass for future Android-specific hooks (JNI callbacks into the
// MainActivity, keyboard visibility, insets, etc.).

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
