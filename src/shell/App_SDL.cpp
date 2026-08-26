// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// SDL platform entry (APPSDL) for macOS / Linux / iOS / Android. Mirrors
// App_Win32.cpp (APPNATIVE): Impl implements IAPPWINDOW; SDL event loop here.
//
// This is the minimum-viable orchestrator: one window, no single-instance
// mutex, no inspector toggle, no persisted window placement. Follow-up
// work will bring those features in alongside the RmlUi URL bar.

#include "shell/App_SDL.h"
#include "shell/AppFrame_SDL.h"
#include "rmlui_sdl/RmlUi_SDL_Platform.h"
//#include "shell/UrlBarRml.h"
#include "canvas/Canvas.h"

#include "logger/ILogger.h"
#include "version.h"

#include <atomic>
#include <cstdlib>
#include <exception>
#include <filesystem>

#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#endif

#if defined (__APPLE__)

#include <SDL3/SDL_vulkan.h>
#include <mach-o/dyld.h>
#include <cstdlib>

namespace
{

// Returns Contents/MacOS/ for the current executable (the .app's binary directory), empty string on failure.
std::string GetExecutableDir ()
{
   char sExe[4096] = {0};
   uint32_t nSize = sizeof (sExe);
   if (_NSGetExecutablePath (sExe, &nSize) != 0)
      return "";

   std::string sPath (sExe);
   std::string::size_type nLastSlash = sPath.find_last_of ('/');
   if (nLastSlash == std::string::npos)
      return "";
   return sPath.substr (0, nLastSlash);
}

void OnBeforeSDLInit (LOGGER* pLogger)
{
   // Set VK_ICD_FILENAMES to the bundled MoltenVK_icd.json before SDL reads it during Vulkan loader init.

   std::string sExeDir = GetExecutableDir ();
   if (!sExeDir.empty ()  &&  std::getenv ("VK_ICD_FILENAMES") == nullptr)
   {
      std::string sIcd = sExeDir + "/../Resources/MoltenVK_icd.json";
      setenv ("VK_ICD_FILENAMES", sIcd.c_str (), 0);
   }
}

void OnAfterSDLInit (LOGGER* pLogger)
{
   // macOS only delivers a window's activating click to the window *content*
   // when this hint is set. Without it, clicking an inactive Rubidium window
   // (e.g. while a terminal holds focus) merely activates the window and the
   // click is swallowed -- so our custom RmlUi window controls (the traffic-
   // light close / minimize / zoom buttons, tabs, the URL bar) appear to need a
   // double-click: one to focus, one to act. AppKit special-cases the *native*
   // traffic lights to act on the first click; since we hide those and draw our
   // own into #caption, we opt into the same one-click behavior here.
   SDL_SetHint (SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

   // After SDL_Init, force SDL to load libMoltenVK.dylib by full path so it doesn't fall back to dlopen's default search (which skips Contents/MacOS/).

   std::string sExeDir = GetExecutableDir ();
   if (!sExeDir.empty ())
   {
      std::string sDylib = sExeDir + "/libMoltenVK.dylib";
      if (!SDL_Vulkan_LoadLibrary (sDylib.c_str ()))
         pLogger->Log (LOGGER::kLOGLEVEL_Warning, "App", std::string ("SDL_Vulkan_LoadLibrary failed: ") + SDL_GetError ());
   }
}

} // namespace

#elif defined(__ANDROID__) || defined(RUBIDIUM_IOS)

namespace
{

void OnBeforeSDLInit (LOGGER* pLogger)
{
   // Force landscape on mobile so SDL doesn't briefly create the window in
   // the device's natural (portrait) orientation and then rotate, which
   // otherwise causes Filament to be initialized with stale dimensions.

   SDL_SetHint (SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
}

void OnAfterSDLInit (LOGGER* pLogger)
{
}

} // namespace

#else

namespace
{

void OnBeforeSDLInit (LOGGER* pLogger)
{
   // Prefer the X11 video driver (falling back to Wayland if X11 is
   // unavailable). The Wayland protocol has no per-window icon concept, so
   // SDL_SetWindowIcon is a silent no-op there and the taskbar shows a generic
   // icon; X11 / XWayland honour _NET_WM_ICON (which WSLg also surfaces in the
   // Windows taskbar). An explicit SDL_VIDEODRIVER env var still wins.
   if (std::getenv ("SDL_VIDEODRIVER") == nullptr)
      SDL_SetHint (SDL_HINT_VIDEO_DRIVER, "x11,wayland");
}

void OnAfterSDLInit (LOGGER* pLogger)
{
}

} // namespace

#endif

#if 0
#ifdef __ANDROID__
#include <jni.h>
#include <atomic>
#include <functional>
#include <string>

namespace
{
   // Wired up by APPSDL::Impl::Run () so the Java EditText overlay's submit
   // callback can deliver text into the active APPFRAME on the main thread.
   std::atomic<std::function<void(const std::string&)>*> g_pUrlSubmittedFn { nullptr };
}

extern "C" JNIEXPORT void JNICALL
Java_com_rp1_Rubidium_MainActivity_nativeUrlSubmitted (JNIEnv* env, jclass /*cls*/, jstring jUrl)
{
   auto* pFn = g_pUrlSubmittedFn.load ();
   if (!pFn || !jUrl)
      return;
   const char* pszUrl = env->GetStringUTFChars (jUrl, nullptr);
   if (pszUrl)
   {
      (*pFn) (std::string (pszUrl));
      env->ReleaseStringUTFChars (jUrl, pszUrl);
   }
}
#endif
#endif

static std::string GetAppDataDir ()
{
   std::string sPath;

#ifdef __ANDROID__
   const char* pszStorage = SDL_GetAndroidInternalStoragePath ();
   sPath = pszStorage ? std::string (pszStorage) : std::string (".");
#elif defined(__APPLE__)
   const char* pszHome = std::getenv ("HOME");
   sPath = pszHome ? std::string (pszHome) + "/Library/Application Support" : std::string (".");
#else
   const char* pszHome = std::getenv ("HOME");
   if (pszHome == nullptr)
      pszHome = "/tmp";

   sPath = std::string (pszHome) + "/.config";
   std::filesystem::create_directories (sPath);
#endif

   return sPath;
}

using namespace RUBIDIUM;

namespace
{
   std::atomic<bool>      g_bShuttingDown { false };
   std::terminate_handler g_pPrevTerminate = nullptr;

   // Filament (halogen) can throw utils::PostconditionPanic ("enumerate size
   // error") from its OWN backend thread during Vulkan teardown on software
   // drivers (llvmpipe / WSL). That throw is on a thread we don't own, so it
   // can't be caught and would abort() the process with a core dump on exit.
   // During shutdown, exit cleanly instead; outside shutdown, fall back to the
   // normal terminate behavior so genuine crashes still surface.
   void OnTerminate ()
   {
      if (g_bShuttingDown.load ())
         std::_Exit (0);

      if (g_pPrevTerminate)
         g_pPrevTerminate ();
      std::abort ();
   }
}

static const char* g_szLogLevels[LOGGER::kLOGLEVEL_COUNT] = { "trace", "info", "warning", "error", "off" };

namespace
{
   bool IsMovementKey (SDL_Keycode Key)
   {
      bool bResult = false;

      switch (Key)
      {
         case SDLK_W:
         case SDLK_A:
         case SDLK_S:
         case SDLK_D:
         case SDLK_SPACE:
         case SDLK_LCTRL:
         case SDLK_RCTRL:
            bResult = true;
            break;

         default:
            break;
      }

      return bResult;
   }
}

class APPSDL::Impl : public IAPPWINDOW, IUPDATER
{
public:
   Impl (APP* pApp) :
      m_pApp                  (pApp),
      m_pSneeze               (nullptr),
      m_pUpdater              (nullptr),
      m_pAppFrame_Active      (nullptr),
      m_pAppFrame_ActiveLast  (nullptr),
      m_bQuit                 (false),
      m_nPendingUpdate        (0),
      m_nReleaseNotesDeferFrames (0)
   {
   }

   ~Impl ()
   {
   }

   bool LoadFonts ()
   {
      const char* pBasePath = SDL_GetBasePath ();
      std::string sFontsDir = std::string (pBasePath) + "fonts/";

      static const char* asFontFiles[] =
      {
         "Inter/Inter-Regular.ttf",
         "Inter/Inter-Italic.ttf",
         "Inter/Inter-Bold.ttf",
         "Inter/Inter-BoldItalic.ttf",
         "Inter/Inter-Medium.ttf",
         "Inter/Inter-MediumItalic.ttf",
         "Inter/Inter-SemiBold.ttf",
         "Inter/Inter-SemiBoldItalic.ttf",
         "Inter/Inter-Light.ttf",
         "Inter/Inter-LightItalic.ttf",
         "JetBrainsMono/JetBrainsMono-Regular.ttf",
         "JetBrainsMono/JetBrainsMono-Bold.ttf",
         "JetBrainsMono/JetBrainsMono-Italic.ttf",
         "MaterialSymbolsOutlined/MaterialSymbolsOutlined.ttf",
      };

      bool bLoaded = true;

      for (const char* sFile : asFontFiles)
      {
         std::string sPath = sFontsDir + sFile;

         if (!Rml::LoadFontFace (sPath))
         {
            bLoaded = false;
            m_pApp->Logger ()->Log (LOGGER::kLOGLEVEL_Warning, "App", std::string ("Font not loaded: ") + sFile);
         }
      }

      if (bLoaded)
         m_pApp->sFontFamily ("Inter");

      return bLoaded;
   }

   bool EventLoop (int& nResult)
   {
      while (!m_bQuit)
      {
         bool bInputProcessedThisFrame = false;

         SDL_PumpEvents ();

         // Defer the "Release Notes" popup until the chrome window has pumped for a
         // few frames. On Linux/X11 a popup created + shown on the very first
         // EventLoop pass often never maps or composites (same class of failure
         // as showing before the loop at all -- see the note in Run ()).
         if (m_nReleaseNotesDeferFrames > 0)
         {
            m_nReleaseNotesDeferFrames--;

            if (m_nReleaseNotesDeferFrames == 0)
               m_pApp->ShowReleaseNotesIfUpdated (m_apAppFrame.empty () ? nullptr : m_apAppFrame.front ()->NativeWindow ());
         }

         SDL_Event ev;
         while (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT) > 0)
            m_bQuit = true;

         while (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_CLOSE_REQUESTED, SDL_EVENT_WINDOW_CLOSE_REQUESTED) > 0)
         {
            APPFRAME* pCloseFrame = nullptr;

            for (APPFRAME* pAppFrame : m_apAppFrame)
            {
               SDL_Window* pWindow = static_cast<SDL_Window*> (pAppFrame->NativeWindow ());
               if (pWindow  &&  SDL_GetWindowID (pWindow) == ev.window.windowID)
                  pCloseFrame = pAppFrame;
            }

            // Destroy just the requested window; quit only when the last app
            // window closes (handled in Window_OnDestroy). Non-app windows
            // (inspector, menu popup) route to their own ISDLWINDOW handler.
            if (pCloseFrame)
               Window_OnDestroy (pCloseFrame);
            else
               m_pApp->SDLWindow_Translate (ev);
         }

         // Route mouse + keyboard events to the per-window Canvas so viewport
         // input works (drag-rotate, scroll-zoom, +/- keys). The Win32 loop
         // does this via SDLWindow_Translate; the SDL loop previously consumed
         // only QUIT / CLOSE / RESIZE and dropped these.
         while (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_WHEEL) > 0)
         {
#if defined(RUBIDIUM_PLATFORM_LINUX) || defined(RUBIDIUM_PLATFORM_MACOS)
            for (APPFRAME* pAppFrame : m_apAppFrame)
               static_cast<APPFRAME_SDL*> (pAppFrame)->DismissChromeMenuIfClickOutside (ev);
#endif
            m_pApp->SDLWindow_Translate (ev);
         }

         while (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_FOCUS_GAINED, SDL_EVENT_WINDOW_FOCUS_LOST) > 0)
            m_pApp->SDLWindow_Translate (ev);

         // Fullscreen transitions reach the chrome so it can re-assert its
         // macOS title-bar configuration (the native green-button fullscreen
         // toggles the NSWindow style mask and drops those attributes on exit,
         // mispositioning the traffic-light buttons).
         while (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_ENTER_FULLSCREEN, SDL_EVENT_WINDOW_LEAVE_FULLSCREEN) > 0)
            m_pApp->SDLWindow_Translate (ev);

         while (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_EVENT_KEY_DOWN, SDL_EVENT_TEXT_INPUT) > 0)
         {
            if (ev.type == SDL_EVENT_KEY_DOWN  ||  ev.type == SDL_EVENT_KEY_UP)
            {
               if (m_pAppFrame_Active)
                  static_cast<APPFRAME_SDL*> (m_pAppFrame_Active)->ApplyMovementKeyEvent (ev);

               // Feed movement on the first KEY_DOWN so a quick tap is not missed
               // between 16 ms ProcessInput polls (matches Win32 waking on key down).
               if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && IsMovementKey (ev.key.key))
               {
                  for (APPFRAME* pAppFrame : m_apAppFrame)
                     pAppFrame->ProcessInput ();

                  bInputProcessedThisFrame = true;
               }
            }

            // F12 opens / raises the RmlUi Inspector (mirrors the Win32 F12
            // accelerator). It only ever shows the window -- ToggleInspector is
            // show-only, so the sole close path is the inspector's own X button,
            // matching the Windows and macOS builds. Consume it here so it never
            // reaches the viewport.
            //
            // Scope it to the frame that owns the focused window. On Linux the
            // canvas is a reparented X11 child, and when the 3D viewport has
            // focus SDL may report an unrecognized (or zero) key windowID, so
            // OwnsWindowID (ev.key.windowID) matches nothing and F12 would be a
            // no-op. Fall back to the SDL keyboard-focus window, then to the
            // front frame, so F12 always opens the inspector regardless of which
            // sub-window (chrome, canvas, or none) currently holds focus.
            if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_F12 && !ev.key.repeat)
            {
               APPFRAME_SDL* pTarget = nullptr;

               for (APPFRAME* pAppFrame : m_apAppFrame)
                  if (static_cast<APPFRAME_SDL*> (pAppFrame)->OwnsWindowID (ev.key.windowID))
                     pTarget = static_cast<APPFRAME_SDL*> (pAppFrame);

               if (!pTarget)
               {
                  SDL_Window*  pFocus   = SDL_GetKeyboardFocus ();
                  SDL_WindowID nFocusID = pFocus ? SDL_GetWindowID (pFocus) : 0;

                  for (APPFRAME* pAppFrame : m_apAppFrame)
                     if (static_cast<APPFRAME_SDL*> (pAppFrame)->OwnsWindowID (nFocusID))
                        pTarget = static_cast<APPFRAME_SDL*> (pAppFrame);
               }

               if (!pTarget  &&  !m_apAppFrame.empty ())
                  pTarget = static_cast<APPFRAME_SDL*> (m_apAppFrame.back ());

               if (pTarget)
                  pTarget->ToggleInspector ();

               continue;
            }

            // F5 reloads the active tab; holding Ctrl (Ctrl+F5, also the older
            // Ctrl+Alt+F5) reloads with a cache reset (hard reload), matching
            // Chrome. Mirrors the Win32 IDR_ACCEL F5 accelerators. Consume it
            // here so it never reaches the viewport.
            if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_F5 && !ev.key.repeat)
            {
               bool bReset = (ev.key.mod & SDL_KMOD_CTRL) != 0;
               for (APPFRAME* pAppFrame : m_apAppFrame)
                  if (static_cast<APPFRAME_SDL*> (pAppFrame)->OwnsWindowID (ev.key.windowID))
                     static_cast<APPFRAME_SDL*> (pAppFrame)->Reload (bReset);
               continue;
            }

#if defined(RUBIDIUM_PLATFORM_MACOS)
            // macOS: Command+R reloads the active tab, Command+Shift+R reloads
            // with a cache reset (hard reload) -- the standard Chrome/Safari
            // shortcuts on the Mac. Consume it here so it never reaches the
            // viewport. (F5 / Ctrl+F5 also work above.)
            if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_R && !ev.key.repeat
                && (ev.key.mod & SDL_KMOD_GUI))
            {
               bool bReset = (ev.key.mod & SDL_KMOD_SHIFT) != 0;
               for (APPFRAME* pAppFrame : m_apAppFrame)
                  if (static_cast<APPFRAME_SDL*> (pAppFrame)->OwnsWindowID (ev.key.windowID))
                     static_cast<APPFRAME_SDL*> (pAppFrame)->Reload (bReset);
               continue;
            }
#else
            // Linux: Ctrl+R reloads the active tab, Ctrl+Shift+R reloads with a
            // cache reset (hard reload) -- the standard Chrome shortcuts, matching
            // the Win32 Ctrl+R / Ctrl+Shift+R accelerators. Consume it here so it
            // never reaches the viewport. (F5 / Ctrl+F5 also work above.)
            if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_R && !ev.key.repeat
                && (ev.key.mod & SDL_KMOD_CTRL))
            {
               bool bReset = (ev.key.mod & SDL_KMOD_SHIFT) != 0;
               for (APPFRAME* pAppFrame : m_apAppFrame)
                  if (static_cast<APPFRAME_SDL*> (pAppFrame)->OwnsWindowID (ev.key.windowID))
                     static_cast<APPFRAME_SDL*> (pAppFrame)->Reload (bReset);
               continue;
            }
#endif

            m_pApp->SDLWindow_Translate (ev);
         }

         if (!bInputProcessedThisFrame)
         {
            for (APPFRAME* pAppFrame : m_apAppFrame)
               pAppFrame->ProcessInput ();
         }

         ProcessPendingUpdate ();

         SDL_Delay (16);
      }

      nResult = 0;

      return true;
   }

   bool Window_Create (APPFRAME* pAppFrame_From, SNEEZE::CONTEXT::eSESSION eSession)
   {
      APPFRAME* pAppFrame = new APPFRAME_NATIVE (this, m_pSneeze, m_pApp->Logger ());

      m_apAppFrame.push_back (pAppFrame);

      if (pAppFrame->Init (pAppFrame_From, eSession) == nullptr)
      {
         Window_Destroy (pAppFrame);
      }

      return (pAppFrame != nullptr);
   }

   void Window_Destroy (APPFRAME*& pAppFrame)
   {
      delete pAppFrame;

      auto it = std::find (m_apAppFrame.begin (), m_apAppFrame.end (), pAppFrame);
      if (it != m_apAppFrame.end ())
      {
         m_apAppFrame.erase (it);
      }

      pAppFrame = nullptr;
   }

   int Run ()
   {
      SNEEZE::ENGINE::CONFIG Config;

      g_pPrevTerminate = std::set_terminate (&OnTerminate);

      int nResult = 0;

      int nLevel;
      nlohmann::json& jSettings = m_pApp->SettingToJSON ();

      std::string sLevel = jSettings["logger"]["level"];
      for (nLevel = 0; nLevel < LOGGER::kLOGLEVEL_COUNT && sLevel.compare (g_szLogLevels[nLevel]) != 0; nLevel++);
      m_pApp->Logger ()->LogLevel (static_cast<LOGGER::eLOGLEVEL> (nLevel));

      m_pApp->Logger ()->Log (LOGGER::kLOGLEVEL_Info, "App", "Rubidium " + std::string (RUBIDIUM_VERSION));

      m_pSneeze = new SNEEZE::ENGINE (m_pApp);

      Config.bBoundingBox = jSettings["developer"]["boundingbox"];

      if (m_pSneeze->Initialize (Config))
      {
         OnBeforeSDLInit (m_pApp->Logger ());

         if (SDL_Init (SDL_INIT_VIDEO))
         {
            OnAfterSDLInit (m_pApp->Logger ());

            RubidiumRmlSystem_Install (m_pApp->Logger ());

            SDL_AddEventWatch (APP::SDLWindow_Filter, m_pApp);

            if (LoadFonts ())
            {
               if (Window_Create (nullptr, SNEEZE::CONTEXT::kSESSION_PERSISTENT))
               {
#if 0
#ifdef __ANDROID__
                  // Android: URL bar is a Java EditText overlay on top of the SDL
                  // SurfaceView (see MainActivity.java). The RmlUi path can't draw
                  // on top of Filament's Vulkan swapchain without a Sneeze-side
                  // overlay pass — see follow-up work.
                  m_fnUrlSubmitted = [this](const std::string& sUrl) { m_pFrame->UrlText (sUrl); };
                  g_pUrlSubmittedFn.store (&m_fnUrlSubmitted);
#else
                  SDL_Window* pMainWindow = static_cast<SDL_Window*> (m_pFrame->NativeWindow ());
                  CANVAS* pCanvas = m_pFrame->Canvas ();
                  SDL_Renderer* pRenderer = pCanvas ? pCanvas->Renderer () : nullptr;

                  m_pUrlBar = new URL_BAR_RML ();
                  if (pMainWindow && pRenderer &&
                     m_pUrlBar->Initialize (pMainWindow, pRenderer, nWidth, sHome,
                        [this](const std::string& sUrl) { m_pFrame->UrlText (sUrl); }))
                  {
                     pCanvas->SetOverlay ([this](SDL_Renderer* pR) { m_pUrlBar->Render (pR); });
                  }
                  else
                  {
                     m_pApp->Logger ()->Log (LOGGER::kLOGLEVEL_Warning, "App", "Failed to initialize URL bar");
                     delete m_pUrlBar;
                     m_pUrlBar = nullptr;
                  }
#endif
#endif
                  if (m_pApp->CreateUpdater (this))
                  {
                     // Defer the "Release Notes" popup into the live EventLoop instead
                     // of showing it here. On pure-SDL platforms a window created
                     // and shown before the loop pumps never maps / composites.
                     // A few extra frames after the first pump are still needed on
                     // Linux/X11 (see EventLoop ()).
                     m_nReleaseNotesDeferFrames = 8;

                     EventLoop (nResult);
                  }

                  m_pApp->DestroyUpdater ();

#if 0
#ifdef __ANDROID__
                  g_pUrlSubmittedFn.store (nullptr);
                  m_fnUrlSubmitted = nullptr;
#else
                  if (m_pUrlBar)
                  {
                     if (CANVAS* pC = m_pFrame->Canvas ())
                        pC->SetOverlay (nullptr);
                     m_pUrlBar->Shutdown ();
                     delete m_pUrlBar;
                     m_pUrlBar = nullptr;
                  }
#endif
#endif
               }
               else m_pApp->Logger ()->Log (LOGGER::kLOGLEVEL_Error, "App", "Failed to create application window");
            }
            else m_pApp->Logger ()->Log (LOGGER::kLOGLEVEL_Error, "App", "Failed to load fonts");

            SDL_RemoveEventWatch (APP::SDLWindow_Filter, m_pApp);

            SDL_Quit ();
         }
         else m_pApp->Logger ()->Log (LOGGER::kLOGLEVEL_Error, "App", std::string ("SDL_Init failed: ") + SDL_GetError ());
      }
      else m_pApp->Logger ()->Log (LOGGER::kLOGLEVEL_Error, "App", "Failed to initialize Sneeze engine");

      g_bShuttingDown.store (true);

      delete m_pSneeze;
      m_pSneeze = nullptr;

      return nResult;
   }

   void Window_OnCreate (APPFRAME* /*pAppFrame*/, APPFRAME* /*pAppFrame_From*/, int& /*nX*/, int& /*nY*/, int& /*nWidth*/, int& /*nHeight*/, bool& /*bMaximized*/) override 
   {
   }

   void Window_OnDestroy (APPFRAME* pAppFrame) override
   {
      APPFRAME* pTarget = pAppFrame;
      Window_Destroy (pTarget);

      if (m_apAppFrame.empty ())
         m_bQuit = true;
   }

   void Window_OnFocus   (APPFRAME* /*pAppFrame*/) override {}
   void Window_OnBlur    (APPFRAME* /*pAppFrame*/) override {}

   void Window_OnNew (APPFRAME* pAppFrame_From, SNEEZE::CONTEXT::eSESSION eSession) override
   {
      Window_Create (pAppFrame_From, eSession);
   }

   void Window_OnExit () override 
   { 
      m_bQuit = true; 
   }

   void onUpdaterAvailable (bool bAvailable) override
   {
      m_nPendingUpdate.store (bAvailable ? 1 : 2);
   }

   void ProcessPendingUpdate ()
   {
      int nPending = m_nPendingUpdate.exchange (0);

      if (nPending != 0)
      {
         SDL_PumpEvents ();
         SDL_Delay (50);

         IUPDATER::ePROMPT eResult = IUPDATER::kPROMPT_CANCEL;

         if (nPending == 1)
         {
            std::string sMsg   = "An update is available.";
            std::string sTitle = "Rubidium";
            eResult = onUpdaterPrompt (sMsg, sTitle);
         }
         else
         {
            bool bOk = SDL_ShowSimpleMessageBox (SDL_MESSAGEBOX_INFORMATION, "Rubidium", "No Update Available", static_cast<SDL_Window*> (m_pAppFrame_Active ? m_pAppFrame_Active->NativeWindow () : nullptr));
            if (!bOk)
               m_pApp->Logger ()->Log (LOGGER::kLOGLEVEL_Error, "Updater", std::string ("SDL_ShowSimpleMessageBox failed: ") + SDL_GetError ());
         }

         if (eResult == IUPDATER::kPROMPT_YES)
         {
            m_pApp->ApplyUpdate ();
            Window_OnExit ();
         }
      }
   }

   IUPDATER::ePROMPT onUpdaterPrompt (std::string& sMsg, std::string& sTitle) override
   {
      SDL_MessageBoxButtonData aButtons[3] = {
         { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, ePROMPT::kPROMPT_YES,     "Install" },
         { 0,                                       ePROMPT::kPROMPT_CANCEL,  "Skip"    },
         { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, ePROMPT::kPROMPT_NO,      "Later"   },
      };
      SDL_MessageBoxData data = {};
      data.flags = SDL_MESSAGEBOX_INFORMATION;
      data.window = static_cast<SDL_Window*> (m_pAppFrame_Active ? m_pAppFrame_Active->NativeWindow () : nullptr);
      data.title = sTitle.c_str ();
      data.message = sMsg.c_str ();
      data.numbuttons = 3;
      data.buttons = aButtons;

      int nChoice = 0;
      if (SDL_ShowMessageBox (&data, &nChoice) == false)
      {
         nChoice = IUPDATER::ePROMPT::kPROMPT_CANCEL;
      }

      return (IUPDATER::ePROMPT)nChoice;
   }

public:
   APP*                       m_pApp;

   SNEEZE::ENGINE*            m_pSneeze;
   UPDATER_NATIVE*            m_pUpdater;
   std::vector<APPFRAME*>     m_apAppFrame;
   APPFRAME*                  m_pAppFrame_Active;
   APPFRAME*                  m_pAppFrame_ActiveLast;
   bool                       m_bQuit;
   std::atomic<int>           m_nPendingUpdate;   // 0 none, 1 available, 2 no-update
   int                        m_nReleaseNotesDeferFrames;
};

APPSDL* APPSDL::GetInstance ()
{
   static APPSDL instance;
   return &instance;
}

APPSDL::APPSDL () :
   APP (GetAppDataDir (), new ILOGGER_NATIVE (true)),
   m_pImpl (new Impl (this)),
   m_pILogger (nullptr)
{
}

APPSDL::~APPSDL ()
{
   delete m_pImpl;
   m_pImpl = nullptr;

   DestroyLogger ();
   delete m_pILogger;
   m_pILogger = nullptr;
}

int APPSDL::Run ()
{
   return m_pImpl->Run ();
}

bool APPSDL::MovementKeysSuppressed () const
{
   bool bResult = false;

   if (m_pImpl->m_pAppFrame_Active)
      bResult = static_cast<APPFRAME_SDL*> (m_pImpl->m_pAppFrame_Active)->MovementKeysSuppressed ();

   return bResult;
}