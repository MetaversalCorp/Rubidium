// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// CHROME_RML -- the macOS / Linux browser chrome window. A top-level SDL window
// rendered with RmlUi (via SDL_Renderer, like RMLUI_SDL / the inspector) that
// draws the tab strip + URL bar as the top-most band, matching native Chrome.
// Window controls follow each platform's convention:
//   macOS: a real titled window whose title bar is made transparent +
//          full-size-content (shell/MacChrome.mm) so the native red/yellow/
//          green traffic-light buttons stay at the top-left while the strip
//          draws across the top; the native frame handles resize and dragging
//          over the empty strip comes from an SDL hit test.
//   Linux: a borderless window with Chrome-style min/max/close buttons drawn
//          into the strip on the right, plus SDL-hit-test resize edges.
// The per-tab Filament canvases are reparented as children of this window (X11
// reparent on Linux, SDL_SetWindowParent on macOS), occupying the region below
// ChromeHeight().
//
// Because Filament owns the Vulkan swapchain of the canvas windows, the chrome
// cannot be composited over the canvas on a single surface -- hence the
// two-window model that mirrors the Win32 native frame + child canvas design.

#ifndef RUBIDIUM_SHELL_CHROMERML_H
#define RUBIDIUM_SHELL_CHROMERML_H

namespace RUBIDIUM
{
   // Callback target for chrome interactions, implemented by the tab manager
   // (APPFRAME_SDL). Mirrors the mouse-driven tab actions on the Win32 frame.
   class ICHROME_HOST
   {
   public:
      virtual ~ICHROME_HOST () = default;

      virtual void Chrome_OnTabSelect (int nTabIx)             = 0;
      virtual void Chrome_OnTabClose  (int nTabIx)             = 0;
      virtual void Chrome_OnTabAdd    ()                       = 0;
      virtual void Chrome_OnUrlSubmit (const std::string& sUrl) = 0;
      virtual void Chrome_OnExit      ()                       = 0;

      // Persisted address-bar history (most-recent first) for the URL dropdown.
      virtual std::vector<std::string> Chrome_UrlHistory () const = 0;

      // Ellipsis menu actions (mirrors the Win32 popup menu).
      virtual void Chrome_OnNewWindow      (SNEEZE::CONTEXT::eSESSION eSession) = 0;
      virtual void Chrome_OnToggleInspector ()                  = 0;
      virtual void Chrome_OnSetLogLevel    (int nLevel)         = 0;
      virtual void Chrome_OnSettings       ()                   = 0;
      virtual void Chrome_OnReleaseNotes   ()                   = 0;
      virtual void Chrome_OnUpdate         ()                   = 0;
      // Current log level (0..N) so the menu can mark the active radio item.
      virtual int  Chrome_LogLevel () const                     = 0;
   };

   class CHROME_RML : public ISDLWINDOW
   {
   public:
      CHROME_RML ();
      ~CHROME_RML ();

      bool Initialize (const char* sTitle, int nWidth, int nHeight, ICHROME_HOST* pHost);

      // Rebuild the tab strip from the manager's tab list and highlight nActive.
      void SetTabs (const std::vector<std::string>& asTitle, int nActive);

      // Reflect the active tab's URL into the address bar.
      void SetUrl (const std::string& sUrl);

      // True while the address bar owns RmlUi keyboard focus.
      bool IsUrlBarFocused () const;

      // Drop keyboard focus from the address bar and end SDL text input. The
      // canvas child never steals focus from the chrome window on its own, so
      // clicking the 3D view has to say so explicitly -- otherwise the address
      // bar keeps capturing keys and the viewport movement keys stay dead.
      // Mirrors the Win32 frame's WM_PARENTNOTIFY focus hand-back.
      void BlurTextEntry ();

      void Render ();

      // While blocked (a modal child window such as Settings is open) the chrome
      // ignores all clicks, mirroring the Win32 EnableWindow(owner, FALSE) modal
      // behaviour so the window cannot be closed or otherwise driven underneath.
      void SetModalBlocked (bool bBlocked);

      // Height of the chrome strip in window pixels -- the canvas dock offset.
      // On macOS in a fullscreen Space this includes the menu-bar push-down
      // inset (see TickMacChrome), so the docked canvas tracks the chrome.
      int ChromeHeight () const;

#if defined(RUBIDIUM_PLATFORM_MACOS)
      // Per-frame macOS fullscreen maintenance (menu-bar reveal push-down +
      // title-bar overlay suppression). Driven from APPFRAME_SDL::ProcessInput.
      // Returns true when the content-top inset changed this frame so the caller
      // re-docks the canvas at the new ChromeHeight ().
      bool TickMacChrome ();
#endif

      bool MenuOpen () const;
      void DismissMenu ();

      SDL_Window* MenuWindow () const;

      // URL history dropdown (the address-bar caret popup).
      bool        UrlDropOpen () const;
      void        DismissUrlDrop ();
      SDL_Window* UrlDropWindow () const;

      SDL_Window* Window () const;

      // X11 Window (XID, cast to void*) of the chrome window -- the reparent
      // target passed to each tab's CANVAS::Initialize.
      void* NativeParentHandle () const;

      // ISDLWINDOW
      SDL_WindowID SDLWindowID () const override;
      void         HandleEvent (SDL_Event& Event) override;

   private:
      class Impl;
      Impl* m_pImpl;
   };
}

#endif // RUBIDIUM_SHELL_CHROMERML_H
