// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// macOS-only Cocoa shim for the browser chrome window. SDL3 exposes no API to
// give an NSWindow a transparent, full-size-content title bar -- the technique
// Chrome and Safari use to draw their tab strip into the title-bar band. This
// shim reaches the underlying NSWindow through SDL's Cocoa window property
// (SDL3 returns an SDL3Window wrapper -- see MacChrome.mm) and applies those
// applies those attributes, and hides the native traffic-light buttons so
// Rubidium can draw its own red/yellow/green controls into the RmlUi tab strip
// (the native buttons cannot stay overlaid on the content in fullscreen
// without macOS reserving an opaque title-bar band). Compiled only on macOS
// desktop (see src/CMakeLists.txt); it is the single intentional Objective-C++
// translation unit in Rubidium.

#ifndef RUBIDIUM_SHELL_MACCHROME_H
#define RUBIDIUM_SHELL_MACCHROME_H

struct SDL_Window;

namespace RUBIDIUM
{
   // Flip the chrome SDL window's NSWindow to a transparent, hidden-title,
   // full-size-content title bar so the RmlUi tab strip draws across the top
   // band, and hide the native traffic-light buttons (Rubidium draws its own in
   // the strip). Re-applied on each fullscreen transition because AppKit
   // restores the buttons and drops the title-bar attributes.
   void MacChrome_ConfigureTitleBar (SDL_Window* pWindow);

   // Toggle the chrome window's native fullscreen Space (the action behind the
   // custom green button). Drives AppKit's -toggleFullScreen: so the standard
   // fullscreen animation runs; SDL reports the result via the
   // ENTER / LEAVE_FULLSCREEN window events.
   void MacChrome_ToggleFullScreen (SDL_Window* pWindow);

   // Install (once, process-wide) observers for AppKit's fullscreen-transition
   // notifications so MacChrome_IsFullScreenTransitioning can report when any
   // window is mid-animation. Idempotent; safe to call from every auxiliary
   // window's initialization. No-op off macOS desktop.
   void MacChrome_TrackFullScreenTransitions ();

   // True while a native fullscreen enter/exit animation is in flight for ANY
   // window. The RmlUi/SDL_Renderer auxiliary windows (inspector, settings,
   // release notes) must NOT present to their Metal drawable during this window
   // -- SDL's live-resize event watch fires from inside the transition and a
   // present while the drawable is being recreated crashes. Skip GPU work while
   // this is true; a fresh frame is drawn on the ENTER/LEAVE_FULLSCREEN event
   // once the animation completes.
   bool MacChrome_IsFullScreenTransitioning ();

   // In a native fullscreen Space, a menu-bar reveal also floats AppKit's
   // title-bar overlay window (NSToolbarFullScreenWindow) above the content --
   // an empty, transparent-titled band that still paints a light strip below
   // the menu bar. Chrome shows no such band. Make that overlay window fully
   // transparent so only the menu bar reveals over the RmlUi chrome. Cheap
   // (no style-mask churn); safe to call on every mouse-motion event while in
   // fullscreen. No-op when the overlay does not exist (windowed, or before the
   // first reveal).
   void MacChrome_SuppressFullScreenTitleBar (SDL_Window* pWindow);

   // Height (in window points) of the system menu bar. This is the vertical
   // distance the RmlUi chrome is pushed down while the menu bar is revealed in
   // a fullscreen Space, so the tab strip is not covered -- matching how Chrome
   // slides its toolbar down below the revealed menu bar.
   int MacChrome_MenuBarHeight ();

   // Amount (in window points) the top of the window content is currently
   // occluded by system UI -- the menu bar / title bar band revealed in a
   // fullscreen Space. Returns 0 when windowed, or in fullscreen while the menu
   // bar is hidden. Polled every frame (the system menu bar swallows mouse
   // events while the cursor is over it, so a motion-based trigger never fires);
   // the RmlUi chrome is pushed down by this value so the tab strip and traffic
   // lights slide below the revealed menu bar, Chrome-style, with no band left
   // behind. Derived from the content view's safe-area inset and the window's
   // contentLayoutRect (whichever reports the larger occlusion).
   int MacChrome_FullScreenTopInset (SDL_Window* pWindow);
}

#endif // RUBIDIUM_SHELL_MACCHROME_H
