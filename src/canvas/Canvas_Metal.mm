// Copyright 2026 Metaversal Corporation. All rights reserved.

// macOS Metal layer configuration.
//
// The red-clear test showed Filament presenting to a layer that is not the
// window on screen. The canvas window's own backing stays black. Move the
// SDL Metal view onto the chrome window, which is the window the user sees,
// into the region below the chrome strip. AppKit's origin is the bottom-left,
// so that region starts at y = 0. The canvas SDL window is then hidden.
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
   void Canvas_InstallMetalView (SDL_Window* pWindow, SDL_Window* pParent, void* pMetalView, int nX, int nY, int nWidth, int nHeight)
   {
      NSView* pView = (__bridge NSView*) pMetalView;

      if (pWindow != nullptr  &&  pView != nil  &&  nWidth > 0  &&  nHeight > 0)
      {
         SDL_Window* pHost = pParent != nullptr ? pParent : pWindow;
         id pCocoa = (__bridge id) SDL_GetPointerProperty (
            SDL_GetWindowProperties (pHost), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);

         if (pCocoa != nil  &&  [pCocoa respondsToSelector: @selector (contentView)])
         {
            NSView* pContent = [pCocoa contentView];

            if (pContent != nil)
            {
               int nHostH = 0;
               SDL_GetWindowSize (pHost, nullptr, &nHostH);

               // nY is the SDL top-left offset. AppKit's y grows upward.
               CGFloat nFrameY = (CGFloat) nHostH - (CGFloat) nY - (CGFloat) nHeight;
               if (nFrameY < 0.0)
                  nFrameY = 0.0;

               pView.frame = NSMakeRect ((CGFloat) nX, nFrameY, (CGFloat) nWidth, (CGFloat) nHeight);
               pView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
               [pContent addSubview: pView positioned: NSWindowAbove relativeTo: nil];
            }
         }

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

   void Canvas_OrderAbove (SDL_Window* pWindow, SDL_Window* pParent)
   {
      if (pWindow != nullptr  &&  pParent != nullptr)
      {
         id pChild = (__bridge id) SDL_GetPointerProperty (
            SDL_GetWindowProperties (pWindow), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
         id pHost = (__bridge id) SDL_GetPointerProperty (
            SDL_GetWindowProperties (pParent), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);

         if (pChild != nil  &&  pHost != nil
            &&  [pChild respondsToSelector: @selector (orderWindow:relativeTo:)]
            &&  [pHost respondsToSelector: @selector (windowNumber)])
         {
            [pChild orderWindow: NSWindowAbove relativeTo: [pHost windowNumber]];
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
