// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_CANVAS_CANVAS_H
#define RUBIDIUM_CANVAS_CANVAS_H

namespace RUBIDIUM {

class CANVAS : public ISDLWINDOW
{
public:
   CANVAS (LOGGER* pLogger, std::string sName);
   ~CANVAS ();

   virtual bool Initialize (void* pParentHandle, int nWidth, int nHeight);

   virtual void SetVisible (bool bVisible) = 0;

   // Create the SDL_Renderer + Texture for CPU-pixel blitting. Only needed
   // when the app-level renderer does NOT render directly to the window; see
   // RENDERER::IsRenderingToNativeSurface().
   bool CreatePresentation ();

   // Overlay hook: invoked between the framebuffer blit and SDL_RenderPresent
   // in Present(). Lets UI layers (URL bar etc.) draw on top of the engine
   // framebuffer without owning their own window or renderer.
   void SetOverlay (std::function<void(SDL_Renderer*)> fn);

   // Accessor for layers that need to construct SDL-backed render interfaces
   // (e.g. RmlUi's RenderInterface_SDL) before the first Present().
   SDL_Renderer* Renderer () const;

   // Returns a platform-native window handle extracted from the SDL_Window:
   //   Win32   : HWND
   //   Android : ANativeWindow*
   //   iOS     : UIView* (suitable for CAMetalLayer)
   //   macOS   : NSWindow*
   //   Wayland : wl_surface*
   //   X11     : Window (as void*)
   // Intended to be passed to RENDERER::SetNativeWindow() before its
   // Initialize() so it can opt into direct-to-window rendering.
   void* NativeWindowHandle () const;

   // Position + size the canvas surface inside its parent. On Linux the canvas
   // owns an X11 child window reparented under the chrome frame, so this moves
   // and resizes it (XMoveResizeWindow) to dock below the chrome strip. On
   // platforms that reuse the host window directly it just forwards the size.
   void SetChildGeometry (int nX, int nY, int nWidth, int nHeight);

   // Re-assert z-order above sibling content (RmlUi repaints the parent each
   // frame and can bury an embedded native child on Windows).
   virtual    void RaiseChild ();

   void ProcessInput (SNEEZE::VIEWPORT* pViewport);

   // Update movement-key state from a KEY_DOWN/KEY_UP event. Called from the
   // app event loop for every key event (regardless of which SDL window owns
   // keyboard focus) as well as from HandleEvent when the canvas receives keys.
   void ApplyMovementKeyEvent (SDL_Event& Event);

   void Present (const uint32_t* pPixels, int nWidth, int nHeight);
   void Resize (int nWidth, int nHeight);
   bool FrameSize (int& nWidth, int& nHeight);

   // ISDLWINDOW
   SDL_WindowID SDLWindowID () const override;
   void         HandleEvent (SDL_Event& Event) override;

   // Input state (updated by HandleEvent)
   int   nMouseX;
   int   nMouseY;
   int   nMouseDX;
   int   nMouseDY;
   bool  bMouseLeft;
   bool  bMouseRight;
   float dScrollY;
   bool  bKeySpace;
   bool  bKeyPlus;
   bool  bKeyMinus;
   bool  bKeySlash;
   bool  bKeyA;
   bool  bKeyS;
   bool  bKeyD;
   bool  bKeyW;
   bool  bKeyCtrl;

protected:
   SDL_Window*    m_pWindow;
   bool           m_bOwnsWindow;
   LOGGER*        m_pLogger;

   // Last docked child geometry (Linux embedded window). Re-asserted whenever
   // the child is re-exposed so SDL / the WM can't leave it off-origin after a
   // minimize/restore or fullscreen toggle. m_nChildW < 0 means "not docked".
   int            m_nChildX;
   int            m_nChildY;
   int            m_nChildW;
   int            m_nChildH;

   virtual void ApplyChildGeometry ();

private:
   SDL_Renderer*  m_pSDLRenderer;
   SDL_Texture*   m_pTexture;
   int            m_nWidth_Pending;
   int            m_nHeight_Pending;
   int            m_nWidth;
   int            m_nHeight;

   std::function<void(SDL_Renderer*)> m_fnOverlay;
   std::mutex     m_textureMutex;
   std::string    m_sName;

   // Rising-edge latches for the movement-speed keys ('+'/'-'/'/') so a held key
   // advances the speed position exactly one notch per press.
   bool           m_bPrevPlus;
   bool           m_bPrevMinus;
   bool           m_bPrevSlash;
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_CANVAS_CANVAS_H
