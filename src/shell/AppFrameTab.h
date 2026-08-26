// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// Windows APPFRAME — native Win32 shell with custom dark-mode chrome.
// Does NOT use SDL for the top-level window (only the Canvas child
// surface does). Keeps the implementation isolated to Windows so
// every other platform's APPFRAME_NATIVE inherits from APPFRAME_SDL instead.

#include "inspector/InspectorRml.h"
#include "settings_ui/SettingsRml.h"

#ifndef RUBIDIUM_SHELL_APPFRAMETAB_NATIVE_H
#define RUBIDIUM_SHELL_APPFRAMETAB_NATIVE_H

namespace RUBIDIUM 
{ 
   class APPFRAMETAB_NATIVE
   {
   public:
      APPFRAMETAB_NATIVE (HINSTANCE hInst, LOGGER* pLogger, SNEEZE::ENGINE* pSneeze, std::wstring wsTitle);
      ~APPFRAMETAB_NATIVE ();

      bool Init (HWND hwndParent, int nX, int nY, int nWidth, int nHeight, SNEEZE::CONTEXT::eSESSION eSession);

      void NotifyChildReady (HWND hwndParent, bool bRaiseToTop = false);
      void Resize (HWND hwndParent, bool bFinal);
      void ProcessInput ();
      void Show (bool bShow);

      void ViewportAttach ();
      void ViewportDetach ();

      void ToggleInspector ();
      void ShowSettings ();

      // Accessors
      const std::wstring& Title () const;

      // Modifiers
      void Title  (std::wstring& wsTitle);
      void Url    (std::string& sUrl);
      void Reload (bool bReset);

      void UpdateUrl (HWND hWndEdit);

   private:
      HINSTANCE                  m_hInst;
      LOGGER*                    m_pLogger;
      std::wstring               m_wsTitle;
      SNEEZE::ENGINE*            m_pSneeze;
      INSPECTOR_RML*             m_pInspectorRml;
      SETTINGS_RML*              m_pSettingsRml;
      CANVAS*                    m_pCanvas;
      SNEEZE::CONTEXT*           m_pContext;
      SNEEZE::VIEWPORT*          m_pViewport;
      SNEEZE::ICONTEXT*          m_pCtxHost;
      SNEEZE::IVIEWPORT*         m_pVPHost;

      std::string                m_sUrl;
      SNEEZE::CONTEXT::eSESSION  m_eSession;
      bool                       m_bActive;
      int                        m_nCanvasPosY;
      HWND                       m_hwndOwner;

      bool CreateContext (bool bReset = false);
      void DestroyContext ();
   };
}

#endif // RUBIDIUM_SHELL_APPFRAMETAB_NATIVE_H
