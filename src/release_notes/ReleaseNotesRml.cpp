// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "pch.h"

#include <SDL3/SDL_misc.h>

#if defined(RUBIDIUM_PLATFORM_LINUX)
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cstdlib>
#endif

#include "rmlui_sdl/RmlUi_SDL.h"

#include "release_notes/ReleaseNotesRml.h"
#include "release_notes/Markdown.h"
#include "version.h"

using namespace RUBIDIUM;

namespace
{
   bool IsHttpUrl (const std::string& sUrl)
   {
      return sUrl.rfind ("http://", 0) == 0  ||  sUrl.rfind ("https://", 0) == 0;
   }

#if defined(RUBIDIUM_PLATFORM_LINUX)
   // Silence a child's stdout/stderr so opener diagnostics (e.g. gio's
   // "Operation not supported") never leak onto our terminal.
   void RedirectStdioToNull ()
   {
      int nNull = open ("/dev/null", O_WRONLY);

      if (nNull >= 0)
      {
         dup2 (nNull, STDOUT_FILENO);
         dup2 (nNull, STDERR_FILENO);

         if (nNull > STDERR_FILENO)
            close (nNull);
      }
   }

   // True if szName resolves to an executable on PATH. Lets us pick a browser
   // binary that actually exists instead of exec-chaining and hoping.
   bool ExecutableOnPath (const char* szName)
   {
      bool        bFound = false;
      const char* szPath = std::getenv ("PATH");

      if (szPath)
      {
         std::string sPath (szPath);
         size_t      nStart = 0;

         while (nStart <= sPath.size ()  &&  !bFound)
         {
            size_t      nColon = sPath.find (':', nStart);
            size_t      nLen   = (nColon == std::string::npos) ? std::string::npos : nColon - nStart;
            std::string sDir   = sPath.substr (nStart, nLen);

            if (!sDir.empty ()  &&  access ((sDir + "/" + szName).c_str (), X_OK) == 0)
               bFound = true;

            nStart = (nColon == std::string::npos) ? sPath.size () + 1 : nColon + 1;
         }
      }

      return bFound;
   }

   // Run a launcher-style opener (xdg-open, gio open) to completion and report
   // whether it exited 0. These spawn the browser and return promptly, so the
   // brief wait is acceptable and lets us detect runtime failures (missing
   // scheme handler) that an exec-only chain cannot see.
   bool RunOpener (const char* szFile, const char* szArg1, const std::string& sUrl)
   {
      bool  bResult = false;
      pid_t pidChild = fork ();

      if (pidChild == 0)
      {
         setsid ();
         RedirectStdioToNull ();

         if (szArg1)
            execlp (szFile, szFile, szArg1, sUrl.c_str (), static_cast<char*> (nullptr));
         else
            execlp (szFile, szFile, sUrl.c_str (), static_cast<char*> (nullptr));

         _exit (127);
      }
      else if (pidChild > 0)
      {
         int nStatus = 0;

         if (waitpid (pidChild, &nStatus, 0) == pidChild)
            bResult = WIFEXITED (nStatus)  &&  WEXITSTATUS (nStatus) == 0;
      }

      return bResult;
   }

   // Launch a browser binary fully detached (double fork -> reparents to init,
   // never blocks us). Used only after the launchers fail; bypasses the desktop
   // scheme-handler system entirely, so it works even when gio/xdg-open can't
   // resolve a default browser.
   bool LaunchBrowserDetached (const char* szFile, const std::string& sUrl)
   {
      bool  bResult = false;
      pid_t pidOuter = fork ();

      if (pidOuter == 0)
      {
         setsid ();

         if (fork () == 0)
         {
            RedirectStdioToNull ();
            execlp (szFile, szFile, sUrl.c_str (), static_cast<char*> (nullptr));
            _exit (127);
         }

         _exit (0);
      }
      else if (pidOuter > 0)
      {
         // Reap the short-lived intermediate; the browser itself is reparented
         // to init and runs on independently.
         waitpid (pidOuter, nullptr, 0);
         bResult = true;
      }

      return bResult;
   }

   bool OpenUrlPosixFallback (const std::string& sUrl)
   {
      // 1) Launchers that respect the user's default browser and return promptly.
      //    RunOpener detects a non-zero exit (e.g. gio "Operation not supported")
      //    so a broken association falls through instead of silently "succeeding".
      bool bResult = RunOpener ("xdg-open", nullptr, sUrl)
                  || RunOpener ("gio", "open", sUrl);

      // 2) No working scheme handler -- launch a browser binary directly. Only
      //    attempt ones present on PATH so the result reflects reality.
      if (!bResult)
      {
         static const char* aszBrowsers[] =
         {
            "firefox", "google-chrome", "chromium", "chromium-browser", "brave-browser", "microsoft-edge"
         };

         for (const char* szBrowser : aszBrowsers)
         {
            if (ExecutableOnPath (szBrowser)  &&  LaunchBrowserDetached (szBrowser, sUrl))
            {
               bResult = true;
               break;
            }
         }
      }

      return bResult;
   }
#endif

   bool OpenUrl (const std::string& sUrl)
   {
      bool    bResult = false;
      LOGGER* pLogger = APPNATIVE::GetInstance ()->Logger ();

      if (IsHttpUrl (sUrl))
      {
#if defined(RUBIDIUM_PLATFORM_LINUX)
         // SDL_OpenURL uses posix_spawn, which fails on some Linux setups
         // (observed: "posix_spawn() failed: Unknown error 512"). A direct,
         // detached fork/exec of the standard openers is reliable here, so try
         // it first; only fall back to SDL_OpenURL if the fork itself fails.
         bResult = OpenUrlPosixFallback (sUrl);

         if (!bResult)
            bResult = SDL_OpenURL (sUrl.c_str ());
#else
         bResult = SDL_OpenURL (sUrl.c_str ());
#endif

         if (pLogger)
            pLogger->Log (bResult ? LOGGER::kLOGLEVEL_Info : LOGGER::kLOGLEVEL_Warning,
               "ReleaseNotes", bResult ? ("Opened URL: " + sUrl)
                                       : ("Failed to open URL: " + sUrl + " (" + SDL_GetError () + ")"));
      }
      else if (pLogger)
      {
         pLogger->Log (LOGGER::kLOGLEVEL_Warning, "ReleaseNotes", "Refusing to open non-http URL: " + sUrl);
      }

      return bResult;
   }
}

/*******************************************************************************************************************************
**                                                        Impl                                                               **
*******************************************************************************************************************************/

class RELEASE_NOTES_RML::Impl : public Rml::EventListener
{
public:
   static inline const char* s_sRmlDocument =
R"rml(
<rml>
<head>
<style>
body
{
   font-family: [{FONT-FAMILY}];
   font-size: 14dp;
   background: #ffffff;
   color: #202124;
   margin: 0;
   padding: 0;
   width: 100%;
   height: 100%;
   display: flex;
   flex-direction: column;
}

div#header
{
   display: flex;
   flex-direction: row;
   align-items: center;
   flex-shrink: 0;
   height: 64dp;
   padding-left: 24dp;
   padding-right: 24dp;
   box-sizing: border-box;
   border-bottom: 1px #E8EAED;
}

img#logo
{
   width: 28dp;
   height: 28dp;
   margin-right: 16dp;
}

div#title
{
   font-size: 20dp;
   font-weight: 500;
   color: #202124;
}

div#body
{
   flex-grow: 1;
   overflow-y: auto;
   padding: 8dp 28dp 20dp 28dp;
   box-sizing: border-box;
}

/* RmlUi lays an auto-width scrollbar out to fill the container, collapsing the
   content client width to near zero (every word then wraps). A definite width
   reserves the gutter and leaves the text its full column. */
scrollbarvertical
{
   width: 12dp;
}

div.h1
{
   display: block;
   font-size: 20dp;
   font-weight: 600;
   color: #202124;
   margin-top: 18dp;
   margin-bottom: 8dp;
}

div.h2
{
   display: block;
   font-size: 17dp;
   font-weight: 600;
   color: #202124;
   margin-top: 16dp;
   margin-bottom: 6dp;
}

div.h3
{
   display: block;
   font-size: 14dp;
   font-weight: 600;
   color: #5f6368;
   margin-top: 12dp;
   margin-bottom: 4dp;
}

p
{
   display: block;
   font-size: 14dp;
   color: #3c4043;
   margin-top: 6dp;
   margin-bottom: 6dp;
}

div.li
{
   display: flex;
   flex-direction: row;
   align-items: flex-start;
   margin-top: 3dp;
   margin-bottom: 3dp;
   padding-left: 6dp;
}

span.bullet
{
   color: #0B57D0;
   margin-right: 10dp;
   flex-shrink: 0;
}

span.litext
{
   color: #3c4043;
   flex-grow: 1;
}

strong
{
   font-weight: 600;
   color: #202124;
}

em
{
   font-style: italic;
}

span.code
{
   font-family: "JetBrains Mono";
   font-size: 13dp;
   background: #F1F3F4;
   color: #202124;
   padding-left: 4dp;
   padding-right: 4dp;
   border-radius: 4dp;
}

a.link
{
   color: #0B57D0;
   text-decoration: underline;
   cursor: pointer;
}

div.hr
{
   display: block;
   height: 1px;
   background: #E8EAED;
   margin-top: 14dp;
   margin-bottom: 14dp;
}

div#footer
{
   display: flex;
   flex-direction: row;
   justify-content: flex-end;
   align-items: center;
   flex-shrink: 0;
   height: 60dp;
   padding-right: 24dp;
   box-sizing: border-box;
   border-top: 1px #E8EAED;
}

div#btn-close
{
   color: #ffffff;
   background: #0B57D0;
   font-weight: 500;
   border-radius: 100dp;
   padding: 8dp 22dp;
   cursor: pointer;
}

div#btn-close:hover
{
   background: #0A4FBF;
}
</style>
</head>
<body>
<div id="header">
   <img id="logo" src="[{LOGO-PATH}]"/>
   <div id="title">Release Notes v [{VERSION}]</div>
</div>
<div id="body">[{BODY}]</div>
<div id="footer">
   <div id="btn-close">Got it</div>
</div>
</body>
</rml>
)rml";

   Impl () :
      m_pBtnClose (nullptr),
      m_pBody     (nullptr),
      m_pOwner    (nullptr)
   {
   }

   ~Impl ()
   {
      if (m_Window.Document ())
      {
         if (m_pBtnClose)
            m_pBtnClose->RemoveEventListener (Rml::EventId::Click, this);

         if (m_pBody)
            m_pBody->RemoveEventListener (Rml::EventId::Click, this);
      }
   }

   // Window-modal behaviour: while the popup is visible the owning application
   // window is disabled so its chrome cannot be interacted with; closing the
   // popup ("Got it" or the window's close button) re-enables and refocuses it.
   // Mirrors SETTINGS_RML -- modality itself is platform-specific.
   static void OnVisibilityChanged (bool bVisible, void* pUserData)
   {
      Impl* pThis = static_cast<Impl*> (pUserData);

#ifdef RUBIDIUM_PLATFORM_WINDOWS
      HWND hOwner = static_cast<HWND> (pThis->m_pOwner);

      if (hOwner)
      {
         EnableWindow (hOwner, !bVisible);

         if (!bVisible)
            SetForegroundWindow (hOwner);
      }
#else
      // SDL platforms (Linux / macOS): the popup is a plain top-level window (no
      // SetModalParent -- that stops the window mapping on X11). Modality is
      // enforced in-app (APPFRAME_SDL::ProcessInput). Raise the owner back to the
      // front once the popup closes so focus returns to the main window.
      if (!bVisible)
      {
         SDL_Window* pOwner = static_cast<SDL_Window*> (pThis->m_pOwner);

         if (pOwner)
            SDL_RaiseWindow (pOwner);
      }
#endif
   }

   static std::string LogoPath ()
   {
      std::string sPath;

      if (const char* pBasePath = SDL_GetBasePath ())
         sPath = std::string (pBasePath) + "images/logo.png";

      std::replace (sPath.begin (), sPath.end (), '\\', '/');

      // RmlUi's SystemInterface::JoinPath strips one leading '/' from absolute
      // POSIX paths; prepend an extra '/' so the strip leaves a valid path.
      // Windows drive paths (C:/...) pass through untouched.
      if (!sPath.empty ()  &&  sPath[0] == '/')
         sPath = "/" + sPath;

      return sPath;
   }

   static void ReplaceAll (std::string& sText, const std::string& sToken, const std::string& sValue)
   {
      size_t nPos = 0;

      while ((nPos = sText.find (sToken, nPos)) != std::string::npos)
      {
         sText.replace (nPos, sToken.size (), sValue);
         nPos += sValue.size ();
      }
   }

   static void ClampPopupToWorkArea (int& nX, int& nY, int nPopupW, int nPopupH, void* pOwner)
   {
#ifdef RUBIDIUM_PLATFORM_WINDOWS
      HWND        hRef  = static_cast<HWND> (pOwner);
      HMONITOR    hMon  = MonitorFromWindow (hRef ? hRef : nullptr, MONITOR_DEFAULTTONEAREST);
      MONITORINFO MonitorInfo = {};

      MonitorInfo.cbSize = sizeof (MonitorInfo);

      if (GetMonitorInfo (hMon, &MonitorInfo))
      {
         RECT Rect = MonitorInfo.rcWork;

         if (nX < Rect.left)
            nX = Rect.left;

         if (nY < Rect.top)
            nY = Rect.top;

         if (nX + nPopupW > Rect.right)
            nX = Rect.right - nPopupW;

         if (nY + nPopupH > Rect.bottom)
            nY = Rect.bottom - nPopupH;
      }
#else
      SDL_Window*   pRef      = static_cast<SDL_Window*> (pOwner);
      SDL_DisplayID nDisplay  = pRef ? SDL_GetDisplayForWindow (pRef) : SDL_GetPrimaryDisplay ();
      SDL_Rect      Usable    = {};

      if (SDL_GetDisplayUsableBounds (nDisplay, &Usable))
      {
         if (nX < Usable.x)
            nX = Usable.x;

         if (nY < Usable.y)
            nY = Usable.y;

         if (nX + nPopupW > Usable.x + Usable.w)
            nX = Usable.x + Usable.w - nPopupW;

         if (nY + nPopupH > Usable.y + Usable.h)
            nY = Usable.y + Usable.h - nPopupH;
      }
#endif
   }

   void CenterOnOwner ()
   {
      SDL_Window* pPopup = SDL_GetWindowFromID (m_Window.SDLWindowID ());

      if (pPopup)
      {
         int nPopupW = 0;
         int nPopupH = 0;

         SDL_GetWindowSize (pPopup, &nPopupW, &nPopupH);

         int  nX       = 0;
         int  nY       = 0;
         bool bCentered = false;

#ifdef RUBIDIUM_PLATFORM_WINDOWS
         HWND hOwner = static_cast<HWND> (m_pOwner);

         if (hOwner)
         {
            RECT Rect = {};

            if (GetWindowRect (hOwner, &Rect))
            {
               int nOwnerW = Rect.right - Rect.left;
               int nOwnerH = Rect.bottom - Rect.top;

               nX = Rect.left + (nOwnerW - nPopupW) / 2;
               nY = Rect.top  + (nOwnerH - nPopupH) / 2;
               bCentered = true;
            }
         }
#else
         SDL_Window* pOwner = static_cast<SDL_Window*> (m_pOwner);

         if (pOwner)
         {
            int nOwnerX = 0;
            int nOwnerY = 0;
            int nOwnerW = 0;
            int nOwnerH = 0;

            SDL_GetWindowPosition (pOwner, &nOwnerX, &nOwnerY);
            SDL_GetWindowSize (pOwner, &nOwnerW, &nOwnerH);

            nX = nOwnerX + (nOwnerW - nPopupW) / 2;
            nY = nOwnerY + (nOwnerH - nPopupH) / 2;
            bCentered = true;
         }
#endif

         if (!bCentered)
         {
            SDL_Rect      Usable    = {};
            SDL_DisplayID nDisplay = SDL_GetPrimaryDisplay ();

            if (SDL_GetDisplayUsableBounds (nDisplay, &Usable))
            {
               nX = Usable.x + (Usable.w - nPopupW) / 2;
               nY = Usable.y + (Usable.h - nPopupH) / 2;
            }
         }

         ClampPopupToWorkArea (nX, nY, nPopupW, nPopupH, m_pOwner);

         m_Window.SetPosition (nX, nY);
      }
   }

   void Show ()
   {
      CenterOnOwner ();
      m_Window.Show ();

      // Linux/X11: the first Show()+Render() can run before the WM assigns a
      // backing store. Pump briefly so SHOWN/EXPOSED/RESIZED reach the window
      // (via the event watch and/or queue), then repaint.
      for (int nFrame = 0; nFrame < 4; nFrame++)
      {
         SDL_PumpEvents ();
         SDL_Delay (10);
      }

      m_Window.Render ();
   }

   bool Initialize (const std::string& sVersion, const std::string& sMarkdown)
   {
      bool bResult = false;

      if (m_Window.Initialize ("Release Notes", 1024, 720))
      {
         m_Window.SetVisibilityCallback (OnVisibilityChanged, this);

         std::string sRmlDocument (s_sRmlDocument);

         ReplaceAll (sRmlDocument, "[{FONT-FAMILY}]", APPNATIVE::GetInstance ()->sFontFamily ());
         ReplaceAll (sRmlDocument, "[{LOGO-PATH}]",   LogoPath ());
         ReplaceAll (sRmlDocument, "[{VERSION}]",     sVersion);
         ReplaceAll (sRmlDocument, "[{BODY}]",        Markdown_ToRml (sMarkdown));

         if (m_Window.LoadDocument (sRmlDocument))
         {
            Rml::ElementDocument* pDoc = m_Window.Document ();

            m_pBtnClose = pDoc->GetElementById ("btn-close");
            m_pBody     = pDoc->GetElementById ("body");

            if (m_pBtnClose  &&  m_pBody)
            {
               m_pBtnClose->AddEventListener (Rml::EventId::Click, this);
               m_pBody    ->AddEventListener (Rml::EventId::Click, this);
               bResult = true;
            }
         }
      }

      return bResult;
   }

   void ProcessEvent (Rml::Event& Event) override
   {
      if (Event.GetId () == Rml::EventId::Click)
      {
         if (Event.GetCurrentElement () == m_pBtnClose)
         {
            // The window is visible when "Got it" is clicked, so Toggle () hides it.
            m_Window.Toggle ();
         }
         else if (Event.GetCurrentElement () == m_pBody)
         {
            // RmlUi has no built-in hyperlink navigation -- delegate clicks on
            // <a href> elements to the system browser via OpenUrl. Walk from the
            // click target up to the body, since the target may be the anchor's
            // inner text element rather than the anchor itself.
            Rml::Element* pElement = Event.GetTargetElement ();
            bool          bFound   = false;

            while (pElement  &&  pElement != m_pBody)
            {
               Rml::String sHref = pElement->GetAttribute<Rml::String> ("href", "");

               if (!sHref.empty ())
               {
                  OpenUrl (sHref.c_str ());
                  bFound = true;
                  break;
               }

               pElement = pElement->GetParentNode ();
            }

            if (!bFound)
            {
               LOGGER* pLogger = APPNATIVE::GetInstance ()->Logger ();

               if (pLogger)
                  pLogger->Log (LOGGER::kLOGLEVEL_Trace, "ReleaseNotes", "Body click with no href in ancestry");
            }
         }
      }
   }

   RMLUI_SDL     m_Window;
   Rml::Element* m_pBtnClose;
   Rml::Element* m_pBody;
   void*         m_pOwner;
};

/*******************************************************************************************************************************
**                                                   RELEASE_NOTES_RML                                                       **
*******************************************************************************************************************************/

RELEASE_NOTES_RML::RELEASE_NOTES_RML () :
   m_pImpl (new Impl ())
{
}

RELEASE_NOTES_RML::~RELEASE_NOTES_RML ()
{
   delete m_pImpl;
   m_pImpl = nullptr;
}

bool RELEASE_NOTES_RML::Initialize (const std::string& sVersion, const std::string& sMarkdown)
{
   return m_pImpl->Initialize (sVersion, sMarkdown);
}

void RELEASE_NOTES_RML::Show ()            {        m_pImpl->Show (); }
bool RELEASE_NOTES_RML::IsVisible () const { return m_pImpl->m_Window.IsVisible (); }
bool RELEASE_NOTES_RML::IsOpen () const    { return m_pImpl->m_Window.IsOpen (); }

void RELEASE_NOTES_RML::SetOwner (void* hOwner)
{
   // Just record the owner. On Windows it is an HWND disabled via EnableWindow in
   // OnVisibilityChanged. On SDL platforms (Linux / macOS) we deliberately do NOT
   // call SetModalParent: SDL_SetWindowParent + SDL_WINDOW_MODAL prevents the
   // popup from mapping on X11 (same failure the Settings window hit). Modality is
   // instead enforced in-app -- APPFRAME_SDL::ProcessInput blocks chrome + canvas
   // input while IsReleaseNotesOpen () is true. This mirrors SETTINGS_RML::SetOwner.
   m_pImpl->m_pOwner = hOwner;
}
