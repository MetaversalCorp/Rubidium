// Copyright 2026 Metaversal Corporation. All rights reserved.

// Apple (macOS + iOS) native canvas implementation.
//
// macOS: Filament's CAMetalLayer is a subview of the chrome window, in the
// region below the chrome strip. The separate canvas SDL window stays hidden;
// its backing is black and was covering the present. Both windows are
// non-HiDPI, so window points equal pixels.
// so window points equal pixels and the docking geometry stays in one unit.
// pParentHandle is the chrome's SDL_Window* (APPFRAME_SDL::ParentNativeHandle).
//
// iOS: a single fullscreen window with no chrome; the canvas reuses the host
// SDL_Window directly (pParentHandle is that window).
//
// Either way the class creates an SDL Metal view to expose the CAMetalLayer
// that Filament's Metal backend requires. Compiled on Apple platforms only.

#include <SDL3/SDL_metal.h>

using namespace RUBIDIUM;

static const char* kMetalLayerProperty = "rubidium.metal.layer";

#if defined(RUBIDIUM_PLATFORM_MACOS)
// Defined in Canvas_Metal.mm (Objective-C++). Moves the SDL Metal view onto
// the chrome window and sizes its drawable. See Canvas_Metal.mm.
namespace RUBIDIUM
{
   void Canvas_InstallMetalView (SDL_Window* pWindow, SDL_Window* pParent, void* pMetalView, int nX, int nY, int nWidth, int nHeight);
   void Canvas_OrderAbove (SDL_Window* pWindow, SDL_Window* pParent);
   void Canvas_SetNativeVisible (SDL_Window* pWindow, bool bVisible);
}
#endif

CANVAS_NATIVE::CANVAS_NATIVE (LOGGER* pLogger) :
   CANVAS (pLogger, kMetalLayerProperty),
   m_pMetalView (nullptr),
   m_pParentWindow (nullptr)
{
}

CANVAS_NATIVE::~CANVAS_NATIVE ()
{
   if (m_pMetalView)
   {
      SDL_Metal_DestroyView (m_pMetalView);
      m_pMetalView = nullptr;
   }
}

bool CANVAS_NATIVE::Initialize (void* pParentHandle, int nWidth, int nHeight)
{
   bool bResult = CANVAS::Initialize (pParentHandle, nWidth, nHeight);

   m_pParentWindow = static_cast<SDL_Window*> (pParentHandle);

#if defined(RUBIDIUM_PLATFORM_MACOS)
   // Own borderless Metal window (hidden until activated). Deliberately NOT
   // high-pixel-density: its drawable pixels equal its point size so the dock
   // geometry (window points) and the Filament framebuffer share one unit.
   m_pWindow = SDL_CreateWindow ("RubidiumCanvas", nWidth, nHeight, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_METAL);
   m_bOwnsWindow = true;
#else
   // iOS: reuse the host (single fullscreen) window directly.
   m_pWindow = m_pParentWindow;
   m_bOwnsWindow = false;
#endif

   if (m_pWindow)
   {
      m_pMetalView = SDL_Metal_CreateView (m_pWindow);
      if (m_pMetalView)
      {
         void* pLayer = SDL_Metal_GetLayer (m_pMetalView);
         if (pLayer)
         {
            SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pWindow);
            SDL_SetPointerProperty (nProps, kMetalLayerProperty, pLayer);
#if defined(RUBIDIUM_PLATFORM_MACOS)
            Canvas_InstallMetalView (m_pWindow, m_pParentWindow, m_pMetalView, 0, 0, nWidth, nHeight);
#endif
         }
         else m_pLogger->Log (LOGGER::kLOGLEVEL_Warning, "CANVAS", "SDL_Metal_GetLayer returned null");
      }
      else m_pLogger->Log (LOGGER::kLOGLEVEL_Warning, "CANVAS", std::string ("SDL_Metal_CreateView failed: ") + SDL_GetError ());

      APPNATIVE::GetInstance ()->SDLWindow_Register (this);
      bResult = true;
   }
   else m_pLogger->Log (LOGGER::kLOGLEVEL_Error, "CANVAS", std::string ("Canvas window unavailable: ") + SDL_GetError ());

   return bResult;
}

void CANVAS_NATIVE::SetVisible (bool bVisible)
{
   if (m_pWindow)
   {
      if (bVisible)
      {
#if defined(RUBIDIUM_PLATFORM_MACOS)
         // The layer lives in the chrome window. Showing this window paints
         // a black rectangle over that present.
         ApplyChildGeometry ();
         Canvas_SetNativeVisible (m_pWindow, false);
         SDL_HideWindow (m_pWindow);
#else
         SDL_ShowWindow (m_pWindow);
         ApplyChildGeometry ();
         Canvas_SetNativeVisible (m_pWindow, true);
         SDL_RaiseWindow (m_pWindow);
#endif
      }
      else
      {
         // orderOut is required — SDL_HideWindow alone can leave the borderless
         // canvas visible after the chrome hides.
         Canvas_SetNativeVisible (m_pWindow, false);
         SDL_HideWindow (m_pWindow);
      }
   }
}

void CANVAS_NATIVE::ApplyChildGeometry ()
{
   // Guarded by m_bOwnsWindow so this is a no-op on iOS (host-window reuse).
   if (m_pWindow && m_bOwnsWindow && m_pParentWindow && m_nChildW > 0 && m_nChildH > 0)
   {
      // Position in screen space relative to the chrome window's top-left (SDL
      // uses a top-left origin and handles the Cocoa bottom-left conversion).
      // Re-asserted on every dock (resize / restore) so the child can't drift
      // after a fullscreen or minimize/restore transition.
      int nParentX = 0, nParentY = 0;
      SDL_GetWindowPosition (m_pParentWindow, &nParentX, &nParentY);

      SDL_SetWindowPosition (m_pWindow, nParentX + m_nChildX, nParentY + m_nChildY);
      SDL_SetWindowSize (m_pWindow, m_nChildW, m_nChildH);

#if defined(RUBIDIUM_PLATFORM_MACOS)
      // Filament reads rubidium.metal.layer when it creates the swapchain,
      // which happens after the first Show.
      Canvas_InstallMetalView (m_pWindow, m_pParentWindow, m_pMetalView, m_nChildX, m_nChildY, m_nChildW, m_nChildH);
#endif
   }
}

void CANVAS_NATIVE::RaiseChild ()
{
   if (m_pWindow && m_bOwnsWindow)
      SDL_RaiseWindow (m_pWindow);
}
