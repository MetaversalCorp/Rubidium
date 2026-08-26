// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_CANVAS_CANVAS_NATIVE_H
#define RUBIDIUM_CANVAS_CANVAS_NATIVE_H

// Android native canvas implementation.
//
// On Android the canvas reuses the host SDL_Window from SDLActivity — no child
// window creation or reparenting. NativeWindowHandle() reads
// SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER for Filament/Vulkan. Compiled on
// Android only; CMake marks it HEADER_FILE_ONLY on other platforms.

namespace RUBIDIUM
{
   class CANVAS_NATIVE : public CANVAS
   {
   public:
      CANVAS_NATIVE (LOGGER* pLogger);
      ~CANVAS_NATIVE ();

      bool Initialize (void* pParentHandle, int nWidth, int nHeight) override;
      void SetVisible (bool bVisible)                                override;
   };
}

#endif // RUBIDIUM_CANVAS_CANVAS_NATIVE_H
