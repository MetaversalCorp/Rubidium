// Copyright 2026 Metaversal Corporation. All rights reserved.

// macOS Metal layer colour-space configuration.
//
// Filament's Metal backend reports isSRGBSwapChainSupported() == false, so it
// applies the sRGB transfer function in its post-processing pass and writes the
// already-encoded colour into a non-sRGB (BGRA8Unorm) drawable. For Core
// Animation to present those bytes the same way Windows does, the CAMetalLayer
// has to be told the contents are sRGB. The layer's default colour space is nil
// (no colour matching), which leaves the image un-managed and visibly dark
// next to the Windows Vulkan path -- where the identical encoded output is
// paired with an SRGB_NONLINEAR swapchain and presented correctly. Pinning the
// layer's colour space to sRGB makes the Metal present match Windows.
//
// Compiled on macOS only (see src/CMakeLists.txt).

#import <QuartzCore/CAMetalLayer.h>
#import <CoreGraphics/CoreGraphics.h>
#import <AppKit/AppKit.h>

#include <SDL3/SDL.h>

namespace RUBIDIUM
{
   void Canvas_ConfigureMetalLayer (void* pLayer)
   {
      CAMetalLayer* pMetalLayer = (CAMetalLayer*) pLayer;

      if (pMetalLayer != nil)
      {
         CGColorSpaceRef pColorSpace = CGColorSpaceCreateWithName (kCGColorSpaceSRGB);

         if (pColorSpace != nullptr)
         {
            // CAMetalLayer retains its own copy of the colour space, so the
            // local Create reference is released immediately after assignment.
            pMetalLayer.colorspace = pColorSpace;
            CGColorSpaceRelease (pColorSpace);
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
