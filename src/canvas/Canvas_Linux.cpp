// Copyright 2026 Metaversal Corporation. All rights reserved.

// Linux-specific native canvas implementation.
//
// This translation unit is compiled on Linux only. It is listed in the MSVC
// project (msvc/Rubidium.vcxproj) as a <None> item so it shows up in the IDE
// for navigation, but is excluded from the Windows build. CMake keeps it in
// the source tree on every host and marks it HEADER_FILE_ONLY off-Linux.

using namespace RUBIDIUM;

#include <X11/Xlib.h>
// Xlib defines a pile of bare macros (None, Bool, Status, ...) that collide
// with SDL / RmlUi / Sneeze identifiers. The engine headers are already parsed
// (force-included via the precompiled header) before this point, so undefining
// the offenders here only protects this translation unit's own code below.
#undef None
#undef Bool
#undef Status
#undef Success
#undef Always
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef CursorShape

CANVAS_NATIVE::CANVAS_NATIVE (LOGGER* pLogger) :
   CANVAS (pLogger, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER)
{
}

CANVAS_NATIVE::~CANVAS_NATIVE ()
{
}

bool CANVAS_NATIVE::Initialize (void* pParentHandle, int nWidth, int nHeight)
{
   bool bResult = CANVAS::Initialize (pParentHandle, nWidth, nHeight);

   // Create an own borderless window (hidden until activated) and reparent it
   // as an X11 child of the chrome frame -- the X11 analogue of the Win32
   // SetParent path above. pParentHandle is the parent's X11 Window (XID).
   m_pWindow = SDL_CreateWindow ("RubidiumCanvas", nWidth, nHeight, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
   m_bOwnsWindow = true;
   if (m_pWindow)
   {
      if (pParentHandle)
      {
         SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pWindow);
         Display* pDisplay = static_cast<Display*> (SDL_GetPointerProperty (nProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
         Window   xChild = static_cast<Window> (SDL_GetNumberProperty (nProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
         Window   xParent = static_cast<Window> (reinterpret_cast<uintptr_t> (pParentHandle));

         if (pDisplay && xChild && xParent)
         {
            XReparentWindow (pDisplay, xChild, xParent, 0, 0);
            XFlush (pDisplay);
         }
         else m_pLogger->Log (LOGGER::kLOGLEVEL_Warning, "CANVAS", "X11 reparent unavailable (Wayland session?) -- tab canvas will not be embedded");
      }
      APPNATIVE::GetInstance ()->SDLWindow_Register (this);
      bResult = true;
   }
   else m_pLogger->Log (LOGGER::kLOGLEVEL_Error, "CANVAS", std::string ("SDL_CreateWindow failed: ") + SDL_GetError ());

   return bResult;
}

void CANVAS_NATIVE::SetVisible (bool bVisible)
{
   if (m_pWindow)
   {
      if (bVisible)
      {
         SDL_ShowWindow (m_pWindow);

         SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pWindow);
         Display* pDisplay = static_cast<Display*> (SDL_GetPointerProperty (nProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
         Window   xWindow = static_cast<Window> (SDL_GetNumberProperty (nProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
         if (pDisplay && xWindow)
         {
            XRaiseWindow (pDisplay, xWindow);
            XFlush (pDisplay);
         }
      }
      else
      {
         SDL_HideWindow (m_pWindow);
      }
   }
}

void CANVAS_NATIVE::ApplyChildGeometry ()
{
   if (m_pWindow && m_bOwnsWindow && m_nChildW > 0 && m_nChildH > 0)
   {
      // Keep SDL's cached geometry in sync (it centers new windows at creation
      // and re-applies that stale, now parent-relative position on show / state
      // changes), then assert the real geometry directly through X11. SDL's own
      // child-window positioning is unreliable across fullscreen / minimize /
      // restore transitions, so XMoveResizeWindow is the authoritative placement.
      SDL_SetWindowPosition (m_pWindow, m_nChildX, m_nChildY);
      SDL_SetWindowSize (m_pWindow, m_nChildW, m_nChildH);

      SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pWindow);
      Display* pDisplay = static_cast<Display*> (SDL_GetPointerProperty (nProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
      Window   xWindow = static_cast<Window> (SDL_GetNumberProperty (nProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
      if (pDisplay && xWindow)
      {
         XMoveResizeWindow (pDisplay, xWindow, m_nChildX, m_nChildY, static_cast<unsigned int> (m_nChildW), static_cast<unsigned int> (m_nChildH));
         XFlush (pDisplay);
      }
   }
}

void CANVAS_NATIVE::RaiseChild ()
{
   if (m_pWindow && m_bOwnsWindow)
   {
      SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pWindow);
      Display* pDisplay = static_cast<Display*> (SDL_GetPointerProperty (nProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
      Window   xWindow = static_cast<Window> (SDL_GetNumberProperty (nProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
      if (pDisplay && xWindow)
      {
         XRaiseWindow (pDisplay, xWindow);
         XFlush (pDisplay);
      }
   }
}

