// Copyright 2026 Metaversal Corporation. All rights reserved.

// macOS Metal layer configuration.
//
// SDL_Metal_CreateView adds the CAMetalLayer as a subview of SDL's content
// view, sized with contentView.frame (origin in the frame view, not the
// content view). That content view paints itself black, and on a Cocoa child
// window the subview's present never becomes the surface on screen. Install
// the Metal view as the window's content view, then size its drawable to the
// docked window, once the window is shown and on every dock.
//
// Filament's Metal backend reports isSRGBSwapChainSupported() == false, so it
// applies the sRGB transfer function in its post-processing pass and writes the
// already-encoded colour into a non-sRGB (BGRA8Unorm) drawable. For Core
// Animation to present those bytes the same way Windows does, the CAMetalLayer
// has to be told the contents are sRGB. The layer's default colour space is nil
// (no colour matching), which leaves the image un-managed and visibly dark
// next to the Windows Vulkan path -- where the identical encoded output is
// paired with an SRGB_NONLINEAR swapchain and presented correctly. Pinning the
// layer's colour space to sRGB makes the Metal present match Windows. Pixel
// format and colour space are set only before Filament claims the layer
// (device == nil); a later resize must not invalidate the swapchain.
//
// Compiled on macOS only (see src/CMakeLists.txt).

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <CoreGraphics/CoreGraphics.h>
#import <AppKit/AppKit.h>

#include <SDL3/SDL.h>

namespace RUBIDIUM
{
   void Canvas_InstallMetalView (SDL_Window* pWindow, void* pMetalView, int nWidth, int nHeight)
   {
      NSView* pView = (__bridge NSView*) pMetalView;

      if (pWindow != nullptr  &&  pView != nil  &&  nWidth > 0  &&  nHeight > 0)
      {
         id pCocoa = (__bridge id) SDL_GetPointerProperty (
            SDL_GetWindowProperties (pWindow), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);

         if (pCocoa != nil  &&  [pCocoa respondsToSelector: @selector (setContentView:)])
         {
            if ([pCocoa contentView] != pView)
               [pCocoa setContentView: pView];
         }

         pView.frame = NSMakeRect (0.0, 0.0, (CGFloat) nWidth, (CGFloat) nHeight);
         pView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

         CAMetalLayer* pMetalLayer = (CAMetalLayer*) pView.layer;

         if (pMetalLayer != nil)
         {
            if (pMetalLayer.device == nil)
            {
               pMetalLayer.opaque = YES;
               pMetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;

               CGColorSpaceRef pColorSpace = CGColorSpaceCreateWithName (kCGColorSpaceSRGB);

               if (pColorSpace != nullptr)
               {
                  // CAMetalLayer retains its own copy of the colour space, so the
                  // local Create reference is released immediately after assignment.
                  pMetalLayer.colorspace = pColorSpace;
                  CGColorSpaceRelease (pColorSpace);
               }
            }

            // Non-HiDPI canvas: window points and drawable pixels are one unit.
            pMetalLayer.contentsScale = 1.0;
            pMetalLayer.drawableSize = CGSizeMake ((CGFloat) nWidth, (CGFloat) nHeight);

            SDL_SetPointerProperty (SDL_GetWindowProperties (pWindow), "rubidium.metal.layer", (__bridge void*) pMetalLayer);
         }
      }
   }

   void Canvas_SetNativeVisible (SDL_Window* pWindow, bool bVisible)
   {
      if (pWindow)
      {
         SDL_PropertiesID nProps    = SDL_GetWindowProperties (pWindow);
         NSWindow*        pNSWindow = (__bridge NSWindow*) SDL_GetPointerProperty (
            nProps, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);

         if (pNSWindow != nil)
         {
            if (bVisible)
               [pNSWindow orderFront: nil];
            else
               [pNSWindow orderOut: nil];
         }
      }
   }
}
