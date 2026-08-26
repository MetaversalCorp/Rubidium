// Copyright 2026 Metaversal Corporation. All rights reserved.

#import <Cocoa/Cocoa.h>

#include <SDL3/SDL.h>

#include "MacChrome.h"

namespace RUBIDIUM
{
   // Tab-strip gray from ChromeRml.cpp (#DEE1E6). Used as the NSWindow and
   // content-view backing so any transient AppKit title-bar band during a
   // fullscreen menu-bar reveal matches chrome instead of flashing white.
   static NSColor* MacChrome_ChromeBackgroundColor ()
   {
      return [NSColor colorWithCalibratedRed:222.0 / 255.0
                                         green:225.0 / 255.0
                                          blue:230.0 / 255.0
                                         alpha:1.0];
   }

   // SDL_PROP_WINDOW_COCOA_WINDOW_POINTER returns SDL's Cocoa wrapper
   // (SDL3Window). It implements many NSWindow-like selectors but is not an
   // NSWindow subclass -- do not cast blindly or call NSWindow-only APIs on it.
   static id MacChrome_CocoaWindow (SDL_Window* pWindow)
   {
      id pCocoaObject = nil;

      if (pWindow)
      {
         void* pCocoa = SDL_GetPointerProperty (
            SDL_GetWindowProperties (pWindow), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);

         if (pCocoa)
            pCocoaObject = (__bridge id) pCocoa;
      }

      return pCocoaObject;
   }

   // With fullSizeContentView the SDL content view should consume the entire
   // window, but AppKit can still lay it out in contentLayoutRect. When the
   // menu bar slides down in a native fullscreen Space, that leaves a white
   // title-bar band above the SDL surface. Expand the content view to the
   // theme frame's bounds so RmlUi chrome paints through the title-bar area.
   static void MacChrome_ExpandContentView (id pCocoaWindow)
   {
      if (![pCocoaWindow respondsToSelector: @selector (contentView)])
         return;

      NSView* pContentView = [pCocoaWindow contentView];

      if (pContentView)
      {
         NSView* pFrameView = pContentView.superview;

         if (pFrameView)
            [pContentView setFrame: pFrameView.bounds];

         pContentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
         pContentView.wantsLayer       = YES;
         pContentView.layer.backgroundColor = MacChrome_ChromeBackgroundColor ().CGColor;
      }
   }

   void MacChrome_ConfigureTitleBar (SDL_Window* pWindow)
   {
      id pCocoaWindow = MacChrome_CocoaWindow (pWindow);

      if (pCocoaWindow)
      {
         // Extend the content view under the title bar, hide the title text,
         // and make the title-bar background transparent so the RmlUi tab
         // strip draws across the entire top band -- including in fullscreen,
         // where the (now empty, transparent) title bar reserves no space.
         if ([pCocoaWindow respondsToSelector: @selector (setStyleMask:)])
         {
            NSUInteger nStyleMask = [pCocoaWindow styleMask];
            nStyleMask |= NSWindowStyleMaskFullSizeContentView;
            [pCocoaWindow setStyleMask: nStyleMask];
         }

         if ([pCocoaWindow respondsToSelector: @selector (setTitlebarAppearsTransparent:)])
            [pCocoaWindow setTitlebarAppearsTransparent: YES];

         if ([pCocoaWindow respondsToSelector: @selector (setTitleVisibility:)])
            [pCocoaWindow setTitleVisibility: NSWindowTitleHidden];

         if ([pCocoaWindow respondsToSelector: @selector (setMovableByWindowBackground:)])
            [pCocoaWindow setMovableByWindowBackground: NO];

         if ([pCocoaWindow respondsToSelector: @selector (setOpaque:)])
            [pCocoaWindow setOpaque: NO];

         if ([pCocoaWindow respondsToSelector: @selector (setBackgroundColor:)])
            [pCocoaWindow setBackgroundColor: MacChrome_ChromeBackgroundColor ()];

         if (@available (macOS 11.0, *))
         {
            if ([pCocoaWindow respondsToSelector: @selector (setTitlebarSeparatorStyle:)])
               [pCocoaWindow setTitlebarSeparatorStyle: NSTitlebarSeparatorStyleNone];
         }

         // Hide the native traffic-light buttons. Rubidium draws its own
         // red / yellow / green controls into the tab strip (see ChromeRml).
         if ([pCocoaWindow respondsToSelector: @selector (standardWindowButton:)])
         {
            [pCocoaWindow standardWindowButton: NSWindowCloseButton].hidden       = YES;
            [pCocoaWindow standardWindowButton: NSWindowMiniaturizeButton].hidden = YES;
            [pCocoaWindow standardWindowButton: NSWindowZoomButton].hidden        = YES;
         }

         MacChrome_ExpandContentView (pCocoaWindow);
      }

      // If a fullscreen title-bar overlay already exists (re-config runs on each
      // ENTER / LEAVE_FULLSCREEN transition), neutralize it too so the band
      // never survives a transition.
      MacChrome_SuppressFullScreenTitleBar (pWindow);
   }

   void MacChrome_ToggleFullScreen (SDL_Window* pWindow)
   {
      id pCocoaWindow = MacChrome_CocoaWindow (pWindow);

      if (pCocoaWindow  &&  [pCocoaWindow respondsToSelector: @selector (toggleFullScreen:)])
         [pCocoaWindow toggleFullScreen: nil];
   }

   // >0 while any window is mid fullscreen enter/exit animation. Incremented on
   // the "will" notifications, decremented on the matching "did" -- AppKit
   // always pairs them, so the counter returns to 0.
   static int g_nFullScreenTransitionDepth = 0;

   void MacChrome_TrackFullScreenTransitions ()
   {
      static bool bInstalled = false;

      if (!bInstalled)
      {
         bInstalled = true;

         NSNotificationCenter* pCenter = [NSNotificationCenter defaultCenter];

         // object:nil observes every window; blocks run on the main thread (these
         // notifications post there), same thread as Render, so the counter needs
         // no synchronization.
         [pCenter addObserverForName: NSWindowWillEnterFullScreenNotification object: nil queue: nil
                          usingBlock: ^(NSNotification*) { g_nFullScreenTransitionDepth++; }];
         [pCenter addObserverForName: NSWindowWillExitFullScreenNotification  object: nil queue: nil
                          usingBlock: ^(NSNotification*) { g_nFullScreenTransitionDepth++; }];
         [pCenter addObserverForName: NSWindowDidEnterFullScreenNotification  object: nil queue: nil
                          usingBlock: ^(NSNotification*) { if (g_nFullScreenTransitionDepth > 0) g_nFullScreenTransitionDepth--; }];
         [pCenter addObserverForName: NSWindowDidExitFullScreenNotification   object: nil queue: nil
                          usingBlock: ^(NSNotification*) { if (g_nFullScreenTransitionDepth > 0) g_nFullScreenTransitionDepth--; }];
      }
   }

   bool MacChrome_IsFullScreenTransitioning ()
   {
      return g_nFullScreenTransitionDepth > 0;
   }

   void MacChrome_SuppressFullScreenTitleBar (SDL_Window* pWindow)
   {
      // The floating title-bar band is a distinct window AppKit lazily creates
      // on menu-bar reveal (class NSToolbarFullScreenWindow) and layers above
      // the fullscreen content -- expanding our own content view can never
      // cover it, so neutralize the overlay itself. Rubidium draws its entire
      // chrome (tabs, toolbar, traffic lights) into the RmlUi content, so the
      // overlay carries nothing we need: make it fully transparent and drop its
      // separator hairline. Only Rubidium windows exist in this process, so an
      // app-wide sweep is safe and covers multi-window fullscreen.
      Class OverlayClass = NSClassFromString (@"NSToolbarFullScreenWindow");

      if (OverlayClass)
      {
         for (NSWindow* pOverlay in [NSApp windows])
         {
            if ([pOverlay isKindOfClass: OverlayClass])
            {
               if ([pOverlay alphaValue] != 0.0)
                  [pOverlay setAlphaValue: 0.0];

               if (@available (macOS 11.0, *))
               {
                  if ([pOverlay respondsToSelector: @selector (setTitlebarSeparatorStyle:)])
                     [pOverlay setTitlebarSeparatorStyle: NSTitlebarSeparatorStyleNone];
               }
            }
         }
      }

      // Belt-and-suspenders: keep the SDL content view spanning the full frame
      // so a reveal-driven relayout can't drop it below a reserved title band.
      MacChrome_ExpandContentView (MacChrome_CocoaWindow (pWindow));
   }

   int MacChrome_MenuBarHeight ()
   {
      CGFloat dHeight = 0.0;

      NSMenu* pMainMenu = [NSApp mainMenu];

      if (pMainMenu)
         dHeight = [pMainMenu menuBarHeight];

      // menuBarHeight reports 0 while the bar is auto-hidden in fullscreen;
      // fall back to the standard 24-point menu bar so the push-down distance
      // stays correct during a reveal.
      if (dHeight <= 0.0)
         dHeight = 24.0;

      return static_cast<int> (dHeight + 0.5);
   }

   int MacChrome_FullScreenTopInset (SDL_Window* pWindow)
   {
      int nInset = 0;

      id pCocoaWindow = MacChrome_CocoaWindow (pWindow);

      if (pCocoaWindow  &&  [pCocoaWindow respondsToSelector: @selector (styleMask)])
      {
         bool bFullScreen = ([pCocoaWindow styleMask] & NSWindowStyleMaskFullScreen) != 0;

         if (bFullScreen  &&  [pCocoaWindow respondsToSelector: @selector (contentView)])
         {
            NSView* pContentView = [pCocoaWindow contentView];
            CGFloat dOcclusion   = 0.0;

            // Measure how far the revealed menu-bar / title-bar band reaches down
            // over the content. Two independent signals: the content view's
            // safe-area inset (modern), and the shrink of contentLayoutRect as the
            // band drops in. Take whichever is larger -- that is the true height
            // AppKit reserves, which [NSMenu menuBarHeight] cannot give us here
            // (it reports 0 while the bar is auto-hidden, the very state we poll
            // in). Pushing the chrome down by this measured amount lands the tab
            // strip / traffic lights exactly below the band, Chrome-style, instead
            // of under a too-short hard-coded push.
            if (pContentView)
            {
               if (@available (macOS 11.0, *))
                  dOcclusion = pContentView.safeAreaInsets.top;
            }

            if (pContentView  &&  [pCocoaWindow respondsToSelector: @selector (contentLayoutRect)])
            {
               NSRect  LayoutRect      = [pCocoaWindow contentLayoutRect];
               NSRect  ContentInWindow = [pContentView convertRect: [pContentView bounds] toView: nil];
               CGFloat dLayoutOcc      = NSMaxY (ContentInWindow) - NSMaxY (LayoutRect);

               if (dLayoutOcc > dOcclusion)
                  dOcclusion = dLayoutOcc;
            }

            if (dOcclusion > 0.5)
               nInset = static_cast<int> (dOcclusion + 0.5);
         }
      }

      return nInset;
   }
}
