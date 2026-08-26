// Copyright 2026 Metaversal Corporation. All rights reserved.

// Apple (macOS + iOS) native canvas implementation.
//
// macOS: the canvas owns its own borderless SDL Metal window, attached as a
// Cocoa child of the chrome window via SDL_SetWindowParent and docked below the
// chrome strip. This mirrors the Linux X11 reparent path (Canvas_Linux.cpp)
// and the Win32 WS_CHILD path -- a two-window browser frame where the chrome
// (RmlUi tab strip + URL bar) sits on top and the Filament canvas fills the
// content region. Both the chrome and this canvas are non-HiDPI SDL windows,
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
// Defined in Canvas_Metal.mm (Objective-C++). Pins the CAMetalLayer's colour
// space to sRGB so Filament's Metal present matches the Windows Vulkan path
// instead of rendering dark. See Canvas_Metal.mm for the full rationale.
namespace RUBIDIUM
{
   void Canvas_ConfigureMetalLayer (void* pLayer);
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
#if defined(RUBIDIUM_PLATFORM_MACOS)
      // Attach as a Cocoa child of the chrome window: stays ordered above the
      // chrome and moves with it. SDL_SetWindowParent maps to [parent
      // addChildWindow:child ...] on macOS -- the analogue of XReparentWindow.
      if (m_pParentWindow)
      {
         if (!SDL_SetWindowParent (m_pWindow, m_pParentWindow))
            m_pLogger->Log (LOGGER::kLOGLEVEL_Warning, "CANVAS", std::string ("SDL_SetWindowParent failed: ") + SDL_GetError ());
      }
#endif

      m_pMetalView = SDL_Metal_CreateView (m_pWindow);
      if (m_pMetalView)
      {
         void* pLayer = SDL_Metal_GetLayer (m_pMetalView);
         if (pLayer)
         {
            SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pWindow);
            SDL_SetPointerProperty (nProps, kMetalLayerProperty, pLayer);
#if defined(RUBIDIUM_PLATFORM_MACOS)
            Canvas_ConfigureMetalLayer (pLayer);
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
         if (m_bOwnsWindow && m_pParentWindow)
            SDL_SetWindowParent (m_pWindow, m_pParentWindow);

         SDL_ShowWindow (m_pWindow);
         ApplyChildGeometry ();
         Canvas_SetNativeVisible (m_pWindow, true);
         SDL_RaiseWindow (m_pWindow);
      }
      else
      {
         // orderOut is required for reparented Metal children — SDL_HideWindow
         // alone can leave the child visible when the parent hides. Keep the
         // Cocoa parent link so the Metal layer survives the next show (detaching
         // with SDL_SetWindowParent(nullptr) broke GLB preview on re-open).
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
   }
}

void CANVAS_NATIVE::RaiseChild ()
{
   if (m_pWindow && m_bOwnsWindow)
      SDL_RaiseWindow (m_pWindow);
}
