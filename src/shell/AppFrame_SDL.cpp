// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// SDL-backed browser frame + tab manager. The per-tab engine state lives in
// APPFRAMETAB_SDL; this class mirrors the Win32 APPFRAME_NATIVE::Impl tab
// bookkeeping (add / close / activate) and, on Linux, drives the CHROME_RML
// tab strip and docks each tab's reparented canvas below it.

#include "AppFrame_SDL.h"
#include "canvas/Canvas.h"
#include "version.h"
#include "Brand.h"

#ifdef __ANDROID__
#include "shell/ChromeXr.h"
#endif

using namespace RUBIDIUM;

APPFRAME_SDL::APPFRAME_SDL (IAPPWINDOW* pController, SNEEZE::ENGINE* pSneeze, LOGGER* pLogger) :
   APPFRAME (pController, pLogger),
   m_pSneeze (pSneeze),
   m_pChrome (nullptr),
   m_pWindow (nullptr),
   m_nTabCount (0),
   m_nTabIx_Active (-1),
   m_eSession (SNEEZE::CONTEXT::kSESSION_PERSISTENT),
   m_bOpen (false),
   m_bCanvasMouseDown (false)
{
   for (int nTab = 0; nTab < TAB_MAX_COUNT; nTab++)
      m_apTab[nTab] = nullptr;
}

APPFRAME_SDL::~APPFRAME_SDL ()
{
   Shutdown ();
}

bool APPFRAME_SDL::Initialize (int nWidth, int nHeight, const char* sTitle, SNEEZE::CONTEXT::eSESSION eSession)
{
   bool bResult = false;

   m_eSession = eSession;

#if defined(RUBIDIUM_PLATFORM_LINUX) || defined(RUBIDIUM_PLATFORM_MACOS)
   // Desktop chrome (tab strip + URL bar) in its own SDL/RmlUi window. Each
   // tab's Filament canvas is reparented as a child of this window and docked
   // below ChromeHeight(). On Linux the reparent is XReparentWindow; on macOS
   // it is SDL_SetWindowParent (Cocoa child window). See CANVAS_NATIVE.
   m_pChrome = new CHROME_RML ();
   if (m_pChrome->Initialize (sTitle, nWidth, nHeight, this))
   {
      m_pWindow = m_pChrome->Window ();
      m_bOpen   = true;
   }
   else
   {
      m_pLogger->Log (LOGGER::kLOGLEVEL_Error, "APPFRAME_SDL", "Failed to create chrome window");
      delete m_pChrome;
      m_pChrome = nullptr;
   }
#else
#ifdef RUBIDIUM_PLATFORM_QUEST
   // OpenXR owns the HMD swapchain. SDL_CreateWindow blocks until an
   // ANativeWindow exists; Quest destroys that surface as soon as the
   // activity pauses (headset doffed or immersive compositor takeover).
   m_pWindow = nullptr;
   m_bOpen   = true;
   m_pLogger->Log (LOGGER::kLOGLEVEL_Info, "APPFRAME_SDL",
      std::string ("Quest XR frame (no SDL window) ") + std::to_string (nWidth) + "x" + std::to_string (nHeight));
   (void) sTitle;
#else
   m_pWindow = SDL_CreateWindow (sTitle, nWidth, nHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
   if (m_pWindow)
   {
      // Some platforms (Android, iOS) ignore the requested size and create the
      // window at the SurfaceView's actual extent, delivered asynchronously.
      // Pump and drain pending resize/pixel-size events before querying the
      // final extent so Sneeze + Filament initialize at the right size.
      for (int i = 0; i < 30; ++i)
      {
         SDL_PumpEvents ();
         SDL_Event ev;
         int nDrained = 0;
         while (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) > 0)
            ++nDrained;
         while (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_RESIZED, SDL_EVENT_WINDOW_RESIZED) > 0)
            ++nDrained;
         if (nDrained == 0 && i >= 2)
            break;
         SDL_Delay (16);
      }
      m_bOpen = true;
      m_pLogger->Log (LOGGER::kLOGLEVEL_Info, "APPFRAME_SDL", "SDL window created");
   }
   else m_pLogger->Log (LOGGER::kLOGLEVEL_Error, "APPFRAME_SDL", std::string ("SDL_CreateWindow failed: ") + SDL_GetError ());
#endif
#endif

   if (m_bOpen)
   {
      FrameTabAdd ();
      bResult = (m_nTabCount > 0);
   }

   return bResult;
}

void APPFRAME_SDL::Shutdown ()
{
   for (int nTab = 0; nTab < m_nTabCount; nTab++)
   {
      delete m_apTab[nTab];
      m_apTab[nTab] = nullptr;
   }
   m_nTabCount     = 0;
   m_nTabIx_Active = -1;

   if (m_pChrome)
   {
      delete m_pChrome;
      m_pChrome = nullptr;
      m_pWindow = nullptr;   // owned by the chrome window
   }
   else if (m_pWindow)
   {
      SDL_DestroyWindow (m_pWindow);
      m_pWindow = nullptr;
   }

   m_bOpen = false;
}

void* APPFRAME_SDL::ParentNativeHandle () const
{
#if defined(RUBIDIUM_PLATFORM_LINUX)
   return m_pChrome ? m_pChrome->NativeParentHandle () : nullptr;
#else
   return m_pWindow;
#endif
}

int APPFRAME_SDL::ContentTop () const
{
#if defined(RUBIDIUM_PLATFORM_LINUX) || defined(RUBIDIUM_PLATFORM_MACOS)
   return m_pChrome ? m_pChrome->ChromeHeight () : 0;
#else
   return 0;
#endif
}

bool APPFRAME_SDL::HostWindowSize (int& nWidth, int& nHeight) const
{
   bool bResult = false;

   if (m_pWindow)
   {
#if defined(RUBIDIUM_PLATFORM_LINUX) || defined(RUBIDIUM_PLATFORM_MACOS)
      // Chrome + child canvas dock in window points; both are non-HiDPI SDL
      // windows so points == pixels and the geometry stays in one unit.
      SDL_GetWindowSize (m_pWindow, &nWidth, &nHeight);
#else
      SDL_GetWindowSizeInPixels (m_pWindow, &nWidth, &nHeight);
#endif
      bResult = true;
   }
#ifdef RUBIDIUM_PLATFORM_QUEST
   else
   {
      nWidth  = 1920;
      nHeight = 1920;
      bResult = true;
   }
#endif

   return bResult;
}

void APPFRAME_SDL::FrameTabAdd ()
{
   if (m_nTabCount < TAB_MAX_COUNT)
   {
      int nWidth = 0, nHeight = 0;
      HostWindowSize (nWidth, nHeight);

      APPFRAMETAB_SDL* pTab = new APPFRAMETAB_SDL (m_pSneeze, m_pLogger);

      if (pTab->Init (ParentNativeHandle (), ContentTop (), nWidth, nHeight, m_eSession))
      {
         m_apTab[m_nTabCount] = pTab;
         FrameTabActive (m_nTabCount++, true);
         FrameTabsRefresh ();
      }
      else
      {
         m_pLogger->Log (LOGGER::kLOGLEVEL_Error, "APPFRAME_SDL", "Failed to create tab");
         delete pTab;
      }
   }
}

void APPFRAME_SDL::FrameTabActive (int nTabIx, bool bHideOld)
{
   if (nTabIx != m_nTabIx_Active  ||  !bHideOld)
   {
      if (bHideOld  &&  m_nTabIx_Active >= 0)
      {
         m_apTab[m_nTabIx_Active]->ViewportDetach ();
         m_apTab[m_nTabIx_Active]->Show (false);
      }

      m_nTabIx_Active = nTabIx;

      int nWidth = 0, nHeight = 0;
      HostWindowSize (nWidth, nHeight);

      // Show first, then dock: SetChildGeometry runs after the window is mapped
      // so the map can't leave it at SDL's stale creation position.
      m_apTab[m_nTabIx_Active]->Show (true);
      m_apTab[m_nTabIx_Active]->Resize (nWidth, nHeight, ContentTop ());
      m_apTab[m_nTabIx_Active]->ViewportAttach ();

      if (m_pWindow)
         SDL_SetWindowTitle (m_pWindow, m_apTab[m_nTabIx_Active]->Title ().c_str ());
   }
}

void APPFRAME_SDL::FrameTabClose (int nTabIx)
{
   if (nTabIx >= 0  &&  nTabIx < m_nTabCount)
   {
      bool bWasActive = (m_nTabIx_Active == nTabIx);

      delete m_apTab[nTabIx];
      m_nTabCount--;

      for (int nTab = nTabIx; nTab < m_nTabCount; nTab++)
         m_apTab[nTab] = m_apTab[nTab + 1];

      m_apTab[m_nTabCount] = nullptr;

      if (m_nTabIx_Active > nTabIx)
         m_nTabIx_Active--;
      else if (m_nTabIx_Active >= m_nTabCount)
         m_nTabIx_Active = m_nTabCount - 1;

      if (m_nTabCount > 0)
      {
         if (bWasActive)
            FrameTabActive (m_nTabIx_Active, false);

         FrameTabsRefresh ();
      }
      else
      {
         m_pController->Window_OnDestroy (this);
      }
   }
}

void APPFRAME_SDL::FrameTabsRefresh ()
{
   if (m_pChrome)
   {
      std::vector<std::string> asTitle;
      for (int nTab = 0; nTab < m_nTabCount; nTab++)
         asTitle.push_back (m_apTab[nTab]->Title ());

      m_pChrome->SetTabs (asTitle, m_nTabIx_Active);

      if (m_nTabIx_Active >= 0)
         m_pChrome->SetUrl (m_apTab[m_nTabIx_Active]->Url ());
   }
#ifdef __ANDROID__
   else if (m_nTabIx_Active >= 0)
      CHROME_XR::GetInstance ().SetUrl (m_apTab[m_nTabIx_Active]->Url ());
#endif
}

void APPFRAME_SDL::ProcessInput ()
{
   if (m_pWindow)
   {
      SDL_WindowID nHostID = SDL_GetWindowID (m_pWindow);

      SDL_Event ev;
      bool bRelayout = false;

#if defined(RUBIDIUM_PLATFORM_MACOS)
      // macOS fullscreen: track the menu-bar reveal each frame (the system menu
      // bar swallows mouse events while the cursor is over it, so this can't be
      // event-driven). When the chrome push-down inset changes, ContentTop moves
      // and the active tab's canvas must re-dock below the shifted chrome.
      if (m_pChrome  &&  m_pChrome->TickMacChrome ())
         bRelayout = true;
#endif

      // Any of these on the host (chrome / main) window can leave the embedded
      // canvas mis-docked -- in particular a fullscreen toggle or a
      // minimize->restore cycle (which fires RESTORED / SHOWN but not RESIZED).
      // Re-dock the active tab whenever one arrives. Canvas child windows also
      // emit some of these when we size them, so filter by the host window ID.
      static const Uint32 anRelayoutEvents[] =
      {
         SDL_EVENT_WINDOW_RESIZED,
         SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,
         SDL_EVENT_WINDOW_RESTORED,
         SDL_EVENT_WINDOW_MAXIMIZED,
         SDL_EVENT_WINDOW_SHOWN,
         SDL_EVENT_WINDOW_ENTER_FULLSCREEN,
         SDL_EVENT_WINDOW_LEAVE_FULLSCREEN,
      };

      for (Uint32 nType : anRelayoutEvents)
         while (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, nType, nType) > 0)
            if (ev.window.windowID == nHostID)
               bRelayout = true;

      // A genuine (non-live-resize) expose -- e.g. restore from minimize, which
      // some WMs report only via EXPOSED, not RESTORED/SHOWN -- forces a re-dock
      // so SDL can't leave the child at a stale position. Live-resize exposes
      // (data1 == 1) are already covered by RESIZED above, so skip those to
      // avoid re-docking every frame during a drag. Expose events are otherwise
      // serviced by the render event-watch; this loop also drains them.
      while (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_EXPOSED, SDL_EVENT_WINDOW_EXPOSED) > 0)
         if (ev.window.windowID == nHostID  &&  ev.window.data1 == 0)
            bRelayout = true;

      if (bRelayout  &&  m_nTabIx_Active >= 0)
      {
         int nWidth = 0, nHeight = 0;
         HostWindowSize (nWidth, nHeight);
         m_apTab[m_nTabIx_Active]->Resize (nWidth, nHeight, ContentTop ());
      }
   }

   // Mirror Win32 modal behaviour: while a modal child (the active tab's
   // Settings window, or the app-global "Release Notes" popup) is open, block chrome
   // clicks AND the 3D canvas input. SDL windowing modality (SetModalParent)
   // already disables the owner on backends that support it, but is a no-op on
   // e.g. Wayland -- this in-app guard makes the popup modal everywhere.
   bool bModal = (m_nTabIx_Active >= 0)  &&
      (m_apTab[m_nTabIx_Active]->IsSettingsOpen ()  ||  APPNATIVE::GetInstance ()->IsReleaseNotesOpen ());

   if (m_pChrome)
      m_pChrome->SetModalBlocked (bModal);

   // Clicking the 3D view hands the keyboard back to the viewport. The canvas is
   // a child window that never takes focus off the chrome by itself, so without
   // this the address bar keeps capturing keys after it has been clicked once and
   // the movement keys stay dead. Rising edge only -- a held button must not keep
   // re-blurring. Mirrors the Win32 frame's WM_PARENTNOTIFY focus hand-back.
   if (!bModal  &&  m_pChrome  &&  m_nTabIx_Active >= 0)
   {
      CANVAS* pCanvas = m_apTab[m_nTabIx_Active]->Canvas ();
      bool    bDown   = pCanvas  &&  (pCanvas->bMouseLeft  ||  pCanvas->bMouseRight);

      if (bDown  &&  !m_bCanvasMouseDown)
         m_pChrome->BlurTextEntry ();

      m_bCanvasMouseDown = bDown;
   }

   if (m_nTabIx_Active >= 0)
   {
      m_apTab[m_nTabIx_Active]->BlockInput (bModal);
      m_apTab[m_nTabIx_Active]->ProcessInput ();
   }
}

void APPFRAME_SDL::ApplyMovementKeyEvent (SDL_Event& Event)
{
   if (m_nTabIx_Active >= 0)
      m_apTab[m_nTabIx_Active]->ApplyMovementKeyEvent (Event);
}

bool APPFRAME_SDL::MovementKeysSuppressed () const
{
   bool bResult = false;

   if (m_pChrome  &&  m_pChrome->IsUrlBarFocused ())
      bResult = true;
   else
   {
      SDL_Window* pFocusWindow = SDL_GetKeyboardFocus ();

      // Inspector / Settings popups enable text input on their own windows --
      // mute viewport movement while the user is typing there. The chrome
      // window is handled above via IsUrlBarFocused; do not treat chrome text
      // input as a blanket suppressor (that killed WASD after any chrome click).
      if (pFocusWindow  &&  SDL_TextInputActive (pFocusWindow))
      {
         if (!m_pChrome  ||  pFocusWindow != m_pChrome->Window ())
            bResult = true;
      }
   }

   return bResult;
}

void APPFRAME_SDL::ToggleInspector ()
{
   if (m_nTabIx_Active >= 0)
      m_apTab[m_nTabIx_Active]->ToggleInspector ();
}

void APPFRAME_SDL::Reload (bool bReset)
{
   if (m_nTabIx_Active >= 0)
      m_apTab[m_nTabIx_Active]->Reload (bReset);
}

void* APPFRAME_SDL::NativeWindow () const
{
   return m_pWindow;
}

void APPFRAME_SDL::Chrome_OnTabSelect (int nTabIx)
{
   if (nTabIx >= 0  &&  nTabIx < m_nTabCount)
   {
      FrameTabActive (nTabIx, true);
      FrameTabsRefresh ();
   }
}

void APPFRAME_SDL::Chrome_OnTabClose (int nTabIx)
{
   FrameTabClose (nTabIx);
}

void APPFRAME_SDL::Chrome_OnTabAdd ()
{
   FrameTabAdd ();
}

void APPFRAME_SDL::Chrome_OnUrlSubmit (const std::string& sUrl)
{
   if (m_nTabIx_Active >= 0)
   {
      APPNATIVE::GetInstance ()->UrlHistory_Add (sUrl);
      m_apTab[m_nTabIx_Active]->Url (sUrl);
      FrameTabsRefresh ();
   }
}

std::vector<std::string> APPFRAME_SDL::Chrome_UrlHistory () const
{
   return APPNATIVE::GetInstance ()->UrlHistory ();
}

void APPFRAME_SDL::Chrome_OnExit ()
{
   m_pController->Window_OnExit ();
}

void APPFRAME_SDL::Chrome_OnNewWindow (SNEEZE::CONTEXT::eSESSION eSession)
{
   m_pController->Window_OnNew (this, eSession);
}

void APPFRAME_SDL::Chrome_OnToggleInspector ()
{
   ToggleInspector ();
}

void APPFRAME_SDL::Chrome_OnSetLogLevel (int nLevel)
{
   static const char* s_szLogLevels[LOGGER::kLOGLEVEL_COUNT] =
      { "trace", "info", "warning", "error", "off" };

   if (nLevel >= 0  &&  nLevel < LOGGER::kLOGLEVEL_COUNT)
   {
      LOGGER::eLOGLEVEL eLevel = static_cast<LOGGER::eLOGLEVEL> (nLevel);

      m_pLogger->LogLevel (eLevel);

      nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();
      jSettings["logger"]["level"] = s_szLogLevels[nLevel];
   }
}

int APPFRAME_SDL::Chrome_LogLevel () const
{
   return static_cast<int> (m_pLogger->LogLevel ());
}

bool APPFRAME_SDL::OwnsWindowID (SDL_WindowID nWindowID) const
{
   bool bOwns = false;

   if (m_pWindow  &&  SDL_GetWindowID (m_pWindow) == nWindowID)
      bOwns = true;
   else if (m_nTabIx_Active >= 0
        &&  m_apTab[m_nTabIx_Active]->Canvas ()
        &&  m_apTab[m_nTabIx_Active]->Canvas ()->SDLWindowID () == nWindowID)
      bOwns = true;

   return bOwns;
}

void APPFRAME_SDL::Chrome_OnSettings ()
{
   if (m_nTabIx_Active >= 0)
      m_apTab[m_nTabIx_Active]->ShowSettings (m_pWindow);
}

void APPFRAME_SDL::Chrome_OnReleaseNotes ()
{
   APPNATIVE::GetInstance ()->ShowReleaseNotes (m_pWindow);
}

void APPFRAME_SDL::Chrome_OnUpdate ()
{
   APPNATIVE::GetInstance ()->CheckForUpdate ();
}

#if defined(RUBIDIUM_PLATFORM_LINUX) || defined(RUBIDIUM_PLATFORM_MACOS)
void APPFRAME_SDL::DismissChromeMenuIfClickOutside (SDL_Event& Event)
{
   if (m_pChrome  &&  Event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
   {
      SDL_Window* pClickWindow = SDL_GetWindowFromEvent (&Event);

      if (m_pChrome->MenuOpen ()
       && pClickWindow != m_pChrome->Window ()
       && pClickWindow != m_pChrome->MenuWindow ())
         m_pChrome->DismissMenu ();

      // The URL history dropdown is dismissed on any click that is neither in
      // the chrome (a click there re-opens / submits) nor in the dropdown itself.
      if (m_pChrome->UrlDropOpen ()
       && pClickWindow != m_pChrome->Window ()
       && pClickWindow != m_pChrome->UrlDropWindow ())
         m_pChrome->DismissUrlDrop ();
   }
}
#endif

void* APPFRAME_SDL::Init (APPFRAME* pAppFrame_From, SNEEZE::CONTEXT::eSESSION eSession)
{
   int  nX = 0;
   int  nY = 0;
   int  nWidth = 1280;
   int  nHeight = 720;
   bool bMaximized = false;

   m_pController->Window_OnCreate (this, pAppFrame_From, nX, nY, nWidth, nHeight, bMaximized);

   if (Initialize (nWidth, nHeight, PRODUCT_NAME, eSession))
   {
#ifdef RUBIDIUM_PLATFORM_QUEST
      // No SDL window; Window_Create treats a non-null return as success.
      return this;
#else
      return NativeWindow ();
#endif
   }

   return nullptr;
}
