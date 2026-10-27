// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_CANVAS_CANVAS_NATIVE_H
#define RUBIDIUM_CANVAS_CANVAS_NATIVE_H

// macOS native canvas implementation.
//
// The canvas owns its own borderless SDL Metal window, docked in screen space
// below the chrome strip. It is not a Cocoa child window: Metal's present does
// not show up through addChildWindow. Canvas_OrderAbove keeps it above the
// chrome without activating it, and the frame re-docks it when the chrome
// finishes a move. Creates an SDL Metal view to provide the CAMetalLayer that
// Filament's Metal backend requires. This file is compiled on macOS only;
// CMake marks it HEADER_FILE_ONLY on other platforms.

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
