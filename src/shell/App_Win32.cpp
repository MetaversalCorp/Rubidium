// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// Win32 derived class of RUBIDIUM::APP. Owns the single-instance
// mutex, WNDCLASSEXW + accelerator infrastructure, the message-pump loop,
// and the persisted window placement read/write.

#include "logger/ILogger.h"
#include "version.h"
#include "Brand.h"
#include "Resource.h"
#include "WinUtils.h"
#include "rmlui_sdl/RmlUi_SDL_Platform.h"

// Opt in to ComCtl32 v6 (themed common controls). Without this the process runs
// against the v5 assembly, where combobox drop-lists ignore post-creation sizing
// and CB_SETMINVISIBLE is a no-op. The manifest dependency is merged into the
// linker-generated application manifest.
#pragma comment(linker, "/manifestdependency:\"type='win32' "                  \
   "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' "               \
   "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace RUBIDIUM;

static constexpr char* g_szLogLevels[LOGGER::kLOGLEVEL_COUNT] = { "trace", "info", "warning", "error", "off" };

static std::string GetAppDataDir ()
{
   std::string sPath;
   char        szPath[MAX_PATH] = {};

   if (SUCCEEDED (SHGetFolderPathA (nullptr, CSIDL_APPDATA, nullptr, 0, szPath)))
   {
      sPath = szPath;
      std::replace (sPath.begin (), sPath.end (), '\\', '/');
   }
   else
   {
      sPath = ".";
   }

   return sPath;
}

// ---------------------------------------------------------------------------
// Class: Impl
// ---------------------------------------------------------------------------

class APPNATIVE::Impl : public IAPPWINDOW, IUPDATER
{
public:
   Impl (APP* pApp) :
      m_pApp                  (pApp),
      m_hInstance             (GetModuleHandleA (nullptr)),
      m_hMutex                (NULL),
      m_hAccel                (NULL),
      m_pSneeze               (nullptr),
      m_pAppFrame_Active      (nullptr),
      m_pAppFrame_ActiveLast  (nullptr)
   {
      HWND hWnd;

      if ((m_hMutex = CreateMutexA (NULL, TRUE, PRODUCT_MUTEX)) != NULL)
      {
         if (GetLastError () == ERROR_ALREADY_EXISTS)
         {
            if ((hWnd = FindWindowA (PRODUCT_WINDOW_CLASS, NULL)) != NULL
               PostMessageA (hWnd, WM_LAUNCH, 0, 0);

            CloseHandle (m_hMutex);
            m_hMutex = NULL;
         }
      }
   }

   ~Impl ()
   {
      if (m_hMutex != NULL)
      {
         ReleaseMutex (m_hMutex);
         CloseHandle (m_hMutex);
      }
   }

   // --- APPFRAME LIST MANAGEMENT

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

   void Window_Destroy (APPFRAME* &pAppFrame)
   {
      delete pAppFrame;

      auto it = std::find (m_apAppFrame.begin (), m_apAppFrame.end (), pAppFrame);
      if (it != m_apAppFrame.end ())
      {
         m_apAppFrame.erase (it);
      }

      pAppFrame = nullptr;
   }

   // --- FONT LOADING

   bool LoadFonts ()
   {
      const char* pBasePath = SDL_GetBasePath ();
      std::string sFontsDir = std::string (pBasePath) + "fonts\\";

      static const char* asFontFiles[] =
      {
         "Inter\\Inter-Regular.ttf",
         "Inter\\Inter-Italic.ttf",
         "Inter\\Inter-Bold.ttf",
         "Inter\\Inter-BoldItalic.ttf",
         "Inter\\Inter-Medium.ttf",
         "Inter\\Inter-MediumItalic.ttf",
         "Inter\\Inter-SemiBold.ttf",
         "Inter\\Inter-SemiBoldItalic.ttf",
         "Inter\\Inter-Light.ttf",
         "Inter\\Inter-LightItalic.ttf",
         "JetBrainsMono\\JetBrainsMono-Regular.ttf",
         "JetBrainsMono\\JetBrainsMono-Bold.ttf",
         "JetBrainsMono\\JetBrainsMono-Italic.ttf",
         "MaterialSymbolsOutlined\\MaterialSymbolsOutlined.ttf",
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

   // -----

   bool EventLoop (int& nResult)
   {
      bool bResult = true;
      MSG msg;
      SDL_Event Event;

      m_hAccel = LoadAccelerators (m_hInstance, MAKEINTRESOURCE (IDR_ACCEL));

      while (bResult)
      {
         while (bResult  &&  PeekMessageA (&msg, nullptr, 0, 0, PM_REMOVE))
         {
            if (msg.message == WM_QUIT)
            {
               nResult = (int)msg.wParam;
               bResult = false;
            }
            else
            {
               HWND hActive = m_pAppFrame_Active ? (HWND)m_pAppFrame_Active->NativeWindow () : HWND_DESKTOP;

               if (hActive == HWND_DESKTOP  ||  !TranslateAccelerator (hActive, m_hAccel, &msg))
               {
                  TranslateMessage (&msg);
                  DispatchMessageA (&msg);
               }
            }
         }

         if (bResult)
         {
            while (SDL_PollEvent (&Event))
               m_pApp->SDLWindow_Translate (Event);

            for (APPFRAME* pAppFrame : m_apAppFrame)
               pAppFrame->ProcessInput ();

            MsgWaitForMultipleObjects (0, nullptr, FALSE, INFINITE, QS_ALLINPUT);
         }
      }

      return bResult;
   }

   int Run ()
   {
      int nResult = 0;
      WNDCLASSEXW WndClassExW = { 0 };
      INITCOMMONCONTROLSEX iccex;
      SNEEZE::ENGINE::CONFIG Config;

      if (m_hMutex != NULL)
      {
         if (SetProcessDpiAwarenessContext (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE)
         {
            iccex.dwSize = sizeof (iccex);
            iccex.dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES;

            if (InitCommonControlsEx (&iccex) != FALSE)
            {
               WndClassExW.cbSize = sizeof (WNDCLASSEXW);
               WndClassExW.lpszClassName = PRODUCT_WINDOW_CLASS_W;
               WndClassExW.lpfnWndProc = APPFRAME_NATIVE::WndProc;
               WndClassExW.style = CS_HREDRAW | CS_VREDRAW;
               WndClassExW.hInstance = m_hInstance;
               WndClassExW.hIcon = LoadIconA (m_hInstance, MAKEINTRESOURCE (IDI_ICON1));
               WndClassExW.hIconSm = WndClassExW.hIcon;

               if (RegisterClassExW (&WndClassExW) != 0)
               {
                  int nLevel;
                  nlohmann::json& jSettings = m_pApp->SettingToJSON ();

                  std::string sLevel = jSettings["logger"]["level"];
                  for (nLevel = 0; nLevel < LOGGER::kLOGLEVEL_COUNT && sLevel.compare (g_szLogLevels[nLevel]) != 0; nLevel++);
                  m_pApp->Logger ()->LogLevel (static_cast<LOGGER::eLOGLEVEL> (nLevel));

                  m_pApp->Logger ()->Log (LOGGER::kLOGLEVEL_Info, "App", std::string (PRODUCT_NAME) + " " + std::string (RUBIDIUM_VERSION));

                  m_pSneeze = new SNEEZE::ENGINE (m_pApp);

                  Config.bBoundingBox = jSettings["developer"]["boundingbox"];

                  if (m_pSneeze->Initialize (Config))
                  {
                     if (SDL_Init (SDL_INIT_VIDEO))
                     {
                        RubidiumRmlSystem_Install (m_pApp->Logger ());

                        SDL_AddEventWatch (APP::SDLWindow_Filter, m_pApp);

                        if (LoadFonts ())
                        {
                           if (Window_Create (nullptr, SNEEZE::CONTEXT::kSESSION_PERSISTENT))
                           {
                              if (m_pApp->CreateUpdater (this))
                              {
                                 m_pApp->ShowReleaseNotesIfUpdated (m_apAppFrame.empty () ? nullptr : m_apAppFrame.front ()->NativeWindow ());

                                 EventLoop (nResult);
                              }

                              m_pApp->DestroyUpdater ();
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

                  delete m_pSneeze;
                  m_pSneeze = nullptr;
               }
            }
         }
      }

      return nResult;
   }

   // ---------------------------------------------------------------------------
   // IAPPWINDOW
   // ---------------------------------------------------------------------------

   void Window_OnCreate (APPFRAME* pAppFrame, APPFRAME* pAppFrame_From, int &nX, int &nY, int &nWidth, int &nHeight, bool &bMaximized) override
   {
      if (pAppFrame_From != nullptr)
      {
         nX          = CW_USEDEFAULT;
         nY          = CW_USEDEFAULT;
         nWidth      = 1024;
         nHeight     = 768;
         bMaximized  = false;

         HWND hWnd = (HWND)pAppFrame_From->NativeWindow ();
         WINDOWPLACEMENT WindowPlacement = {};

         WindowPlacement.length = sizeof (WINDOWPLACEMENT);
         if (GetWindowPlacement (hWnd, &WindowPlacement))
         {
            RECT rc = WindowPlacement.rcNormalPosition;

            nWidth      = rc.right - rc.left;
            nHeight     = rc.bottom - rc.top;
            bMaximized  = false;  // TBD
         }
      }
      else
      {
         nlohmann::json& jSettings = m_pApp->SettingToJSON ();

         nX           = jSettings["window"].value ("x", 100);
         nY           = jSettings["window"].value ("y", 100);
         nWidth       = jSettings["window"].value ("width", 1280);
         nHeight      = jSettings["window"].value ("height", 720);
         bMaximized   = jSettings["window"].value ("maximized", false);
      }
   }

   void Window_OnDestroy (APPFRAME* pAppFrame) override
   {
      HWND hWnd = (HWND)pAppFrame->NativeWindow ();

      if (m_pAppFrame_Active == pAppFrame)
         m_pAppFrame_Active = nullptr;

      Window_Destroy (pAppFrame);

      if (m_apAppFrame.empty ())
      {
         WINDOWPLACEMENT WindowPlacement = {};
         WindowPlacement.length = sizeof (WINDOWPLACEMENT);

         if (GetWindowPlacement (hWnd, &WindowPlacement))
         {
            nlohmann::json& jSettings = m_pApp->SettingToJSON ();
            RECT Rect = WindowPlacement.rcNormalPosition;

            jSettings["window"]["x"] = Rect.left;
            jSettings["window"]["y"] = Rect.top;
            jSettings["window"]["width"] = Rect.right - Rect.left;
            jSettings["window"]["height"] = Rect.bottom - Rect.top;
            jSettings["window"]["maximized"] = (WindowPlacement.showCmd == SW_SHOWMAXIMIZED);
         }

         PostQuitMessage (0);
      }
   }

   void Window_OnFocus (APPFRAME* pAppFrame) override
   {
      m_pAppFrame_Active      = pAppFrame;
      m_pAppFrame_ActiveLast  = m_pAppFrame_Active;
   }

   void Window_OnBlur (APPFRAME* pAppFrame) override
   {
      if (m_pAppFrame_Active == pAppFrame)
      {
         m_pAppFrame_ActiveLast  = m_pAppFrame_Active;
         m_pAppFrame_Active      = nullptr;
      }
   }

   void Window_OnNew (APPFRAME* pAppFrame_From, SNEEZE::CONTEXT::eSESSION eSession) override
   {
      Window_Create (pAppFrame_From, eSession);
   }

   void Window_OnExit () override
   {
      for (auto pAppFrame : m_apAppFrame)
         PostMessage ((HWND)pAppFrame->NativeWindow (), WM_CLOSE, 0, 0);
   }

   void onUpdaterAvailable (bool bAvailable) override
   {
      HWND hWndParent = m_pAppFrame_Active ? (HWND)m_pAppFrame_Active->NativeWindow () : HWND_DESKTOP;

      int nResult;

      if (bAvailable)
         nResult = MessageBoxA (hWndParent, "An update is available. Would you like to install the update?", PRODUCT_NAME, MB_YESNO | MB_ICONINFORMATION);
      else 
         nResult = MessageBoxA (hWndParent, "No Update Available", PRODUCT_NAME, MB_OK | MB_ICONINFORMATION);

      if (nResult == IDYES)
      {
         m_pApp->ApplyUpdate ();

         Window_OnExit ();
      }
   }

   IUPDATER::ePROMPT onUpdaterPrompt (std::string& sMsg, std::string& sTitle) override
   {
      HWND hWndParent = m_pAppFrame_Active ? (HWND)m_pAppFrame_Active->NativeWindow () : HWND_DESKTOP;
      IUPDATER::ePROMPT eResult;

      int nResult = MessageBoxA (hWndParent, sMsg.c_str (), sTitle.c_str (), MB_YESNOCANCEL | MB_ICONINFORMATION);

      if (nResult == IDYES)
         eResult = IUPDATER::ePROMPT::kPROMPT_YES;
      else if (nResult == IDNO)
         eResult = IUPDATER::ePROMPT::kPROMPT_NO;
      else
         eResult = IUPDATER::ePROMPT::kPROMPT_CANCEL;

      return eResult;
   }

public:
   APP*                       m_pApp;
   HINSTANCE                  m_hInstance;
   HANDLE                     m_hMutex;
   HACCEL                     m_hAccel;

   SNEEZE::ENGINE*            m_pSneeze;
   std::vector<APPFRAME*>     m_apAppFrame;
   APPFRAME*                  m_pAppFrame_Active;
   APPFRAME*                  m_pAppFrame_ActiveLast;
};

APPNATIVE* APPNATIVE::GetInstance ()
{
   static APPNATIVE instance;
   return &instance;
}

APPNATIVE::APPNATIVE () :
   APP (GetAppDataDir (), new ILOGGER_NATIVE (true)),
   m_pImpl (new Impl (this)) 
{
}

APPNATIVE::~APPNATIVE () 
{
   delete m_pImpl; 
}

int APPNATIVE::Run () 
{ 
   return m_pImpl->Run (); 
}
