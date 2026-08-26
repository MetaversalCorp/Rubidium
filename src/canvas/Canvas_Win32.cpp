// Copyright 2026 Metaversal Corporation. All rights reserved.

// Linux-specific native canvas implementation.
//
// This translation unit is compiled on Linux only. It is listed in the MSVC
// project (msvc/Rubidium.vcxproj) as a <None> item so it shows up in the IDE
// for navigation, but is excluded from the Windows build. CMake keeps it in
// the source tree on every host and marks it HEADER_FILE_ONLY off-Linux.

using namespace RUBIDIUM;

CANVAS_NATIVE::CANVAS_NATIVE (LOGGER* pLogger) :
   CANVAS (pLogger, SDL_PROP_WINDOW_WIN32_HWND_POINTER)
{
}

CANVAS_NATIVE::~CANVAS_NATIVE ()
{
}

bool CANVAS_NATIVE::Initialize (void* pParentHandle, int nWidth, int nHeight)
{
   bool bResult = CANVAS::Initialize (pParentHandle, nWidth, nHeight);

   // RESIZABLE so SDL's Win32 backend does not clamp the window back to its
   // creation size when it is later re-docked (the preview canvas is resized
   // to fit the inspector's preview pane; a non-resizable SDL window silently
   // reverts external size changes via WM_WINDOWPOSCHANGING).
   m_pWindow = SDL_CreateWindow ("RubidiumCanvas", nWidth, nHeight, SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE);
   m_bOwnsWindow = true;
   if (m_pWindow)
   {
      if (pParentHandle)
      {
         SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pWindow);
         HWND hSdlWnd = (HWND)SDL_GetPointerProperty (nProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
         if (hSdlWnd)
         {
            LONG nStyle = GetWindowLong (hSdlWnd, GWL_STYLE);
            nStyle &= ~(WS_POPUP | WS_OVERLAPPEDWINDOW);
            nStyle |= WS_CHILD;
            LONG nExStyle = GetWindowLong (hSdlWnd, GWL_EXSTYLE);
            SetWindowLong (hSdlWnd, GWL_EXSTYLE, nExStyle | WS_EX_NOACTIVATE);

            SetWindowLong (hSdlWnd, GWL_STYLE, nStyle);
            SetParent (hSdlWnd, (HWND)pParentHandle);
            SetWindowPos (hSdlWnd, nullptr, 0, 0, nWidth, nHeight, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
         }
      }
      APPNATIVE::GetInstance ()->SDLWindow_Register (this);
      bResult = true;
   }
   else m_pLogger->Log (LOGGER::kLOGLEVEL_Error, "CANVAS", std::string ("SDL_CreateWindow failed: ") + SDL_GetError ());

   return bResult;
}

void CANVAS_NATIVE::SetVisible (bool bVisible)
{
   SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pWindow);
   HWND hSdlWnd = (HWND)SDL_GetPointerProperty (nProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
   if (hSdlWnd)
   {
      if (bVisible)
      {
         ApplyChildGeometry ();
         ShowWindow (hSdlWnd, SW_SHOWNA);
         RaiseChild ();
      }
      else
      {
         ShowWindow (hSdlWnd, SW_HIDE);
      }
   }
}

void CANVAS_NATIVE::ApplyChildGeometry ()
{
   if (m_pWindow && m_bOwnsWindow && m_nChildW > 0 && m_nChildH > 0)
   {
      // Resize through SDL first. The canvas SDL window is not resizable, so
      // SDL's Win32 backend clamps any external SetWindowPos back to its
      // internal size via WM_WINDOWPOSCHANGING -- a raw SetWindowPos to a new
      // size is silently reverted (observed: request 796x467 stayed 320x240).
      // SDL_SetWindowSize updates SDL's own size (and the HWND) so the new
      // size sticks; SetWindowPos then only needs to place the child in
      // parent-client coords (SDL_SetWindowPosition works in screen coords and
      // is wrong for a reparented WS_CHILD).
      SDL_SetWindowSize (m_pWindow, m_nChildW, m_nChildH);

      SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pWindow);
      HWND hSdlWnd = (HWND)SDL_GetPointerProperty (nProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);

      if (hSdlWnd)
         SetWindowPos (hSdlWnd, nullptr, m_nChildX, m_nChildY, m_nChildW, m_nChildH, SWP_NOZORDER | SWP_NOACTIVATE);
   }
}

void CANVAS_NATIVE::RaiseChild ()
{
   if (m_pWindow && m_bOwnsWindow)
   {
      SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pWindow);
      HWND hSdlWnd = (HWND)SDL_GetPointerProperty (nProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);

      if (hSdlWnd)
         SetWindowPos (hSdlWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
   }
}
