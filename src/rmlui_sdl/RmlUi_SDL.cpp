// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "RmlUi_SDL.h"
#include "RmlUi_SDL_Renderer.h"
#include "RmlUi_SDL_Platform.h"

#include <cstring>

#if defined(RUBIDIUM_PLATFORM_MACOS)
#include "shell/MacChrome.h"
#endif

#if defined(RUBIDIUM_PLATFORM_LINUX)
#include <cstdlib>
#include <iterator>
#include <X11/Xlib.h>
// Xlib defines bare macros (None, Bool, ...) that collide with SDL / RmlUi /
// Sneeze identifiers. The engine + RmlUi headers above are already parsed, so
// undefining the offenders here only guards this translation unit's own body
// (which -- verified -- uses none of these names). Mirrors Canvas_Linux.cpp.
#undef None
#undef Bool
#undef Status
#undef Success
#undef Always
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef CursorShape
#endif

namespace RUBIDIUM
{

class RMLUI_SDL::Impl
{
public:
   static inline int s_nInstanceCount = 0;

   Impl () :
      m_pSDLWindow        (nullptr),
      m_pSDLRenderer      (nullptr),
      m_pRmlRenderer      (nullptr),
      m_pRmlContext       (nullptr),
      m_pRmlDocument      (nullptr),
      m_nSDLWindowID      (0),
      m_bVisible          (false),
      m_bNeedsRender      (true),
      m_dLoggedPixelDensity (0.0f),
      m_dLoggedDisplayScale (0.0f),
      m_fnGeometry        (nullptr),
      m_pGeometryUserData (nullptr),
      m_fnVisibility       (nullptr),
      m_pVisibilityUserData (nullptr),
      m_pParentWindow     (nullptr)
   {
   }

   void SetVisible (bool bVisible)
   {
      m_bVisible = bVisible;

      if (m_fnVisibility)
         m_fnVisibility (bVisible, m_pVisibilityUserData);
   }

   void SyncWindowDpi ()
   {
      if (m_pSDLWindow  &&  m_pSDLRenderer  &&  m_pRmlContext)
      {
         // The window carries SDL_WINDOW_HIGH_PIXEL_DENSITY and the context is
         // sized in SDL points (see Render), so the physical resolution is
         // absorbed entirely by the render scale -- one point covers
         // dPixelDensity pixels. SDL reports display scale as pixel density
         // times the display's content scale, so the dp ratio has to divide the
         // density back out; feeding the raw display scale in as well lays a
         // Retina window out at double size. What is left is the pure UI scaling
         // on top of the physical resolution: 1.0 on a Retina Mac, 1.5 at 150%
         // on Windows.
         float dPixelDensity = SDL_GetWindowPixelDensity (m_pSDLWindow);
         float dDisplayScale = SDL_GetWindowDisplayScale (m_pSDLWindow);

         if (dPixelDensity <= 0.0f)
            dPixelDensity = 1.0f;

         if (dDisplayScale <= 0.0f)
            dDisplayScale = dPixelDensity;

         SDL_SetRenderScale (m_pSDLRenderer, dPixelDensity, dPixelDensity);

         m_pRmlContext->SetDensityIndependentPixelRatio (dDisplayScale / dPixelDensity);

         if (dPixelDensity != m_dLoggedPixelDensity  ||  dDisplayScale != m_dLoggedDisplayScale)
         {
            int nWindowW = 0;
            int nWindowH = 0;
            int nPixelW  = 0;
            int nPixelH  = 0;

            SDL_GetWindowSize         (m_pSDLWindow, &nWindowW, &nWindowH);
            SDL_GetWindowSizeInPixels (m_pSDLWindow, &nPixelW,  &nPixelH);

            SDL_Log ("RMLUI_SDL dpi: window=%dx%d pixels=%dx%d density=%.3f displayscale=%.3f dp=%.3f",
                     nWindowW, nWindowH, nPixelW, nPixelH,
                     dPixelDensity, dDisplayScale, dDisplayScale / dPixelDensity);

            m_dLoggedPixelDensity = dPixelDensity;
            m_dLoggedDisplayScale = dDisplayScale;
         }
      }
   }

   bool Initialize (const char* sTitle, int nWidth, int nHeight)
   {
      bool bResult = false;

      m_pSDLWindow = SDL_CreateWindow (sTitle, nWidth, nHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);

      if (m_pSDLWindow)
      {
         m_nSDLWindowID = SDL_GetWindowID (m_pSDLWindow);

#if defined(RUBIDIUM_PLATFORM_MACOS)
         // These auxiliary windows (inspector, settings, release notes) keep the
         // native green-button fullscreen. The crash on entering a fullscreen
         // Space came from presenting to the Metal drawable mid-transition (SDL's
         // live-resize event watch fires from inside the animation); Render skips
         // GPU work while MacChrome_IsFullScreenTransitioning is true. Ensure the
         // transition observers are installed.
         MacChrome_TrackFullScreenTransitions ();
#endif

         m_pSDLRenderer = SDL_CreateRenderer (m_pSDLWindow, nullptr);

         if (m_pSDLRenderer)
         {
            SDL_SetRenderVSync (m_pSDLRenderer, 1);

            m_pRmlRenderer = new RenderInterface_SDL (m_pSDLRenderer);
            m_pRmlRenderer->SetClearColor (0xff, 0xff, 0xff);

            std::string sContextName = "rmlui_sdl_" + std::to_string (s_nInstanceCount++);
            m_pRmlContext = Rml::CreateContext (sContextName, Rml::Vector2i (nWidth, nHeight), m_pRmlRenderer);

            if (m_pRmlContext)
            {
               SyncWindowDpi ();
               bResult = true;
            }
         }
      }

      return bResult;
   }

   ~Impl ()
   {
      if (m_pRmlContext)
      {
         Rml::RemoveContext (m_pRmlContext->GetName ());
         m_pRmlContext = nullptr;
      }

      Rml::ReleaseRenderManagers ();

      if (m_pRmlRenderer)
      {
         delete m_pRmlRenderer;
         m_pRmlRenderer = nullptr;
      }

      if (m_pSDLRenderer)
      {
         SDL_DestroyRenderer (m_pSDLRenderer);
         m_pSDLRenderer = nullptr;
      }

      if (m_pSDLWindow)
      {
         SDL_DestroyWindow (m_pSDLWindow);
         m_pSDLWindow = nullptr;
      }
   }

   void HandleEvent (SDL_Event& Event)
   {
      switch (Event.type)
      {
         case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
         case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
         {
            SyncWindowDpi ();
            break;
         }

         case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            // Clear modal before hiding (see the note in Toggle).
            if (m_pParentWindow)
               SDL_SetWindowModal (m_pSDLWindow, false);

            ExitFullscreen ();
            // Notify while the window is still mapped so child surfaces (e.g.
            // the inspector's PREVIEW3D canvas on macOS) can hide first. Hiding
            // the parent before its children on Cocoa leaves reparented Metal
            // windows orphaned on screen.
            SetVisible (false);
            SDL_HideWindow (m_pSDLWindow);
            break;

         case SDL_EVENT_WINDOW_MOVED:
         case SDL_EVENT_WINDOW_RESIZED:
            if (m_fnGeometry)
            {
               int nX = 0, nY = 0, nWidth = 0, nHeight = 0;
               bool bMaximized = (SDL_GetWindowFlags (m_pSDLWindow) & SDL_WINDOW_MAXIMIZED) ? true : false;
               SDL_GetWindowPosition (m_pSDLWindow, &nX, &nY);
               SDL_GetWindowSize (m_pSDLWindow, &nWidth, &nHeight);
               m_fnGeometry (nX, nY, nWidth, nHeight, bMaximized, m_pGeometryUserData);
            }
            break;

         default:
            break;
      }

      RmlSDL::InputEventHandler (m_pRmlContext, m_pSDLWindow, Event);
      m_bNeedsRender = true;

      Render ();
   }

   void Render ()
   {
      bool bCanRender = m_bVisible;

#if defined(RUBIDIUM_PLATFORM_MACOS)
      // Guard the Metal drawable through the native fullscreen enter/exit. Two
      // danger windows crash a present while the drawable is being recreated:
      //   (1) any present while the transition is animating, and
      //   (2) a present from inside SDL's event watch (which fires synchronously
      //       within Cocoa's own resize/transition callbacks) on a fullscreen
      //       window.
      // Skip GPU work in both; the main loop repaints once the animation ends
      // and the callback unwinds (flag it dirty so it is not skipped). Windowed
      // live-resize is untouched -- it renders through the event watch as before,
      // since a windowed, non-transitioning window hits neither case.
      bool bFullscreen = m_pSDLWindow  &&  (SDL_GetWindowFlags (m_pSDLWindow) & SDL_WINDOW_FULLSCREEN);

      if (MacChrome_IsFullScreenTransitioning ()  ||  (APP::InWindowFilter ()  &&  bFullscreen))
      {
         m_bNeedsRender = true;
         bCanRender     = false;
      }
#endif

      if (bCanRender)
      {
         SyncWindowDpi ();

         int nWindowW = 0;
         int nWindowH = 0;
         SDL_GetWindowSize (m_pSDLWindow, &nWindowW, &nWindowH);

         Rml::Vector2i Dimensions = m_pRmlContext->GetDimensions ();

         // On Linux/X11 the first paint after SDL_ShowWindow can run before the
         // WM reports a non-zero client size. Keep the context at its authored
         // dimensions until a real resize arrives.
         if (nWindowW > 0  &&  nWindowH > 0  &&
             (nWindowW != Dimensions.x  ||  nWindowH != Dimensions.y))
         {
            m_pRmlContext->SetDimensions (Rml::Vector2i (nWindowW, nWindowH));
            m_bNeedsRender = true;
         }

         m_pRmlContext->Update ();

         m_pRmlRenderer->BeginFrame ();
         m_pRmlContext->Render ();
         m_pRmlRenderer->EndFrame ();

         SDL_RenderPresent (m_pSDLRenderer);

         m_bNeedsRender = false;
      }
   }

   void RenderOffscreen ()
   {
      bool bWas = m_bVisible;
      m_bVisible = true;
      Render ();
      m_bVisible = bWas;
   }

   bool CaptureRgba (std::vector<uint8_t>& aRgba, int& nWidth, int& nHeight)
   {
      bool bOk = false;

      RenderOffscreen ();

      if (m_pSDLRenderer)
      {
         SDL_Surface* pSurface = SDL_RenderReadPixels (m_pSDLRenderer, nullptr);

         if (pSurface)
         {
            SDL_Surface* pRgba = SDL_ConvertSurface (pSurface, SDL_PIXELFORMAT_RGBA32);

            if (pRgba)
            {
               nWidth  = pRgba->w;
               nHeight = pRgba->h;
               aRgba.resize (static_cast<size_t> (nWidth) * static_cast<size_t> (nHeight) * 4u);
               if (SDL_MUSTLOCK (pRgba))
                  SDL_LockSurface (pRgba);
               const uint8_t* pSrc = static_cast<const uint8_t*> (pRgba->pixels);
               const int nPitch = pRgba->pitch;
               for (int nY = 0; nY < nHeight; nY++)
                  std::memcpy (aRgba.data () + static_cast<size_t> (nY) * static_cast<size_t> (nWidth) * 4u,
                     pSrc + nY * nPitch, static_cast<size_t> (nWidth) * 4u);
               if (SDL_MUSTLOCK (pRgba))
                  SDL_UnlockSurface (pRgba);
               SDL_DestroySurface (pRgba);
               bOk = true;
            }

            SDL_DestroySurface (pSurface);
         }
      }

      return bOk;
   }

   // macOS gives a native-fullscreen window (green button) its own Space. Hiding
   // the window while it is still fullscreen leaves that empty Space on screen as
   // a blank gray rectangle instead of returning to the desktop. Drop out of
   // fullscreen first and SDL_SyncWindow to let the (animated, asynchronous)
   // transition collapse the Space before the hide. No-op when not fullscreen and
   // harmless on other platforms.
   void ExitFullscreen ()
   {
      if (m_pSDLWindow  &&  (SDL_GetWindowFlags (m_pSDLWindow) & SDL_WINDOW_FULLSCREEN))
      {
         SDL_SetWindowFullscreen (m_pSDLWindow, false);
         SDL_SyncWindow (m_pSDLWindow);
      }
   }

   void Hide ()
   {
      // Clear modal status before hiding -- SDL leaves a phantom, un-closeable
      // window on macOS if a window is hidden while still flagged modal.
      if (m_pParentWindow)
         SDL_SetWindowModal (m_pSDLWindow, false);

      SDL_StopTextInput (m_pSDLWindow);
      ExitFullscreen ();
      SetVisible (false);
      SDL_HideWindow (m_pSDLWindow);
   }

   void Toggle ()
   {
      if (m_bVisible)
         Hide ();
      else
         Show ();
   }

   // Bring the window to the front (show + de-minimize + raise + focus) UNLESS
   // it is already the visible, focused window -- in which case hide it. A
   // single hotkey then reveals/raises a background window and only dismisses
   // it once it already has the user's attention.
   void ToggleFront ()
   {
      bool bFocused = m_pSDLWindow  &&
         (SDL_GetWindowFlags (m_pSDLWindow) & SDL_WINDOW_INPUT_FOCUS);

      if (m_bVisible  &&  bFocused)
         Hide ();
      else
         Show ();
   }

   // X11 focus-stealing prevention makes a plain SDL_RaiseWindow (an
   // _NET_ACTIVE_WINDOW request with the "application" source) from a window
   // that doesn't already hold focus frequently no-op, so an inspector sitting
   // behind the main window never comes forward on F12. Re-send the EWMH
   // activation with source indication 2 ("pager" / direct user request), which
   // WMs honor without the focus-stealing checks, then raise in the stacking
   // order for WMs that only consult that. No-op on Wayland / non-X11 (the X11
   // properties come back null) -- SDL_RaiseWindow covers those.
#if defined(RUBIDIUM_PLATFORM_LINUX)
   // WSLg's XWayland/Weston (proxied to the Windows host) silently drops
   // SDL_RaiseWindow, the EWMH activation and the always-on-top toggle for an
   // already-mapped window, so the inspector needs an unmap/remap to come
   // forward there. Native Linux desktops honour the standard raise, so this
   // lets the raise path stay native (flicker-free, matching macOS/Windows)
   // everywhere except WSL, where the remap fallback kicks in. Detected once.
   static bool RunningUnderWSL ()
   {
      static int s_nIsWSL = -1;

      if (s_nIsWSL < 0)
      {
         s_nIsWSL = (std::getenv ("WSL_DISTRO_NAME") || std::getenv ("WSL_INTEROP")) ? 1 : 0;

         if (!s_nIsWSL)
         {
            std::ifstream ifsVersion ("/proc/version");

            if (ifsVersion)
            {
               std::string sVersion ((std::istreambuf_iterator<char> (ifsVersion)),
                                      std::istreambuf_iterator<char> ());

               if (sVersion.find ("microsoft") != std::string::npos  ||
                   sVersion.find ("Microsoft") != std::string::npos  ||
                   sVersion.find ("WSL")       != std::string::npos)
                  s_nIsWSL = 1;
            }
         }
      }

      return s_nIsWSL == 1;
   }
#endif

   void PlatformRaise ()
   {
#if defined(RUBIDIUM_PLATFORM_LINUX)
      if (!m_pSDLWindow)
         return;

      SDL_PropertiesID nProps   = SDL_GetWindowProperties (m_pSDLWindow);
      Display*         pDisplay = static_cast<Display*> (SDL_GetPointerProperty (nProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
      Window           xWindow  = static_cast<Window>  (SDL_GetNumberProperty  (nProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));

      if (pDisplay  &&  xWindow)
      {
         Atom atomActive = XInternAtom (pDisplay, "_NET_ACTIVE_WINDOW", 0);

         if (atomActive != 0)
         {
            XEvent Event {};

            Event.type                 = ClientMessage;
            Event.xclient.window       = xWindow;
            Event.xclient.message_type = atomActive;
            Event.xclient.format       = 32;
            Event.xclient.data.l[0]    = 2;   // source indication: pager / direct user request
            Event.xclient.data.l[1]    = 0;   // timestamp (0 == CurrentTime)
            Event.xclient.data.l[2]    = 0;

            XSendEvent (pDisplay, DefaultRootWindow (pDisplay), 0,
               SubstructureRedirectMask | SubstructureNotifyMask, &Event);
         }

         XRaiseWindow (pDisplay, xWindow);
         XFlush (pDisplay);
      }
#endif
   }

   void Show ()
   {
      bool bFocused = m_pSDLWindow  &&
         (SDL_GetWindowFlags (m_pSDLWindow) & SDL_WINDOW_INPUT_FOCUS);

      // Already the visible, focused window -- it's the front window, so there is
      // nothing to raise. Bail out (just repaint) BEFORE the raise/remap dance
      // below. This is critical on WSLg: the only raise it honours is an
      // unmap/remap (see the block further down), and running that against the
      // window the user is currently interacting with makes it flicker
      // closed/open on every F12 and -- because WSLg doesn't preserve placement
      // across a remap -- march leftward across the screen. Only raise/remap when
      // the window is actually behind another window or hidden.
      if (m_bVisible  &&  bFocused)
      {
         Render ();
      }
      else
      {
         // Restore first so a minimized window comes back to its prior size
         // before being shown / raised.
         SDL_RestoreWindow (m_pSDLWindow);

         // WSLg only: SDL_RaiseWindow, the EWMH _NET_ACTIVE_WINDOW activation and
         // the always-on-top toggle are all silently dropped for a window that is
         // already mapped, so the inspector can never come forward on F12. The one
         // thing WSLg honours is a fresh map -- a newly-mapped toplevel always
         // appears front-most and focused (which is why the first F12 works). So
         // when the window is already open but behind, force an unmap/remap to
         // replay that one reliable path. Restricted to WSL: macOS, Windows and
         // native Linux desktops raise natively, and doing the remap there just
         // makes the inspector visibly flicker closed/open on every F12. No state
         // change (we don't touch m_bVisible or fire the visibility callback, and
         // a programmatic hide never raises SDL_EVENT_WINDOW_CLOSE_REQUESTED, so
         // nothing mistakes it for a close).
         //
         // The remap does NOT preserve the window's position on WSLg, so capture
         // the placement, unmap, then re-apply the position WHILE THE WINDOW IS
         // STILL HIDDEN and only then re-map it -- mirroring the very first open
         // (SetPosition on the hidden window, then Show). A position set on a
         // mapped window is dropped there; one pending when the window maps is
         // honoured. Setting it after SDL_ShowWindow (even with SDL_SyncWindow)
         // races the map and leaves the window offset.
#if defined(RUBIDIUM_PLATFORM_LINUX)
         if (RunningUnderWSL ()  &&  IsOpen ())
         {
            int nPrevX = 0, nPrevY = 0;

            SDL_GetWindowPosition (m_pSDLWindow, &nPrevX, &nPrevY);
            SDL_HideWindow (m_pSDLWindow);
            SDL_SyncWindow (m_pSDLWindow);
            SDL_SetWindowPosition (m_pSDLWindow, nPrevX, nPrevY);
         }
#endif

         SDL_ShowWindow (m_pSDLWindow);

         SyncWindowDpi ();

         // Native raise -- promote to the always-on-top band, raise, then drop
         // back so the window keeps its new front-most stacking position. This is
         // the path macOS, Windows and native Linux use; on WSLg it is ignored
         // (the remap above already did the work).
         SDL_SetWindowAlwaysOnTop (m_pSDLWindow, true);
         SDL_RaiseWindow (m_pSDLWindow);
         PlatformRaise ();
         SDL_SetWindowAlwaysOnTop (m_pSDLWindow, false);
         SDL_RaiseWindow (m_pSDLWindow);

         // Windowing-system modality (SDL requires the window be shown + parented
         // before the modal flag takes). No-op on backends that don't support it
         // (e.g. Wayland), where the parent relationship still keeps us on top.
         if (m_pParentWindow)
            SDL_SetWindowModal (m_pSDLWindow, true);

         // The global RmlUi system interface (owned by Sneeze) drives text input
         // for Sneeze's window, not this one. SDL3 text input is per-window, so
         // enable it explicitly here or inspector text fields receive no input.
         SDL_StartTextInput (m_pSDLWindow);

         SetVisible (true);

         Render ();
      }
   }

   bool IsOpen () const
   {
      return m_pSDLWindow  &&  ((SDL_GetWindowFlags (m_pSDLWindow) & SDL_WINDOW_HIDDEN) == 0);
   }

   void SetModalParent (SDL_Window* pParent)
   {
      m_pParentWindow = pParent;

      // Establish the parent/child link now, while hidden. SDL forbids changing
      // a window's parent while it is modal, and requires a parent before the
      // modal flag can be set (done in Show). Reparenting does not recreate the
      // window, so the bound renderer / RmlUi context stay valid.
      if (m_pSDLWindow  &&  m_pParentWindow)
         SDL_SetWindowParent (m_pSDLWindow, m_pParentWindow);
   }

   bool LoadDocument (const std::string& sRmlDocument)
   {
      bool bResult = false;

      if (m_pRmlContext)
      {
         m_pRmlDocument = m_pRmlContext->LoadDocumentFromMemory (sRmlDocument);

         if (m_pRmlDocument)
         {
            m_pRmlDocument->Show ();
            bResult = true;
         }
      }

      return bResult;
   }

   SDL_Window*           m_pSDLWindow;
   SDL_Renderer*         m_pSDLRenderer;
   SDL_WindowID          m_nSDLWindowID;

   RenderInterface_SDL*  m_pRmlRenderer;

   Rml::Context*         m_pRmlContext;
   Rml::ElementDocument* m_pRmlDocument;

   bool                  m_bVisible;
   bool                  m_bNeedsRender;

   float                 m_dLoggedPixelDensity;
   float                 m_dLoggedDisplayScale;

   FN_WINDOW_GEOMETRY    m_fnGeometry;
   void*                 m_pGeometryUserData;

   FN_WINDOW_VISIBILITY  m_fnVisibility;
   void*                 m_pVisibilityUserData;

   SDL_Window*           m_pParentWindow;    // non-null => this window is modal to it
};

/*******************************************************************************************************************************
**                                                    RMLUI_SDL                                                               **
*******************************************************************************************************************************/

RMLUI_SDL::RMLUI_SDL () :
   m_pImpl (new Impl ())
{
}

bool RMLUI_SDL::Initialize (const char* sTitle, int nWidth, int nHeight)
{
   bool bResult = m_pImpl->Initialize (sTitle, nWidth, nHeight);

   if (bResult)
      APPNATIVE::GetInstance ()->SDLWindow_Register (this);

   return bResult;
}

RMLUI_SDL::~RMLUI_SDL ()
{
   APPNATIVE::GetInstance ()->SDLWindow_Unregister (this);

   delete m_pImpl;
   m_pImpl = nullptr;
}

bool RMLUI_SDL::LoadDocument (const std::string& sRmlDocument)
{
   return m_pImpl->LoadDocument (sRmlDocument);
}

Rml::ElementDocument* RMLUI_SDL::Document ()       { return m_pImpl->m_pRmlDocument; }
bool                  RMLUI_SDL::IsVisible () const { return m_pImpl->m_bVisible; }
bool                  RMLUI_SDL::IsOpen () const    { return m_pImpl->IsOpen (); }

void* RMLUI_SDL::ParentNativeHandle () const
{
   void* pResult = nullptr;

   if (m_pImpl->m_pSDLWindow)
   {
#if defined(RUBIDIUM_PLATFORM_LINUX)
      SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pImpl->m_pSDLWindow);
      Uint64           nX11   = SDL_GetNumberProperty (nProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
      pResult = reinterpret_cast<void*> (static_cast<uintptr_t> (nX11));
#elif defined(RUBIDIUM_PLATFORM_WINDOWS)
      SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pImpl->m_pSDLWindow);
      pResult = SDL_GetPointerProperty (nProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
      pResult = m_pImpl->m_pSDLWindow;
#endif
   }

   return pResult;
}

SDL_WindowID RMLUI_SDL::SDLWindowID () const { return m_pImpl->m_nSDLWindowID; }

void RMLUI_SDL::HandleEvent (SDL_Event& Event) { m_pImpl->HandleEvent (Event); }
void RMLUI_SDL::Render ()                      { m_pImpl->Render (); }
void RMLUI_SDL::RenderOffscreen ()             { m_pImpl->RenderOffscreen (); }
bool RMLUI_SDL::CaptureRgba (std::vector<uint8_t>& aRgba, int& nWidth, int& nHeight)
{
   return m_pImpl->CaptureRgba (aRgba, nWidth, nHeight);
}
void RMLUI_SDL::Toggle ()                      { m_pImpl->Toggle (); }
void RMLUI_SDL::ToggleFront ()                 { m_pImpl->ToggleFront (); }
void RMLUI_SDL::Show ()                        { m_pImpl->Show (); }

void RMLUI_SDL::SetPosition (int nX, int nY)
{
   SDL_SetWindowPosition (m_pImpl->m_pSDLWindow, nX, nY);
}

void RMLUI_SDL::SetGeometryCallback (FN_WINDOW_GEOMETRY fnCallback, void* pUserData)
{
   m_pImpl->m_fnGeometry        = fnCallback;
   m_pImpl->m_pGeometryUserData = pUserData;
}

void RMLUI_SDL::SetVisibilityCallback (FN_WINDOW_VISIBILITY fnCallback, void* pUserData)
{
   m_pImpl->m_fnVisibility        = fnCallback;
   m_pImpl->m_pVisibilityUserData = pUserData;
}

void RMLUI_SDL::SetModalParent (void* pParent)
{
   m_pImpl->SetModalParent (static_cast<SDL_Window*> (pParent));
}

} // namespace RUBIDIUM
