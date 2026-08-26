// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_CANVAS_CANVAS_NATIVE_H
#define RUBIDIUM_CANVAS_CANVAS_NATIVE_H

// macOS native canvas implementation.
//
// Mirrors the Linux two-window model: the canvas owns its own borderless SDL
// Metal window which is attached as a Cocoa child of the chrome window (via
// SDL_SetWindowParent) and docked in the region below the chrome strip. Cocoa
// child windows move with their parent and stay ordered above it, so the
// chrome (tab strip + URL bar) and the Filament canvas read as one browser
// window -- the macOS analogue of the X11 XReparentWindow path. Creates an SDL
// Metal view to provide the CAMetalLayer that Filament's Metal backend
// requires. This file is compiled on macOS only; CMake marks it
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
