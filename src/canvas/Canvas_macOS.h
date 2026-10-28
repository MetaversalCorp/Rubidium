// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_CANVAS_CANVAS_NATIVE_H
#define RUBIDIUM_CANVAS_CANVAS_NATIVE_H

// macOS native canvas implementation.
//
// The Filament CAMetalLayer is a subview of the chrome window, placed below
// the chrome strip. The canvas SDL window stays hidden so its black backing
// cannot cover that present. The frame re-docks the view when the chrome
// moves or resizes. This file is compiled on macOS only; CMake marks it
// HEADER_FILE_ONLY on other platforms.

struct SDL_Window;

namespace RUBIDIUM
{
   class CANVAS_NATIVE : public CANVAS
   {
   public:
      CANVAS_NATIVE (LOGGER* pLogger);
      ~CANVAS_NATIVE ();

      bool Initialize (void* pParentHandle, int nWidth, int nHeight) override;
      void SetVisible (bool bVisible)                                override;

   protected:
      void ApplyChildGeometry () override;
      void RaiseChild ()         override;

   private:
      SDL_MetalView m_pMetalView;
      SDL_Window*   m_pParentWindow;   // chrome window; canvas docks relative to it
   };
}

#endif // RUBIDIUM_CANVAS_CANVAS_NATIVE_H
