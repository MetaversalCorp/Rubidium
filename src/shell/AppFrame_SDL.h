// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// APPFRAME_SDL -- the SDL-backed browser frame and tab manager (macOS, Linux,
// iOS, Android). Mirrors the Win32 APPFRAME_NATIVE: it owns an array of
// per-tab objects (APPFRAMETAB_SDL) and switches the active tab.
//
// On Linux it also owns a CHROME_RML window (tab strip + URL bar) and the
// per-tab Filament canvases are reparented as X11 children of the chrome
// window. On macOS / iOS / Android there is no chrome window: a single tab is
// created whose canvas reuses the host SDL window directly.
//
// Windows uses its own direct-Win32 APPFRAME_NATIVE class that does not
// derive from APPFRAME_SDL.

#include "shell/ChromeRml.h"
#include "shell/AppFrameTab_SDL.h"

#include <SDL3/SDL_events.h>

#ifndef RUBIDIUM_SHELL_APPFRAME_SDL_H
#define RUBIDIUM_SHELL_APPFRAME_SDL_H

struct SDL_Window;

namespace RUBIDIUM {

#define TAB_MAX_COUNT 24

class APPFRAME_SDL : public APPFRAME, public ICHROME_HOST
{
public:
   APPFRAME_SDL (IAPPWINDOW* pController, SNEEZE::ENGINE* pSneeze, LOGGER* pLogger);
   ~APPFRAME_SDL () override;

   bool Initialize (int nWidth, int nHeight, const char* sTitle, SNEEZE::CONTEXT::eSESSION eSession = SNEEZE::CONTEXT::kSESSION_PERSISTENT);
   void Shutdown ();

   void ProcessInput ()   override;
   void ToggleInspector () override;
   void Reload (bool bReset);

   // Track viewport movement keys from SDL key events routed to any window in
   // this frame (chrome, canvas, inspector). The canvas child rarely receives
   // keyboard focus, so polling SDL_GetKeyboardState there is unreliable.
   void ApplyMovementKeyEvent (SDL_Event& Event);

   // True while a text field in this frame should capture WASD movement keys.
   bool MovementKeysSuppressed () const;

   void* NativeWindow () const override;
   void* Init (APPFRAME* pAppFrame_From, SNEEZE::CONTEXT::eSESSION eSession = SNEEZE::CONTEXT::kSESSION_PERSISTENT) override;

   // ICHROME_HOST
   void Chrome_OnTabSelect (int nTabIx)              override;
   void Chrome_OnTabClose  (int nTabIx)              override;
   void Chrome_OnTabAdd    ()                        override;
   void Chrome_OnUrlSubmit (const std::string& sUrl) override;
   void Chrome_OnExit      ()                        override;
   void Chrome_OnNewWindow (SNEEZE::CONTEXT::eSESSION eSession) override;
   void Chrome_OnToggleInspector ()                  override;
   void Chrome_OnSetLogLevel (int nLevel)            override;
   int  Chrome_LogLevel () const                     override;
   void Chrome_OnSettings ()                         override;
   void Chrome_OnReleaseNotes ()                     override;
   void Chrome_OnUpdate ()                           override;
   std::vector<std::string> Chrome_UrlHistory () const override;


   // True if nWindowID is this frame's chrome window or its active tab's
   // canvas window -- used to scope global hotkeys (F12) to the focused frame.
   bool OwnsWindowID (SDL_WindowID nWindowID) const;

   // Android AR passthrough: transparent backdrop so the camera feed shows
   // through. No-op on platforms that do not host a video underlay.
   void Passthrough (bool bPassthrough);

   // Host-tracked camera orientation (Sneeze CAMERA quaternion x,y,z,w).
   // Phone IMU today; OpenXR views later. Position is left at the last
   // VIEWPORT::Camera() (fabric spawn until VPS writes translation).
   void TrackingRotation (double dQx, double dQy, double dQz, double dQw);

   // Vertical FOV in radians for the tracked camera (phone passthrough,
   // later OpenXR views). 0 restores the compositor default.
   void TrackingFovY (double dFovY);

#if defined(RUBIDIUM_PLATFORM_LINUX) || defined(RUBIDIUM_PLATFORM_MACOS)
   void DismissChromeMenuIfClickOutside (SDL_Event& Event);
#endif

protected:
   void FrameTabAdd    ();
   void FrameTabClose  (int nTabIx);
   void FrameTabActive (int nTabIx, bool bHideOld);
   void FrameTabsRefresh ();

   void* ParentNativeHandle () const;
   int   ContentTop () const;
   bool  HostWindowSize (int& nWidth, int& nHeight) const;

   SNEEZE::ENGINE*           m_pSneeze;

   CHROME_RML*               m_pChrome;
   SDL_Window*               m_pWindow;

   APPFRAMETAB_SDL*          m_apTab[TAB_MAX_COUNT];
   int                       m_nTabCount;
   int                       m_nTabIx_Active;

   SNEEZE::CONTEXT::eSESSION m_eSession;
   bool                      m_bOpen;

   // Previous frame's canvas mouse-button state, for the rising-edge test that
   // releases address-bar focus on the first click into the 3D view.
   bool                      m_bCanvasMouseDown;
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_SHELL_APPFRAME_SDL_H
