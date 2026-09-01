// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_RMLUI_SDL_H
#define RUBIDIUM_RMLUI_SDL_H

#include "shell/App.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Rml { class ElementDocument; }

namespace RUBIDIUM
{

typedef void (*FN_WINDOW_GEOMETRY)(int nX, int nY, int nWidth, int nHeight, bool bMaximized, void* pUserData);
typedef void (*FN_WINDOW_VISIBILITY)(bool bVisible, void* pUserData);

class RMLUI_SDL : public ISDLWINDOW
{
public:
   RMLUI_SDL ();
   ~RMLUI_SDL ();

   bool Initialize (const char* sTitle, int nWidth, int nHeight);
   bool LoadDocument (const std::string& sRmlDocument);

   Rml::ElementDocument* Document ();

   bool CaptureRgba (std::vector<uint8_t>& aRgba, int& nWidth, int& nHeight);
   void RenderOffscreen ();
   void Render ();
   void Toggle ();

   // Show + de-minimize + raise the window (idempotent -- unlike Toggle it never
   // hides). Used for modal child windows (Settings) that must always come to the
   // front when re-invoked rather than toggling closed.
   void Show ();

   // Focus-aware toggle: bring the window to the front (show + raise + focus)
   // unless it is already the visible, focused window, in which case hide it.
   void ToggleFront ();

   // True while the window is on-screen (shown). A minimized window is still
   // visible in this sense.
   bool IsVisible () const;

   // True while the window is "open" -- shown OR minimized, i.e. not hidden. Only
   // a real close (the window's close button) hides it. Use this (not IsVisible)
   // to keep a modal owner blocked while its Settings child is merely minimized.
   bool IsOpen () const;

   void SetPosition (int nX, int nY);
   void SetGeometryCallback (FN_WINDOW_GEOMETRY fnCallback, void* pUserData);
   void SetVisibilityCallback (FN_WINDOW_VISIBILITY fnCallback, void* pUserData);

   // Make this window modal to a parent (SDL) window. pParent is an SDL_Window*
   // (passed as void* so this header stays SDL-free). Establishes the SDL
   // parent/child relationship now (the window must be hidden) and toggles
   // SDL_WINDOW_MODAL on show / off before hide, giving true windowing-system
   // modality on backends that honour it (X11, Win32, macOS) and a transient
   // "stays above parent" relationship elsewhere (Wayland). Passing nullptr
   // clears the relationship. Not used on Windows, where the Win32 shell owns
   // its native chrome and disables the owner via EnableWindow instead.
   void SetModalParent (void* pParent);

   // Native handle a CANVAS_NATIVE reparents under to embed a child render
   // surface (the GLB preview viewport) inside this window: an HWND on Windows,
   // the X11 window XID (as void*) on Linux, the SDL_Window* on macOS -- exactly
   // matching APPFRAME_SDL::ParentNativeHandle so the same CANVAS_NATIVE code
   // path works. Returns null before Initialize.
   void* ParentNativeHandle () const;

   // ISDLWINDOW
   SDL_WindowID SDLWindowID () const override;
   void         HandleEvent (SDL_Event& Event) override;

private:
   class Impl;
   Impl* m_pImpl;
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_RMLUI_SDL_H
