// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "shell/ChromeRml.h"

#include "rmlui_sdl/RmlUi_SDL_Renderer.h"
#include "rmlui_sdl/RmlUi_SDL_Platform.h"

#if defined(RUBIDIUM_PLATFORM_MACOS)
#include "shell/MacChrome.h"
#endif

#include <Image.h>

#include "Utils.h"

#include <algorithm>

namespace RUBIDIUM
{

// True when logo.png is readable at sPath (SDL_LoadFile, same check as the
// window icon path).
static bool LogoFileReadable (const std::string& sPath)
{
   bool bResult = false;

   if (!sPath.empty ())
   {
      size_t nSize = 0;
      void*  pData = SDL_LoadFile (sPath.c_str (), &nSize);

      if (pData)
      {
         SDL_free (pData);
         bResult = true;
      }
   }

   return bResult;
}

// Deployed logo path for RmlUi <img> src attributes. JoinPath strips one leading
// '/' from absolute POSIX paths, so prepend an extra '/' on Linux/macOS after
// the on-disk path is verified. Returns empty when logo.png cannot be found.
static std::string TabLogoPath ()
{
   std::string sResult;

   if (const char* pBasePath = SDL_GetBasePath ())
   {
      std::vector<std::string> asCandidate;
      asCandidate.push_back (std::string (pBasePath) + "images/logo.png");

#if defined(RUBIDIUM_PLATFORM_MACOS)
      // Some launch contexts resolve the base path to Contents/MacOS/ while the
      // POST_BUILD step installs logo.png under Contents/Resources/images/.
      asCandidate.push_back (std::string (pBasePath) + "../Resources/images/logo.png");
#endif

      for (std::string sPath : asCandidate)
      {
         std::replace (sPath.begin (), sPath.end (), '\\', '/');

         if (LogoFileReadable (sPath))
         {
            if (sPath[0] == '/')
               sPath = "/" + sPath;

            sResult = sPath;
            break;
         }
      }
   }

   return sResult;
}

// Escape a filesystem path for embedding in an HTML attribute (img src=).
static std::string HtmlAttrPath (const std::string& sPath)
{
   std::string sResult;
   sResult.reserve (sPath.size ());

   for (char cChar : sPath)
   {
      if (cChar == '&')
         sResult += "&amp;";
      else if (cChar == '"')
         sResult += "&quot;";
      else
         sResult += cChar;
   }

   return sResult;
}

// Chrome strip layout, authored at 1x in density-independent pixels (dp).
static const int kCHROME_TABBAR_DP  = 34;
static const int kCHROME_TOOLBAR_DP = 40;
static const int kCHROME_TOTAL_DP   = kCHROME_TABBAR_DP + kCHROME_TOOLBAR_DP;
static const int kCHROME_MENU_W_DP  = 240;

static const char* s_szChromeStyle = R"css(
body
{
   font-family: [{FONT-FAMILY}];
   font-size: 13dp;
   color: #1F1F1F;
   background-color: #DEE1E6;
   margin: 0;
   padding: 0;
   width: 100%;
   height: 100%;
   position: relative;
}
#tabstrip
{
   display: block;
   width: 100%;
   height: 34dp;
   padding: [{TABSTRIP-PAD}];
   box-sizing: border-box;
   background-color: #DEE1E6;
   white-space: nowrap;
}
#caption
{
   position: absolute;
   top: 0dp;
   [{CAPTION-POS}]
   height: 34dp;
   white-space: nowrap;
   z-index: 5;
}
.cap-btn
{
   display: inline-block;
   width: 24dp;
   height: 24dp;
   line-height: 24dp;
   margin-top: 5dp;
   margin-left: 4dp;
   text-align: center;
   vertical-align: top;
   font-family: "Material Symbols Outlined";
   font-size: 15dp;
   color: #2E3436;
   background-color: #D7D7D7;
   border-radius: 12dp;
}
.cap-btn:hover
{
   background-color: #C7C7C7;
}
.cap-close:hover
{
   background-color: #E01B24;
   color: #FFFFFF;
}
.mac-light
{
   display: inline-block;
   width: 12dp;
   height: 12dp;
   line-height: 12dp;
   margin-top: 11dp;
   margin-right: 8dp;
   text-align: center;
   vertical-align: top;
   font-family: "Material Symbols Outlined";
   font-size: 8dp;
   color: #00000000;
   border-radius: 6dp;
}
.mac-close { background-color: #FF5F57; }
.mac-min   { background-color: #FEBC2E; }
.mac-zoom  { background-color: #28C840; }
#caption:hover .mac-light
{
   color: #000000B3;
}
#caption.fullscreen .mac-min
{
   background-color: #C7C7C7;
}
#caption.fullscreen:hover .mac-min
{
   color: #00000000;
}
.tab
{
   display: inline-block;
   height: 30dp;
   width: 180dp;
   padding: 0dp 6dp 0dp 12dp;
   margin-right: 1dp;
   vertical-align: bottom;
   background-color: #C3C7CE;
   border-radius: 8dp 8dp 0dp 0dp;
   color: #1F1F1F;
}
.tab.active
{
   background-color: #FFFFFF;
}
.tab-favicon
{
   display: inline-block;
   width: 16dp;
   height: 16dp;
   margin-top: 7dp;
   margin-right: 6dp;
   vertical-align: top;
}
.tab-title
{
   display: inline-block;
   width: 118dp;
   height: 30dp;
   line-height: 30dp;
   vertical-align: top;
   overflow: hidden;
   text-overflow: ellipsis;
   white-space: nowrap;
   color: #1F1F1F;
}
.tab-close
{
   display: inline-block;
   float: right;
   width: 22dp;
   height: 30dp;
   line-height: 30dp;
   font-family: "Material Symbols Outlined";
   font-size: 16dp;
   text-align: center;
   color: #5F6368;
}
.tab-close:hover
{
   color: #1F1F1F;
}
.tab-add
{
   display: inline-block;
   width: 28dp;
   height: 28dp;
   line-height: 28dp;
   text-align: center;
   vertical-align: bottom;
   font-size: 18dp;
   color: #5F6368;
   border-radius: 6dp;
}
.tab-add:hover
{
   background-color: #C3C7CE;
}
#toolbar
{
   display: flex;
   align-items: center;
   width: 100%;
   height: 40dp;
   padding: 0dp 8dp;
   box-sizing: border-box;
   background-color: #FFFFFF;
   position: relative;
   gap: 6dp;
}
#urlwrap
{
   display: flex;
   align-items: center;
   flex: 1;
   min-width: 0;
   height: 30dp;
   border-radius: 15dp;
   background-color: #F1F3F4;
}
#urlbar
{
   display: block;
   flex: 1;
   min-width: 0;
   height: 30dp;
   margin: 0;
   padding: 0dp 12dp;
   box-sizing: border-box;
   line-height: 30dp;
   background-color: transparent;
   color: #1F1F1F;
   font-size: 13dp;
}
.url-caret
{
   flex-shrink: 0;
   width: 28dp;
   height: 30dp;
   line-height: 30dp;
   text-align: center;
   border-radius: 0dp 15dp 15dp 0dp;
   color: #5F6368;
   font-family: "Material Symbols Outlined";
   font-size: 20dp;
   cursor: pointer;
}
.url-caret:hover
{
   background-color: #E4E6E9;
   color: #1F1F1F;
}
.menu-btn
{
   flex-shrink: 0;
   width: 30dp;
   height: 30dp;
   line-height: 30dp;
   text-align: center;
   border-radius: 15dp;
   background-color: #E8EAED;
   color: #5F6368;
   font-family: "Material Symbols Outlined";
   font-size: 20dp;
   cursor: pointer;
}
.menu-btn:hover
{
   background-color: #DADCE0;
   color: #1F1F1F;
}
)css";

static const char* s_szMenuStyle = R"css(
body
{
   font-family: [{FONT-FAMILY}];
   font-size: 13dp;
   color: #1F1F1F;
   background-color: #FFFFFF;
   margin: 0;
   padding: 5dp 0dp;
   width: 100%;
   height: 100%;
   box-sizing: border-box;
   border-width: 1px;
   border-color: #DADCE0;
   border-radius: 4dp;
}
.menu-item
{
   display: block;
   width: 100%;
   height: 32dp;
   line-height: 32dp;
   padding: 0dp 16dp;
   box-sizing: border-box;
   color: #1F1F1F;
   cursor: pointer;
}
.menu-item:hover
{
   background-color: #F1F3F4;
}
.menu-sep
{
   display: block;
   height: 1dp;
   margin: 4dp 12dp;
   background-color: #E8EAED;
}
.menu-header
{
   display: block;
   width: 100%;
   height: 26dp;
   line-height: 26dp;
   padding: 0dp 16dp;
   box-sizing: border-box;
   color: #5F6368;
   font-size: 11dp;
}
.menu-radio
{
   display: block;
   width: 100%;
   height: 28dp;
   line-height: 28dp;
   padding: 0dp 16dp 0dp 12dp;
   box-sizing: border-box;
   color: #1F1F1F;
   cursor: pointer;
}
.menu-radio:hover
{
   background-color: #F1F3F4;
}
.menu-check
{
   display: inline-block;
   width: 20dp;
   font-family: "Material Symbols Outlined";
   font-size: 14dp;
   color: #0B57D0;
   vertical-align: middle;
}
)css";

static const char* s_szChromeDocument = R"rml(<rml>
<head>
<style>
[{STYLE}]
</style>
</head>
<body>
   <div id="caption"></div>
   <div id="tabstrip"></div>
   <div id="toolbar">
      <div id="urlwrap">
         <input type="text" id="urlbar"/>
         <div id="urldrop" class="url-caret" action="urldrop">&#xE5C5;</div>
      </div>
      <div id="menubtn" class="menu-btn" action="menu">&#xE5D4;</div>
   </div>
</body>
</rml>)rml";

static const char* s_szMenuDocument = R"rml(<rml>
<head>
<style>
[{STYLE}]
</style>
</head>
<body>
   <div id="menu-root"></div>
</body>
</rml>)rml";

// Ellipsis menu layout -- mirrors the Win32 popup. The table drives both the
// generated RML and the popup window height so the two stay in sync.
enum eMENU_TYPE { kMENU_ITEM, kMENU_SEP, kMENU_HEADER, kMENU_RADIO };

struct MENU_ENTRY
{
   const char* sAction;   // action attribute (empty for separators / headers)
   const char* sLabel;
   eMENU_TYPE  eType;
   int         nLevel;    // log level index for radio rows; -1 otherwise
   int         nHeightDp; // authored row height, summed to size the window
};

static const MENU_ENTRY s_aMenuEntries[] =
{
   { "new-window",    "New Window",           kMENU_ITEM,   -1, 32 },
   { "new-incognito", "New Incognito Window", kMENU_ITEM,   -1, 32 },
   { "",              "",                     kMENU_SEP,    -1,  9 },
   { "inspector",     "Inspector",            kMENU_ITEM,   -1, 32 },
   { "",              "",                     kMENU_SEP,    -1,  9 },
   { "settings",      "Settings",             kMENU_ITEM,   -1, 32 },
   { "",              "Log Level",            kMENU_HEADER, -1, 26 },
   { "loglevel",      "Trace",                kMENU_RADIO,   0, 28 },
   { "loglevel",      "Info",                 kMENU_RADIO,   1, 28 },
   { "loglevel",      "Warning",              kMENU_RADIO,   2, 28 },
   { "loglevel",      "Error",                kMENU_RADIO,   3, 28 },
   { "loglevel",      "Off",                  kMENU_RADIO,   4, 28 },
   { "",              "",                     kMENU_SEP,    -1,  9 },
   { "release-notes", "Release Notes",        kMENU_ITEM,   -1, 32 },
   { "update",        "Check for Updates",    kMENU_ITEM,   -1, 32 },
   { "",              "",                     kMENU_SEP,    -1,  9 },
   { "exit",          "Exit",                 kMENU_ITEM,   -1, 32 },
};

class CHROME_RML::Impl : public Rml::EventListener
{
public:
   static inline int s_nInstanceCount = 0;

   // Dispatch target for the always-on-top menu popup window. The chrome's own
   // Impl already implements Rml::EventListener for the chrome window; the menu
   // popup is a separate SDL window with its own ID, so it needs a distinct
   // ISDLWINDOW to route SDL events through the registry.
   class MENU_WINDOW : public ISDLWINDOW
   {
   public:
      MENU_WINDOW (Impl* pImpl) :
         m_pImpl (pImpl)
      {
      }

      SDL_WindowID SDLWindowID () const override
      {
         return m_pImpl->m_nMenuSDLWindowID;
      }

      void HandleEvent (SDL_Event& Event) override
      {
         m_pImpl->HandleMenuPopupEvent (Event);
      }

   private:
      Impl* m_pImpl;
   };

   // The URL history dropdown is a second always-on-top popup window, distinct
   // from both the chrome window and the ellipsis menu, so it routes through its
   // own ISDLWINDOW like the menu does.
   class URL_WINDOW : public ISDLWINDOW
   {
   public:
      URL_WINDOW (Impl* pImpl) :
         m_pImpl (pImpl)
      {
      }

      SDL_WindowID SDLWindowID () const override
      {
         return m_pImpl->m_nUrlSDLWindowID;
      }

      void HandleEvent (SDL_Event& Event) override
      {
         m_pImpl->HandleUrlPopupEvent (Event);
      }

   private:
      Impl* m_pImpl;
   };

   Impl () :
      m_pSDLWindow         (nullptr),
      m_pSDLRenderer       (nullptr),
      m_pRmlRenderer       (nullptr),
      m_pRmlContext        (nullptr),
      m_pRmlDocument       (nullptr),
      m_pTabStrip          (nullptr),
      m_pCaption           (nullptr),
      m_pMacZoom           (nullptr),
      m_pWndMax            (nullptr),
      m_pMenuBtn           (nullptr),
      m_pUrlBar            (nullptr),
      m_pUrlWrap           (nullptr),
      m_pUrlDrop           (nullptr),
      m_pUrlWindow         (nullptr),
      m_pUrlRenderer       (nullptr),
      m_pUrlRmlRenderer    (nullptr),
      m_pUrlContext        (nullptr),
      m_pUrlDocument       (nullptr),
      m_pUrlRoot           (nullptr),
      m_pUrlDispatch       (nullptr),
      m_nUrlSDLWindowID    (0),
      m_bUrlOpen           (false),
      m_pMenuWindow        (nullptr),
      m_pMenuRenderer      (nullptr),
      m_pMenuRmlRenderer   (nullptr),
      m_pMenuContext       (nullptr),
      m_pMenuDocument      (nullptr),
      m_pMenuRoot          (nullptr),
      m_pMenuDispatch      (nullptr),
      m_nSDLWindowID       (0),
      m_nMenuSDLWindowID   (0),
      m_pHost              (nullptr),
      m_bMenuOpen          (false),
      m_bModalBlocked      (false),
      m_bMacFullscreen     (false),
      m_nMacTopInset       (0),
      m_bWndMaximized      (false),
      m_RestoreRect        {0, 0, 0, 0},
      m_sTabLogoPath       ()
   {
   }

   ~Impl ()
   {
      SetPopupMenuOpen (false);
      SetUrlDropOpen (false);

      if (m_pUrlDispatch)
      {
         APPNATIVE::GetInstance ()->SDLWindow_Unregister (m_pUrlDispatch);
         delete m_pUrlDispatch;
         m_pUrlDispatch = nullptr;
      }

      if (m_pUrlContext)
      {
         Rml::RemoveContext (m_pUrlContext->GetName ());
         m_pUrlContext = nullptr;
      }

      if (m_pUrlRmlRenderer)
      {
         delete m_pUrlRmlRenderer;
         m_pUrlRmlRenderer = nullptr;
      }

      if (m_pUrlRenderer)
      {
         SDL_DestroyRenderer (m_pUrlRenderer);
         m_pUrlRenderer = nullptr;
      }

      if (m_pUrlWindow)
      {
         SDL_DestroyWindow (m_pUrlWindow);
         m_pUrlWindow = nullptr;
      }

      if (m_pMenuDispatch)
      {
         APPNATIVE::GetInstance ()->SDLWindow_Unregister (m_pMenuDispatch);
         delete m_pMenuDispatch;
         m_pMenuDispatch = nullptr;
      }

      if (m_pMenuContext)
      {
         Rml::RemoveContext (m_pMenuContext->GetName ());
         m_pMenuContext = nullptr;
      }

      if (m_pMenuRmlRenderer)
      {
         delete m_pMenuRmlRenderer;
         m_pMenuRmlRenderer = nullptr;
      }

      if (m_pMenuRenderer)
      {
         SDL_DestroyRenderer (m_pMenuRenderer);
         m_pMenuRenderer = nullptr;
      }

      if (m_pMenuWindow)
      {
         SDL_DestroyWindow (m_pMenuWindow);
         m_pMenuWindow = nullptr;
      }

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

   // Load the deployed logo PNG and apply it as the SDL window icon. Best-effort
   // -- a missing file or decode failure simply leaves the default WM icon. SDL
   // copies the surface internally, so it is safe to free everything afterwards.
   void SetWindowIcon ()
   {
      LOGGER*     pLogger   = APPNATIVE::GetInstance ()->Logger ();
      const char* pBasePath = SDL_GetBasePath ();

      if (pBasePath  &&  m_pSDLWindow)
      {
         std::string sPath = std::string (pBasePath) + "images/logo.png";

         size_t nSize = 0;
         void*  pData = SDL_LoadFile (sPath.c_str (), &nSize);

         if (pData)
         {
            std::vector<uint8_t> aEncoded (static_cast<uint8_t*> (pData),
                                           static_cast<uint8_t*> (pData) + nSize);
            SDL_free (pData);

            int                  nWidth  = 0;
            int                  nHeight = 0;
            std::vector<uint8_t> aPixels;

            if (SNEEZE::IMAGE::Decode (aEncoded, nWidth, nHeight, aPixels))
            {
               SDL_Surface* pSurface = SDL_CreateSurfaceFrom (nWidth, nHeight, SDL_PIXELFORMAT_RGBA32,
                                                              aPixels.data (), nWidth * 4);

               if (pSurface)
               {
                  bool bOk = SDL_SetWindowIcon (m_pSDLWindow, pSurface);
                  SDL_DestroySurface (pSurface);

                  if (!bOk  &&  pLogger)
                  {
                     const char* pDriver = SDL_GetCurrentVideoDriver ();
                     pLogger->Log (LOGGER::kLOGLEVEL_Warning, "Chrome",
                        std::string ("SDL_SetWindowIcon failed (driver=") + (pDriver ? pDriver : "?") +
                        "): " + SDL_GetError ());
                  }
               }
            }
            else if (pLogger)
            {
               pLogger->Log (LOGGER::kLOGLEVEL_Warning, "Chrome", "Window icon: PNG decode failed");
            }
         }
         else if (pLogger)
         {
            pLogger->Log (LOGGER::kLOGLEVEL_Warning, "Chrome",
               std::string ("Window icon: could not load ") + sPath + " : " + SDL_GetError ());
         }
      }
   }

   bool Initialize (const char* sTitle, int nWidth, int nHeight, ICHROME_HOST* pHost)
   {
      bool bResult = false;

      m_pHost = pHost;

      // Pull the tab strip up into the title-bar band, matching native Chrome.
      //   macOS: keep a real (titled) window so the native frame still handles
      //          resize; MacChrome_ConfigureTitleBar then makes the title bar
      //          transparent + full-size-content and hides the native traffic
      //          lights. Rubidium draws its own traffic lights into the strip so
      //          they stay overlaid in fullscreen with no reserved band.
      //   Linux: borderless (no OS title bar); window controls are custom
      //          buttons drawn into the strip and resize edges come from the
      //          hit test below.
#if defined(RUBIDIUM_PLATFORM_MACOS)
      Uint32 nWindowFlags = SDL_WINDOW_RESIZABLE;
#else
      Uint32 nWindowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS;
#endif

      m_pSDLWindow = SDL_CreateWindow (sTitle, nWidth, nHeight, nWindowFlags);

      if (m_pSDLWindow)
      {
         m_nSDLWindowID = SDL_GetWindowID (m_pSDLWindow);

         // Window / taskbar icon (the Win32 build embeds this in the .exe via
         // rubidium.rc; SDL platforms must set it explicitly from the deployed
         // logo PNG so the WM shows the Rubidium mark instead of a generic icon).
         SetWindowIcon ();
         m_sTabLogoPath = TabLogoPath ();

         if (m_sTabLogoPath.empty ())
         {
            LOGGER* pLogger = APPNATIVE::GetInstance ()->Logger ();
            if (pLogger)
               pLogger->Log (LOGGER::kLOGLEVEL_Warning, "Chrome",
                  "Tab favicon: images/logo.png not found under SDL_GetBasePath()");
         }

#if defined(RUBIDIUM_PLATFORM_MACOS)
         MacChrome_ConfigureTitleBar (m_pSDLWindow);
#endif

         // Designate the bare tab-strip background as the window-drag region
         // (and, on Linux, the outer edges as resize handles -- a borderless
         // window has neither unless we supply a hit test).
         SDL_SetWindowHitTest (m_pSDLWindow, HitTestCallback, this);

         m_pSDLRenderer = SDL_CreateRenderer (m_pSDLWindow, nullptr);

         if (m_pSDLRenderer)
         {
            SDL_SetRenderVSync (m_pSDLRenderer, 1);

            m_pRmlRenderer = new RenderInterface_SDL (m_pSDLRenderer);
            m_pRmlRenderer->SetClearColor (0xde, 0xe1, 0xe6);

            std::string sContextName = "chrome_rml_" + std::to_string (s_nInstanceCount++);
            m_pRmlContext = Rml::CreateContext (sContextName, Rml::Vector2i (nWidth, nHeight), m_pRmlRenderer);

            if (m_pRmlContext)
            {
               float dDisplayScale = SDL_GetWindowDisplayScale (m_pSDLWindow);
               m_pRmlContext->SetDensityIndependentPixelRatio (dDisplayScale);

               std::string sDocument = s_szChromeDocument;
               std::string sStyle    = s_szChromeStyle;

               std::string sFontFamily = APPNATIVE::GetInstance ()->sFontFamily ();
               Replace (sStyle, "[{FONT-FAMILY}]", sFontFamily);

               // Window-control placement is platform-native:
                  //   macOS: Chrome-style traffic lights drawn by RmlUi at the
                  //          top-left (the native ones are hidden -- see
                  //          MacChrome.mm -- so they stay overlaid in fullscreen
                  //          with no reserved title-bar band). The strip is
                  //          padded left to clear them.
                  //   Linux: Chrome-style custom buttons on the right; the strip
                  //          is padded right to clear them.
#if defined(RUBIDIUM_PLATFORM_MACOS)
               Replace (sStyle, "[{CAPTION-POS}]",  "left: 12dp;");
               Replace (sStyle, "[{TABSTRIP-PAD}]", "4dp 6dp 0dp 80dp");
#else
               Replace (sStyle, "[{CAPTION-POS}]",  "right: 6dp;");
               Replace (sStyle, "[{TABSTRIP-PAD}]", "4dp 96dp 0dp 6dp");
#endif

               Replace (sDocument, "[{STYLE}]", sStyle);

               m_pRmlDocument = m_pRmlContext->LoadDocumentFromMemory (sDocument);

               if (m_pRmlDocument)
               {
                  m_pRmlDocument->Show ();

                  m_pTabStrip  = m_pRmlDocument->GetElementById ("tabstrip");
                  m_pCaption   = m_pRmlDocument->GetElementById ("caption");
                  m_pMenuBtn   = m_pRmlDocument->GetElementById ("menubtn");
                  m_pUrlBar    = m_pRmlDocument->GetElementById ("urlbar");
                  m_pUrlWrap   = m_pRmlDocument->GetElementById ("urlwrap");
                  m_pUrlDrop   = m_pRmlDocument->GetElementById ("urldrop");

                  if (m_pTabStrip)
                     m_pTabStrip->AddEventListener (Rml::EventId::Click, this);

                  // Window controls are drawn into #caption on every platform.
                  //   macOS: red / yellow / green traffic lights at the left
                  //          (close / minimize / fullscreen). Glyphs (an X, a
                  //          minus, the fullscreen arrows) appear on group hover,
                  //          matching native behavior. The green light's glyph
                  //          swaps to the collapse arrows while in fullscreen
                  //          (see HandleEvent). The native buttons are hidden.
                  //   Linux: Chrome-style min / max / close buttons on the right.
#if defined(RUBIDIUM_PLATFORM_MACOS)
                  if (m_pCaption)
                  {
                     m_pCaption->SetInnerRML (
                        "<span class=\"mac-light mac-close\" action=\"wnd-close\">&#xE5CD;</span>"
                        "<span class=\"mac-light mac-min\" action=\"wnd-min\">&#xE15B;</span>"
                        "<span class=\"mac-light mac-zoom\" id=\"mac-zoom\" action=\"wnd-fullscreen\">&#xF1CE;</span>");
                     m_pCaption->AddEventListener (Rml::EventId::Click, this);

                     m_pMacZoom = m_pRmlDocument->GetElementById ("mac-zoom");
                  }
#else
                  if (m_pCaption)
                  {
                     // Chrome-on-Linux window controls: minimize (a centered
                     // line), maximize (a square -- swapped to the overlapping
                     // "restore" squares when maximized, see ToggleMaximize),
                     // and close (an X).
                     m_pCaption->SetInnerRML (
                        "<span class=\"cap-btn\" action=\"wnd-min\">&#xE15B;</span>"
                        "<span class=\"cap-btn\" id=\"cap-max\" action=\"wnd-max\">&#xE3C6;</span>"
                        "<span class=\"cap-btn cap-close\" action=\"wnd-close\">&#xE5CD;</span>");
                     m_pCaption->AddEventListener (Rml::EventId::Click, this);

                     m_pWndMax = m_pRmlDocument->GetElementById ("cap-max");
                  }
#endif

                  if (m_pMenuBtn)
                     m_pMenuBtn->AddEventListener (Rml::EventId::Click, this);

                  if (m_pUrlBar)
                  {
                     m_pUrlBar->AddEventListener (Rml::EventId::Keydown, this);
                     m_pUrlBar->AddEventListener (Rml::EventId::Click, this);

                     // Sneeze owns the global RmlUi system interface, so focusing
                     // a text field here does not auto-start SDL text input --
                     // drive it from the address bar's own focus instead. It must
                     // stay OFF while the bar is unfocused: the canvas suppresses
                     // its movement keys whenever the focused window is taking
                     // text input, so leaving it on permanently kills WASD.
                     m_pUrlBar->AddEventListener (Rml::EventId::Focus, this);
                     m_pUrlBar->AddEventListener (Rml::EventId::Blur, this);
                  }

                  if (m_pUrlDrop)
                     m_pUrlDrop->AddEventListener (Rml::EventId::Click, this);

                  bResult = true;
                  InitializeMenuPopup ();
                  InitializeUrlPopup ();
               }
            }
         }
      }

      return bResult;
   }

   static int MenuContentHeightDp ()
   {
      int nTotal = 0;

      for (const MENU_ENTRY& Entry : s_aMenuEntries)
         nTotal += Entry.nHeightDp;

      // body padding (5dp top + 5dp bottom).
      nTotal += 10;

      return nTotal;
   }

   std::string BuildMenuRml (int nActiveLevel) const
   {
      std::string sRml;

      for (const MENU_ENTRY& Entry : s_aMenuEntries)
      {
         switch (Entry.eType)
         {
            case kMENU_SEP:
               sRml += "<div class=\"menu-sep\"></div>";
               break;

            case kMENU_HEADER:
               sRml += "<div class=\"menu-header\">";
               sRml += Entry.sLabel;
               sRml += "</div>";
               break;

            case kMENU_RADIO:
               sRml += "<div class=\"menu-radio\" action=\"";
               sRml += Entry.sAction;
               sRml += "\" level=\"";
               sRml += std::to_string (Entry.nLevel);
               sRml += "\"><span class=\"menu-check\">";
               if (Entry.nLevel == nActiveLevel)
                  sRml += "&#xE5CA;";
               sRml += "</span>";
               sRml += Entry.sLabel;
               sRml += "</div>";
               break;

            case kMENU_ITEM:
            default:
               sRml += "<div class=\"menu-item\" action=\"";
               sRml += Entry.sAction;
               sRml += "\">";
               sRml += Entry.sLabel;
               sRml += "</div>";
               break;
         }
      }

      return sRml;
   }

   void RefreshMenuItems ()
   {
      if (m_pMenuRoot)
      {
         int nActiveLevel = m_pHost ? m_pHost->Chrome_LogLevel () : -1;
         m_pMenuRoot->SetInnerRML (BuildMenuRml (nActiveLevel));
      }
   }

   bool InitializeMenuPopup ()
   {
      bool bResult = false;

      float dDisplayScale = SDL_GetWindowDisplayScale (m_pSDLWindow);
      int   nMenuW        = static_cast<int> (kCHROME_MENU_W_DP * dDisplayScale + 0.5f);
      int   nMenuH        = static_cast<int> (MenuContentHeightDp () * dDisplayScale + 0.5f) + 2;

      // A popup window is positioned relative to its parent's client area and
      // is not managed by the window manager (no WM centering / decoration),
      // which is exactly what a dropdown needs. The offset is re-applied via
      // SDL_SetWindowPosition (also parent-relative for popups) before showing.
      m_pMenuWindow = SDL_CreatePopupWindow (m_pSDLWindow, 0, 0, nMenuW, nMenuH,
         SDL_WINDOW_POPUP_MENU | SDL_WINDOW_HIDDEN);

      if (m_pMenuWindow)
      {
         m_nMenuSDLWindowID = SDL_GetWindowID (m_pMenuWindow);

         // SDL_WINDOW_HIDDEN is not reliably honored for popup/child windows on
         // macOS -- Cocoa orders the child front together with its parent. Force
         // it hidden so the menu only ever appears via SetPopupMenuOpen().
         SDL_HideWindow (m_pMenuWindow);

         m_pMenuRenderer = SDL_CreateRenderer (m_pMenuWindow, nullptr);

         if (m_pMenuRenderer)
         {
            SDL_SetRenderVSync (m_pMenuRenderer, 1);

            m_pMenuRmlRenderer = new RenderInterface_SDL (m_pMenuRenderer);
            m_pMenuRmlRenderer->SetClearColor (0xff, 0xff, 0xff);

            std::string sContextName = "chrome_menu_" + std::to_string (s_nInstanceCount);
            m_pMenuContext = Rml::CreateContext (sContextName, Rml::Vector2i (nMenuW, nMenuH), m_pMenuRmlRenderer);

            if (m_pMenuContext)
            {
               m_pMenuContext->SetDensityIndependentPixelRatio (dDisplayScale);

               std::string sDocument = s_szMenuDocument;
               std::string sStyle    = s_szMenuStyle;

               std::string sFontFamily = APPNATIVE::GetInstance ()->sFontFamily ();
               Replace (sStyle, "[{FONT-FAMILY}]", sFontFamily);
               Replace (sDocument, "[{STYLE}]", sStyle);

               m_pMenuDocument = m_pMenuContext->LoadDocumentFromMemory (sDocument);

               if (m_pMenuDocument)
               {
                  m_pMenuDocument->Show ();
                  m_pMenuDocument->AddEventListener (Rml::EventId::Click, this);

                  m_pMenuRoot = m_pMenuDocument->GetElementById ("menu-root");
                  RefreshMenuItems ();

                  m_pMenuDispatch = new MENU_WINDOW (this);
                  APPNATIVE::GetInstance ()->SDLWindow_Register (m_pMenuDispatch);

                  bResult = true;
               }
            }
         }
      }

      return bResult;
   }

   bool InitializeUrlPopup ()
   {
      bool bResult = false;

      // Created at a nominal size; the real width/height/position are computed
      // from the address bar each time the dropdown is opened.
      m_pUrlWindow = SDL_CreatePopupWindow (m_pSDLWindow, 0, 0, 200, 200,
         SDL_WINDOW_POPUP_MENU | SDL_WINDOW_HIDDEN);

      if (m_pUrlWindow)
      {
         m_nUrlSDLWindowID = SDL_GetWindowID (m_pUrlWindow);

         SDL_HideWindow (m_pUrlWindow);

         m_pUrlRenderer = SDL_CreateRenderer (m_pUrlWindow, nullptr);

         if (m_pUrlRenderer)
         {
            SDL_SetRenderVSync (m_pUrlRenderer, 1);

            m_pUrlRmlRenderer = new RenderInterface_SDL (m_pUrlRenderer);
            m_pUrlRmlRenderer->SetClearColor (0xff, 0xff, 0xff);

            float       dDisplayScale = SDL_GetWindowDisplayScale (m_pSDLWindow);
            std::string sContextName  = "chrome_url_" + std::to_string (s_nInstanceCount);
            m_pUrlContext = Rml::CreateContext (sContextName, Rml::Vector2i (200, 200), m_pUrlRmlRenderer);

            if (m_pUrlContext)
            {
               m_pUrlContext->SetDensityIndependentPixelRatio (dDisplayScale);

               std::string sDocument = s_szMenuDocument;
               std::string sStyle    = s_szMenuStyle;

               std::string sFontFamily = APPNATIVE::GetInstance ()->sFontFamily ();
               Replace (sStyle, "[{FONT-FAMILY}]", sFontFamily);
               Replace (sDocument, "[{STYLE}]", sStyle);

               m_pUrlDocument = m_pUrlContext->LoadDocumentFromMemory (sDocument);

               if (m_pUrlDocument)
               {
                  m_pUrlDocument->Show ();
                  m_pUrlDocument->AddEventListener (Rml::EventId::Click, this);

                  m_pUrlRoot = m_pUrlDocument->GetElementById ("menu-root");

                  m_pUrlDispatch = new URL_WINDOW (this);
                  APPNATIVE::GetInstance ()->SDLWindow_Register (m_pUrlDispatch);

                  bResult = true;
               }
            }
         }
      }

      return bResult;
   }

   // History row height (matches .menu-item) used to size the dropdown.
   static const int kURL_ROW_DP     = 32;
   static const int kURL_MAX_ROWS   = 8;

   std::string BuildUrlRml (const std::vector<std::string>& asUrl) const
   {
      std::string sRml;

      for (size_t nUrl = 0; nUrl < asUrl.size (); nUrl++)
      {
         sRml += "<div class=\"menu-item\" action=\"url\" urlix=\"";
         sRml += std::to_string (nUrl);
         sRml += "\">";
         sRml += UTILS::Escape (asUrl[nUrl]);
         sRml += "</div>";
      }

      return sRml;
   }

   void UpdateUrlPopupPosition ()
   {
      if (m_pUrlWindow  &&  m_pSDLWindow)
      {
         float dDisplayScale = SDL_GetWindowDisplayScale (m_pSDLWindow);
         int   nChromeH      = ChromeHeight ();
         int   nRows         = static_cast<int> (m_asUrlHistory.size ());

         if (nRows > kURL_MAX_ROWS)
            nRows = kURL_MAX_ROWS;

         int nRowH    = static_cast<int> (kURL_ROW_DP * dDisplayScale + 0.5f);
         int nPadding = static_cast<int> (10.0f * dDisplayScale + 0.5f);
         int nMenuH   = nRows * nRowH + nPadding + 2;

         // Align the dropdown to the address-bar pill. RmlUi element offsets are
         // in physical pixels, which match this window's SDL popup coordinates.
         Rml::Element* pAnchor = m_pUrlWrap ? m_pUrlWrap : m_pUrlBar;
         int nX = 0;
         int nW = 200;

         if (pAnchor)
         {
            Rml::Vector2f vOffset = pAnchor->GetAbsoluteOffset (Rml::BoxArea::Border);
            nX = static_cast<int> (vOffset.x + 0.5f);
            nW = static_cast<int> (pAnchor->GetBox ().GetSize (Rml::BoxArea::Border).x + 0.5f);
         }

         if (nW < 1)  nW = 1;

         SDL_SetWindowSize (m_pUrlWindow, nW, nMenuH);

         if (m_pUrlContext)
            m_pUrlContext->SetDimensions (Rml::Vector2i (nW, nMenuH));

         SDL_SetWindowPosition (m_pUrlWindow, nX, nChromeH);
      }
   }

   void RenderUrlPopup ()
   {
      if (m_pUrlContext  &&  m_pUrlRenderer  &&  m_pUrlRmlRenderer)
      {
         m_pUrlContext->Update ();

         m_pUrlRmlRenderer->BeginFrame ();
         m_pUrlContext->Render ();
         m_pUrlRmlRenderer->EndFrame ();

         SDL_RenderPresent (m_pUrlRenderer);
      }
   }

   void HandleUrlPopupEvent (SDL_Event& Event)
   {
      if (Event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
      {
         SetUrlDropOpen (false);
      }
      else if (m_pUrlContext)
      {
         RmlSDL::InputEventHandler (m_pUrlContext, m_pUrlWindow, Event);
         RenderUrlPopup ();
      }
   }

   void SetUrlDropOpen (bool bOpen)
   {
      if (m_bUrlOpen == bOpen)
         return;

      m_bUrlOpen = bOpen;

      if (m_pUrlWindow)
      {
         if (bOpen)
         {
            // Snapshot the history so row indices map to a stable list while open.
            m_asUrlHistory = m_pHost ? m_pHost->Chrome_UrlHistory () : std::vector<std::string> ();

            if (m_pUrlRoot)
               m_pUrlRoot->SetInnerRML (BuildUrlRml (m_asUrlHistory));

            UpdateUrlPopupPosition ();
            SDL_ShowWindow (m_pUrlWindow);
            SDL_RaiseWindow (m_pUrlWindow);
            RenderUrlPopup ();
         }
         else
         {
            SDL_HideWindow (m_pUrlWindow);
         }
      }

      Render ();
   }

   void UpdateMenuPopupPosition ()
   {
      if (m_pMenuWindow  &&  m_pSDLWindow)
      {
         float dDisplayScale = SDL_GetWindowDisplayScale (m_pSDLWindow);
         int   nMenuW        = static_cast<int> (kCHROME_MENU_W_DP * dDisplayScale + 0.5f);
         int   nMenuH        = static_cast<int> (MenuContentHeightDp () * dDisplayScale + 0.5f) + 2;
         int   nMargin       = static_cast<int> (8.0f * dDisplayScale + 0.5f);
         int   nChromeH      = ChromeHeight ();

         SDL_SetWindowSize (m_pMenuWindow, nMenuW, nMenuH);

         if (m_pMenuContext)
            m_pMenuContext->SetDimensions (Rml::Vector2i (nMenuW, nMenuH));

         // Drop down from just below the chrome strip, right-aligned under the
         // ellipsis button. Anchor to the button's own layout box (physical
         // pixels, matching this popup's parent-relative SDL coordinates) rather
         // than deriving X from SDL_GetWindowSize: the reported host size can lag
         // the actual surface when the window is maximized / fullscreen on some
         // Linux compositors, which parked the menu mid-screen. The button offset
         // stays correct in every window state -- the same approach the URL
         // dropdown uses.
         int nX = 0;

         if (m_pMenuBtn)
         {
            Rml::Vector2f vOffset   = m_pMenuBtn->GetAbsoluteOffset (Rml::BoxArea::Border);
            int           nBtnRight = static_cast<int> (
               vOffset.x + m_pMenuBtn->GetBox ().GetSize (Rml::BoxArea::Border).x + 0.5f);

            nX = nBtnRight - nMenuW;
         }
         else
         {
            int nHostW = 0;
            int nHostH = 0;

            SDL_GetWindowSize (m_pSDLWindow, &nHostW, &nHostH);
            nX = nHostW - nMenuW - nMargin;
         }

         if (nX < 0)
            nX = 0;

         SDL_SetWindowPosition (m_pMenuWindow, nX, nChromeH);
      }
   }

   void RenderMenuPopup ()
   {
      if (m_pMenuContext  &&  m_pMenuRenderer  &&  m_pMenuRmlRenderer)
      {
         m_pMenuContext->Update ();

         m_pMenuRmlRenderer->BeginFrame ();
         m_pMenuContext->Render ();
         m_pMenuRmlRenderer->EndFrame ();

         SDL_RenderPresent (m_pMenuRenderer);
      }
   }

   void HandleMenuPopupEvent (SDL_Event& Event)
   {
      if (Event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
      {
         SetPopupMenuOpen (false);
      }
      else if (m_pMenuContext)
      {
         RmlSDL::InputEventHandler (m_pMenuContext, m_pMenuWindow, Event);
         RenderMenuPopup ();
      }
   }

   SDL_WindowID MenuPopupSDLWindowID () const
   {
      return m_nMenuSDLWindowID;
   }

   void SetPopupMenuOpen (bool bOpen)
   {
      if (m_bMenuOpen == bOpen)
         return;

      m_bMenuOpen = bOpen;

      if (m_pMenuWindow)
      {
         if (bOpen)
         {
            RefreshMenuItems ();
            UpdateMenuPopupPosition ();
            SDL_ShowWindow (m_pMenuWindow);
            SDL_RaiseWindow (m_pMenuWindow);
            RenderMenuPopup ();
         }
         else
         {
            SDL_HideWindow (m_pMenuWindow);
         }
      }

      Render ();
   }

   void ProcessEvent (Rml::Event& Event) override
   {
      if (Event.GetId () == Rml::EventId::Click)
      {
         // A modal child window (Settings) is open: swallow every chrome click so
         // tabs, the caption close button, the ellipsis menu, etc. are inert --
         // the windowing-system modality is unreliable on some compositors.
         if (m_bModalBlocked)
            return;

         Rml::Element* pElement = Event.GetTargetElement ();

         while (pElement)
         {
            Rml::String sAttr = pElement->GetAttribute<Rml::String> ("action", "");
            if (!sAttr.empty ())
               break;
            pElement = pElement->GetParentNode ();
         }

         if (pElement  &&  m_pHost)
         {
            std::string sAction = std::string (pElement->GetAttribute<Rml::String> ("action", "").c_str ());
            int         nTabIx  = pElement->GetAttribute<int> ("tabix", -1);

            if (sAction == "menu")
            {
               SetUrlDropOpen (false);
               SetPopupMenuOpen (!m_bMenuOpen);
            }
            else if (sAction == "wnd-min")
            {
               SetPopupMenuOpen (false);
               SetUrlDropOpen (false);

               // A window in a macOS fullscreen Space cannot be minimized; the
               // yellow light is greyed out in that state (see HandleEvent), so
               // ignore the click rather than no-op'ing deep in SDL / AppKit.
               if (!m_bMacFullscreen)
                  SDL_MinimizeWindow (m_pSDLWindow);
            }
            else if (sAction == "wnd-max")
            {
               SetPopupMenuOpen (false);
               SetUrlDropOpen (false);
               ToggleMaximize ();
            }
            else if (sAction == "wnd-fullscreen")
            {
               SetPopupMenuOpen (false);
               SetUrlDropOpen (false);
#if defined(RUBIDIUM_PLATFORM_MACOS)
               // The green traffic light toggles the native fullscreen Space;
               // the glyph (expand vs. collapse arrows) is swapped from the
               // ENTER / LEAVE_FULLSCREEN events in HandleEvent.
               MacChrome_ToggleFullScreen (m_pSDLWindow);
#endif
            }
            else if (sAction == "wnd-close")
            {
               // Route through the existing close path (the event loop maps a
               // CLOSE_REQUESTED for this window to Window_OnDestroy of its
               // frame), so a borderless window closes exactly like the old
               // native title-bar button did.
               SDL_Event evClose;
               SDL_zero (evClose);
               evClose.type            = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
               evClose.window.windowID = m_nSDLWindowID;
               SDL_PushEvent (&evClose);
            }
            else if (sAction == "urldrop")
            {
               SetPopupMenuOpen (false);
               SetUrlDropOpen (!m_bUrlOpen);
            }
            else if (sAction == "url")
            {
               int nUrlIx = pElement->GetAttribute<int> ("urlix", -1);

               if (nUrlIx >= 0  &&  nUrlIx < static_cast<int> (m_asUrlHistory.size ()))
               {
                  const std::string& sUrl = m_asUrlHistory[nUrlIx];

                  if (m_pUrlBar)
                     m_pUrlBar->SetAttribute ("value", sUrl);

                  m_pHost->Chrome_OnUrlSubmit (sUrl);
               }

               SetUrlDropOpen (false);
            }
            else if (sAction == "exit")
            {
               SetPopupMenuOpen (false);
               m_pHost->Chrome_OnExit ();
            }
            else if (sAction == "new-window")
            {
               SetPopupMenuOpen (false);
               m_pHost->Chrome_OnNewWindow (SNEEZE::CONTEXT::kSESSION_PERSISTENT);
            }
            else if (sAction == "new-incognito")
            {
               SetPopupMenuOpen (false);
               m_pHost->Chrome_OnNewWindow (SNEEZE::CONTEXT::kSESSION_TRANSITORY);
            }
            else if (sAction == "inspector")
            {
               SetPopupMenuOpen (false);
               m_pHost->Chrome_OnToggleInspector ();
            }
            else if (sAction == "settings")
            {
               SetPopupMenuOpen (false);
               m_pHost->Chrome_OnSettings ();
            }
            else if (sAction == "release-notes")
            {
               SetPopupMenuOpen (false);
               m_pHost->Chrome_OnReleaseNotes ();
            }
            else if (sAction == "update")
            {
               SetPopupMenuOpen (false);
               m_pHost->Chrome_OnUpdate ();
            }
            else if (sAction == "loglevel")
            {
               int nLevel = pElement->GetAttribute<int> ("level", -1);
               if (nLevel >= 0)
                  m_pHost->Chrome_OnSetLogLevel (nLevel);
               SetPopupMenuOpen (false);
            }
            else if (sAction == "add")
            {
               SetPopupMenuOpen (false);
               m_ePendingTab = kPENDING_TAB_ADD;
            }
            else if (sAction == "close"  &&  nTabIx >= 0)
            {
               SetPopupMenuOpen (false);
               m_ePendingTab   = kPENDING_TAB_CLOSE;
               m_nPendingTabIx = nTabIx;
            }
            else if (sAction == "select"  &&  nTabIx >= 0)
            {
               SetPopupMenuOpen (false);
               m_ePendingTab   = kPENDING_TAB_SELECT;
               m_nPendingTabIx = nTabIx;
            }
            else if (m_bMenuOpen)
               SetPopupMenuOpen (false);
         }
         else if (m_bMenuOpen)
         {
            SetPopupMenuOpen (false);
         }
      }
      else if (Event.GetId () == Rml::EventId::Keydown)
      {
         Rml::Input::KeyIdentifier eKey = static_cast<Rml::Input::KeyIdentifier> (
            Event.GetParameter<int> ("key_identifier", Rml::Input::KI_UNKNOWN));

         if ((eKey == Rml::Input::KI_RETURN  ||  eKey == Rml::Input::KI_NUMPADENTER)  &&  m_pUrlBar  &&  m_pHost)
         {
            SetUrlDropOpen (false);

            Rml::String sValue = m_pUrlBar->GetAttribute<Rml::String> ("value", "");
            m_pHost->Chrome_OnUrlSubmit (std::string (sValue.c_str ()));

            // Submitting hands the keyboard back to the viewport, matching the
            // Win32 frame -- otherwise the bar keeps capturing WASD afterwards.
            BlurTextEntry ();
         }
      }
      else if (Event.GetId () == Rml::EventId::Focus)
      {
         SDL_StartTextInput (m_pSDLWindow);
      }
      else if (Event.GetId () == Rml::EventId::Blur)
      {
         SDL_StopTextInput (m_pSDLWindow);
      }
   }

   // True while the address bar owns RmlUi keyboard focus. The focus leaf can be
   // an element the text control builds inside itself, so walk up to the bar
   // rather than comparing the leaf directly.
   bool IsUrlBarFocused () const
   {
      bool bResult = false;

      if (m_pUrlBar  &&  m_pRmlDocument)
      {
         for (Rml::Element* pElement = m_pRmlDocument->GetFocusLeafNode ();
              pElement  &&  !bResult;
              pElement = pElement->GetParentNode ())
            bResult = (pElement == m_pUrlBar);
      }

      return bResult;
   }

   void BlurTextEntry ()
   {
      // Blur fires the listener above, which ends text input. Stop it here too
      // so focus already sitting elsewhere still releases the keyboard.
      if (IsUrlBarFocused ())
         m_pUrlBar->Blur ();

      SDL_StopTextInput (m_pSDLWindow);
   }

   void SetTabs (const std::vector<std::string>& asTitle, int nActive)
   {
      if (m_pTabStrip)
      {
         std::string sRml;

         for (size_t nTab = 0; nTab < asTitle.size (); nTab++)
         {
            std::string sTitle = UTILS::Escape (asTitle[nTab]);
            std::string sIx    = std::to_string (nTab);
            std::string sClass = (static_cast<int> (nTab) == nActive) ? "tab active" : "tab";

            sRml += "<div class=\"" + sClass + "\" action=\"select\" tabix=\"" + sIx + "\">";

            if (!m_sTabLogoPath.empty ())
            {
               sRml += "<img class=\"tab-favicon\" src=\"" + HtmlAttrPath (m_sTabLogoPath) +
                       "\" action=\"select\" tabix=\"" + sIx + "\"/>";
            }

            sRml += "<span class=\"tab-title\" action=\"select\" tabix=\"" + sIx + "\">" + sTitle + "</span>";
            sRml += "<span class=\"tab-close\" action=\"close\" tabix=\"" + sIx + "\">&#xE5CD;</span>";
            sRml += "</div>";
         }

         sRml += "<div class=\"tab-add\" action=\"add\">+</div>";

         m_pTabStrip->SetInnerRML (sRml);
      }

      Render ();
   }

   void SetUrl (const std::string& sUrl)
   {
      if (m_pUrlBar)
         m_pUrlBar->SetAttribute ("value", sUrl);
   }

   float DisplayScale () const
   {
      float dDisplayScale = SDL_GetWindowDisplayScale (m_pSDLWindow);

#if defined(RUBIDIUM_PLATFORM_MACOS)
      // Window display scale can transiently read 1.0 during hide/show or a
      // native fullscreen transition even on Retina. Fall back to the display's
      // content scale so chrome dp layout and canvas docking stay consistent.
      if (dDisplayScale < 1.01f)
      {
         SDL_DisplayID nDisplay = SDL_GetDisplayForWindow (m_pSDLWindow);

         if (nDisplay)
         {
            float dContentScale = SDL_GetDisplayContentScale (nDisplay);

            if (dContentScale > dDisplayScale)
               dDisplayScale = dContentScale;
         }
      }
#endif

      return dDisplayScale;
   }

   void SyncDisplayScale ()
   {
      if (!m_pRmlContext)
         return;

      float dDisplayScale = DisplayScale ();

      m_pRmlContext->SetDensityIndependentPixelRatio (dDisplayScale);

      if (m_pMenuContext)
         m_pMenuContext->SetDensityIndependentPixelRatio (dDisplayScale);

      if (m_pUrlContext)
         m_pUrlContext->SetDensityIndependentPixelRatio (dDisplayScale);
   }

   void Render ()
   {
      SyncDisplayScale ();

      int nWindowW = 0, nWindowH = 0;
      SDL_GetWindowSize (m_pSDLWindow, &nWindowW, &nWindowH);

      Rml::Vector2i Dimensions = m_pRmlContext->GetDimensions ();
      if (nWindowW != Dimensions.x  ||  nWindowH != Dimensions.y)
         m_pRmlContext->SetDimensions (Rml::Vector2i (nWindowW, nWindowH));

      m_pRmlContext->Update ();

      m_pRmlRenderer->BeginFrame ();
      m_pRmlContext->Render ();
      m_pRmlRenderer->EndFrame ();

      SDL_RenderPresent (m_pSDLRenderer);
   }

   int ChromeHeight () const
   {
      // The mac top inset (menu-bar push-down) is added so the docked canvas
      // starts below the shifted chrome strip -- ContentTop tracks it too.
      return static_cast<int> (kCHROME_TOTAL_DP * DisplayScale () + 0.5f) + m_nMacTopInset;
   }

#if defined(RUBIDIUM_PLATFORM_MACOS)
   // While the menu bar is revealed in a fullscreen Space, slide the whole RmlUi
   // chrome down by the occluded height (Chrome-style) so the tab strip AND the
   // traffic-light caption clear the menu bar, and re-dock the canvas below it.
   // The vacated strip above the chrome is the body background, which the
   // revealed menu bar sits over -- so no white/gray band ever shows. nInset is
   // in window points; the chrome context is authored in points, so px maps 1:1.
   // The tab strip / toolbar are in normal flow (moved by body padding-top); the
   // caption is position:absolute (padding does not affect it), so its top edge
   // is offset explicitly.
   void SetMacTopInset (int nInset)
   {
      m_nMacTopInset = nInset;

      std::string sInset = std::to_string (nInset) + "px";

      if (m_pRmlDocument)
         m_pRmlDocument->SetProperty ("padding-top", sInset);

      if (m_pCaption)
         m_pCaption->SetProperty ("top", sInset);

      Render ();
   }

   // Per-frame maintenance for the macOS fullscreen chrome, driven from
   // APPFRAME_SDL::ProcessInput (the system menu bar swallows mouse events while
   // the cursor is over it, so this cannot be event-driven). Keeps the empty
   // title-bar overlay suppressed and tracks the menu-bar reveal so the chrome
   // pushes down / springs back. Returns true when the content-top inset changed
   // this frame, so the caller re-docks the canvas at the new ContentTop.
   bool TickMacChrome ()
   {
      bool bChanged = false;

      if (m_bMacFullscreen)
      {
         MacChrome_SuppressFullScreenTitleBar (m_pSDLWindow);

         int nInset = MacChrome_FullScreenTopInset (m_pSDLWindow);

         if (nInset != m_nMacTopInset)
         {
            SetMacTopInset (nInset);
            bChanged = true;
         }
      }
      else if (m_nMacTopInset != 0)
      {
         SetMacTopInset (0);
         bChanged = true;
      }

      return bChanged;
   }
#endif

   // Toggle a Windows-style maximize for the borderless (non-macOS) window.
   // SDL_MaximizeWindow on an undecorated window is unreliable across
   // compositors (Wayland / WSLg resize it but leave it offset, so it never
   // covers the top of the screen). Instead, snap the window to the display's
   // usable bounds -- the work area minus panels / taskbar -- exactly like a
   // Windows maximize, and remember the previous geometry to restore to.
   void ToggleMaximize ()
   {
      if (m_bWndMaximized)
      {
         SDL_SetWindowSize     (m_pSDLWindow, m_RestoreRect.w, m_RestoreRect.h);
         SDL_SetWindowPosition (m_pSDLWindow, m_RestoreRect.x, m_RestoreRect.y);
         m_bWndMaximized = false;
      }
      else
      {
         SDL_GetWindowPosition (m_pSDLWindow, &m_RestoreRect.x, &m_RestoreRect.y);
         SDL_GetWindowSize     (m_pSDLWindow, &m_RestoreRect.w, &m_RestoreRect.h);

         SDL_Rect      Usable;
         SDL_DisplayID nDisplay = SDL_GetDisplayForWindow (m_pSDLWindow);

         if (SDL_GetDisplayUsableBounds (nDisplay, &Usable))
         {
            SDL_SetWindowPosition (m_pSDLWindow, Usable.x, Usable.y);
            SDL_SetWindowSize     (m_pSDLWindow, Usable.w, Usable.h);
            m_bWndMaximized = true;
         }
      }

      // Mirror Chrome: a plain square when restoring is available, the two
      // overlapping squares ("restore") while maximized.
      if (m_pWndMax)
         m_pWndMax->SetInnerRML (m_bWndMaximized ? "&#xE3E0;" : "&#xE3C6;");
   }

   // SDL hit test for the chrome window. The bare tab-strip background (no tab /
   // button under the cursor) is the window-drag region on every platform. On
   // Linux (borderless) the outer few pixels are additionally resize handles;
   // macOS keeps its native titled frame and resizes itself. Everything else
   // stays NORMAL so RmlUi receives the click. Coordinates are in window points,
   // which equal the RmlUi context units for this non-HiDPI window.
   static SDL_HitTestResult SDLCALL HitTestCallback (SDL_Window* pWindow, const SDL_Point* pArea, void* pData)
   {
      (void) pWindow;
      return static_cast<Impl*> (pData)->HitTest (pArea);
   }

   SDL_HitTestResult HitTest (const SDL_Point* pArea)
   {
      SDL_HitTestResult eResult = SDL_HITTEST_NORMAL;

      float dScale   = DisplayScale ();
      int   nTabBarH = static_cast<int> (kCHROME_TABBAR_DP * dScale + 0.5f);

      bool bEdge = false;

#if !defined(RUBIDIUM_PLATFORM_MACOS)
      // Borderless (Linux): the window has no native frame, so the outer few
      // pixels act as resize handles. macOS keeps its native titled frame and
      // resizes itself, so it skips this and only needs the drag region.
      int nWindowW = 0, nWindowH = 0;
      SDL_GetWindowSize (m_pSDLWindow, &nWindowW, &nWindowH);

      const int nBorder    = 5;
      bool      bMaximized = m_bWndMaximized;

      bool bLeft   = pArea->x < nBorder;
      bool bRight  = pArea->x >= nWindowW - nBorder;
      bool bTop    = pArea->y < nBorder;
      bool bBottom = pArea->y >= nWindowH - nBorder;

      if (!bMaximized  &&  (bLeft || bRight || bTop || bBottom))
      {
         bEdge = true;

         if      (bTop    &&  bLeft)  eResult = SDL_HITTEST_RESIZE_TOPLEFT;
         else if (bTop    &&  bRight) eResult = SDL_HITTEST_RESIZE_TOPRIGHT;
         else if (bBottom &&  bLeft)  eResult = SDL_HITTEST_RESIZE_BOTTOMLEFT;
         else if (bBottom &&  bRight) eResult = SDL_HITTEST_RESIZE_BOTTOMRIGHT;
         else if (bLeft)              eResult = SDL_HITTEST_RESIZE_LEFT;
         else if (bRight)             eResult = SDL_HITTEST_RESIZE_RIGHT;
         else if (bTop)               eResult = SDL_HITTEST_RESIZE_TOP;
         else                         eResult = SDL_HITTEST_RESIZE_BOTTOM;
      }
#endif

      if (!bEdge  &&  pArea->y < nTabBarH  &&  m_pRmlContext)
      {
         Rml::Element* pElement = m_pRmlContext->GetElementAtPoint (
            Rml::Vector2f (static_cast<float> (pArea->x), static_cast<float> (pArea->y)));

         // Only the strip itself (gap to the right of the tabs) is draggable;
         // tabs, the add button and caption buttons report as their own
         // elements and must stay clickable.
         if (!pElement  ||  pElement == m_pTabStrip)
            eResult = SDL_HITTEST_DRAGGABLE;
      }

      return eResult;
   }

   void HandleEvent (SDL_Event& Event)
   {
      if (Event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED
       || Event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
         SyncDisplayScale ();

#if defined(RUBIDIUM_PLATFORM_MACOS)
      // Native fullscreen toggles the NSWindow style mask, which makes AppKit
      // drop our transparent full-size-content title-bar attributes and un-hide
      // the native traffic-light buttons. Re-assert the configuration on each
      // transition so the content stays full-size under a transparent (empty)
      // title bar and the native buttons stay hidden -- our own RmlUi traffic
      // lights remain overlaid at the top-left with no reserved band. Also swap
      // the green light's glyph between open_in_full (F1CE, expand arrows,
      // windowed) and close_fullscreen (F1CF, collapse arrows, fullscreen) so
      // it matches the native green button.
      if (Event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN ||
          Event.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)
      {
         MacChrome_ConfigureTitleBar (m_pSDLWindow);
         SDL_SyncWindow (m_pSDLWindow);
         MacChrome_ConfigureTitleBar (m_pSDLWindow);
         SyncDisplayScale ();

         m_bMacFullscreen = (Event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN);

         if (m_pMacZoom)
            m_pMacZoom->SetInnerRML (m_bMacFullscreen ? "&#xF1CF;" : "&#xF1CE;");

         // Greys out the yellow minimize light in fullscreen: a window in a
         // fullscreen Space cannot be minimized (matching Chrome / native apps).
         // The wnd-min action is also ignored while fullscreen (see Click).
         if (m_pCaption)
            m_pCaption->SetClass ("fullscreen", m_bMacFullscreen);

         // Leaving fullscreen: drop any menu-bar push-down so the chrome sits
         // flush at the top of the restored window. Entering: TickMacChrome
         // (driven per-frame from ProcessInput) begins tracking the reveal.
         if (!m_bMacFullscreen)
            SetMacTopInset (0);
      }
#endif

      // macOS re-orders popup/child windows to the front whenever the parent is
      // shown or regains focus, even though the menu was created hidden. That is
      // what makes the menu flash at its (0,0) creation origin on launch, new
      // window, and tab activation. Keep it hidden unless it was explicitly
      // opened via SetPopupMenuOpen().
      if (Event.type == SDL_EVENT_WINDOW_SHOWN
       || Event.type == SDL_EVENT_WINDOW_FOCUS_GAINED
       || Event.type == SDL_EVENT_WINDOW_EXPOSED
       || Event.type == SDL_EVENT_WINDOW_RESTORED
       || Event.type == SDL_EVENT_WINDOW_MAXIMIZED)
      {
         if (!m_bMenuOpen  &&  m_pMenuWindow  &&  (SDL_GetWindowFlags (m_pMenuWindow) & SDL_WINDOW_HIDDEN) == 0)
            SDL_HideWindow (m_pMenuWindow);

         if (!m_bUrlOpen  &&  m_pUrlWindow  &&  (SDL_GetWindowFlags (m_pUrlWindow) & SDL_WINDOW_HIDDEN) == 0)
            SDL_HideWindow (m_pUrlWindow);
      }

      if (Event.type == SDL_EVENT_WINDOW_MOVED
       ||  Event.type == SDL_EVENT_WINDOW_RESIZED
       ||  Event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
      {
         if (m_bMenuOpen)
         {
            UpdateMenuPopupPosition ();
            RenderMenuPopup ();
         }

         if (m_bUrlOpen)
         {
            UpdateUrlPopupPosition ();
            RenderUrlPopup ();
         }
      }

      // SDL3 text input and key events are delivered to the window with keyboard
      // focus. The embedded Filament canvas keeps focus after tab activate, so
      // typed/pasted characters never reach this chrome context unless we reclaim
      // focus when the user interacts with the chrome strip. SDL3 removed
      // SDL_SetWindowInputFocus; SDL_RaiseWindow is the supported way to request
      // keyboard focus from the window manager. Text input itself is NOT started
      // here -- it follows the address bar's focus (see the Focus/Blur listeners),
      // because the canvas mutes its movement keys while the focused window is
      // taking text input.
      if (Event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
      {
         SDL_RaiseWindow (m_pSDLWindow);
      }
      else if (Event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
      {
         if (IsUrlBarFocused ())
            SDL_StartTextInput (m_pSDLWindow);
      }
      else if (Event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
      {
         SDL_StopTextInput (m_pSDLWindow);
      }

      RmlSDL::InputEventHandler (m_pRmlContext, m_pSDLWindow, Event);

      // Execute any tab action recorded during the dispatch above, now that
      // RmlUi has finished propagating the click. This can rebuild the tab-strip
      // DOM and, on a last-tab close, delete this chrome -- so a close returns
      // immediately and touches no members afterward.
      if (m_ePendingTab != kPENDING_TAB_NONE)
      {
         ePENDING_TAB ePending = m_ePendingTab;
         int          nTabIx   = m_nPendingTabIx;

         m_ePendingTab   = kPENDING_TAB_NONE;
         m_nPendingTabIx = -1;

         switch (ePending)
         {
            case kPENDING_TAB_ADD:    m_pHost->Chrome_OnTabAdd ();          break;
            case kPENDING_TAB_SELECT: m_pHost->Chrome_OnTabSelect (nTabIx); break;
            case kPENDING_TAB_CLOSE:  m_pHost->Chrome_OnTabClose (nTabIx);  return;   // may delete this
            default:                                                        break;
         }
      }

      Render ();
   }

   void* NativeParentHandle () const
   {
      SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pSDLWindow);
      Uint64 nX11 = SDL_GetNumberProperty (nProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
      return reinterpret_cast<void*> (static_cast<uintptr_t> (nX11));
   }

   static void Replace (std::string& sText, const std::string& sFind, const std::string& sReplace)
   {
      std::string::size_type nPos = sText.find (sFind);
      if (nPos != std::string::npos)
         sText.replace (nPos, sFind.length (), sReplace);
   }

   SDL_Window*           m_pSDLWindow;
   SDL_Renderer*         m_pSDLRenderer;
   RenderInterface_SDL*  m_pRmlRenderer;
   Rml::Context*         m_pRmlContext;
   Rml::ElementDocument* m_pRmlDocument;
   Rml::Element*         m_pTabStrip;
   Rml::Element*         m_pCaption;
   Rml::Element*         m_pMacZoom;   // macOS green traffic light (glyph swaps in fullscreen)
   Rml::Element*         m_pWndMax;    // non-macOS maximize button (glyph swaps maximize/restore)
   Rml::Element*         m_pMenuBtn;
   Rml::Element*         m_pUrlBar;
   Rml::Element*         m_pUrlWrap;
   Rml::Element*         m_pUrlDrop;
   SDL_Window*           m_pUrlWindow;
   SDL_Renderer*         m_pUrlRenderer;
   RenderInterface_SDL*  m_pUrlRmlRenderer;
   Rml::Context*         m_pUrlContext;
   Rml::ElementDocument* m_pUrlDocument;
   Rml::Element*         m_pUrlRoot;
   URL_WINDOW*           m_pUrlDispatch;
   SDL_WindowID          m_nUrlSDLWindowID;
   bool                  m_bUrlOpen;
   std::vector<std::string> m_asUrlHistory;   // snapshot while the dropdown is open
   SDL_Window*           m_pMenuWindow;
   SDL_Renderer*         m_pMenuRenderer;
   RenderInterface_SDL*  m_pMenuRmlRenderer;
   Rml::Context*         m_pMenuContext;
   Rml::ElementDocument* m_pMenuDocument;
   Rml::Element*         m_pMenuRoot;
   MENU_WINDOW*          m_pMenuDispatch;
   SDL_WindowID          m_nSDLWindowID;
   SDL_WindowID          m_nMenuSDLWindowID;
   ICHROME_HOST*         m_pHost;
   bool                  m_bMenuOpen;
   bool                  m_bModalBlocked;    // a modal child (Settings) is open -- ignore all chrome clicks
   bool                  m_bMacFullscreen;   // macOS: window is in a fullscreen Space (minimize disabled)
   int                   m_nMacTopInset;     // macOS: points the chrome is pushed down while the menu bar is revealed in fullscreen
   bool                  m_bWndMaximized;    // non-macOS: tracks our manual maximize state
   SDL_Rect              m_RestoreRect;      // non-macOS: geometry to restore to when un-maximizing
   std::string           m_sTabLogoPath;     // deployed logo.png path for tab favicons (RmlUi img src)

   // Deferred tab actions. Closing / selecting / adding a tab rebuilds the
   // tab-strip DOM (SetInnerRML) and, on a last-tab close, tears down this whole
   // chrome. Doing that inside ProcessEvent -- which runs during RmlUi's event
   // dispatch -- frees the element whose click is still bubbling (use-after-free
   // crash). ProcessEvent only records the request here; HandleEvent executes it
   // after RmlSDL::InputEventHandler returns, outside the dispatch.
   enum ePENDING_TAB { kPENDING_TAB_NONE = 0, kPENDING_TAB_ADD, kPENDING_TAB_SELECT, kPENDING_TAB_CLOSE };
   ePENDING_TAB          m_ePendingTab   = kPENDING_TAB_NONE;
   int                   m_nPendingTabIx = -1;
};

/*******************************************************************************************************************************
**                                                    CHROME_RML                                                             **
*******************************************************************************************************************************/

CHROME_RML::CHROME_RML () :
   m_pImpl (new Impl ())
{
}

CHROME_RML::~CHROME_RML ()
{
   APPNATIVE::GetInstance ()->SDLWindow_Unregister (this);

   delete m_pImpl;
   m_pImpl = nullptr;
}

bool CHROME_RML::Initialize (const char* sTitle, int nWidth, int nHeight, ICHROME_HOST* pHost)
{
   bool bResult = m_pImpl->Initialize (sTitle, nWidth, nHeight, pHost);

   if (bResult)
      APPNATIVE::GetInstance ()->SDLWindow_Register (this);

   return bResult;
}

void CHROME_RML::SetTabs (const std::vector<std::string>& asTitle, int nActive) { m_pImpl->SetTabs (asTitle, nActive); }
void CHROME_RML::SetUrl  (const std::string& sUrl)                              { m_pImpl->SetUrl (sUrl); }
bool CHROME_RML::IsUrlBarFocused () const                                       { return m_pImpl->IsUrlBarFocused (); }
void CHROME_RML::BlurTextEntry ()                                               { m_pImpl->BlurTextEntry (); }
void CHROME_RML::Render  ()                                                     { m_pImpl->Render (); }
void CHROME_RML::SetModalBlocked (bool bBlocked)                                { m_pImpl->m_bModalBlocked = bBlocked; }
int  CHROME_RML::ChromeHeight () const                                          { return m_pImpl->ChromeHeight (); }

#if defined(RUBIDIUM_PLATFORM_MACOS)
bool CHROME_RML::TickMacChrome ()                                               { return m_pImpl->TickMacChrome (); }
#endif

SDL_Window* CHROME_RML::Window ()             const { return m_pImpl->m_pSDLWindow; }
void*       CHROME_RML::NativeParentHandle () const { return m_pImpl->NativeParentHandle (); }

SDL_WindowID CHROME_RML::SDLWindowID () const { return m_pImpl->m_nSDLWindowID; }
void         CHROME_RML::HandleEvent (SDL_Event& Event) { m_pImpl->HandleEvent (Event); }

bool        CHROME_RML::MenuOpen () const     { return m_pImpl->m_bMenuOpen; }
void        CHROME_RML::DismissMenu ()        { m_pImpl->SetPopupMenuOpen (false); }
SDL_Window* CHROME_RML::MenuWindow () const   { return m_pImpl->m_pMenuWindow; }

bool        CHROME_RML::UrlDropOpen () const   { return m_pImpl->m_bUrlOpen; }
void        CHROME_RML::DismissUrlDrop ()      { m_pImpl->SetUrlDropOpen (false); }
SDL_Window* CHROME_RML::UrlDropWindow () const { return m_pImpl->m_pUrlWindow; }

} // namespace RUBIDIUM
