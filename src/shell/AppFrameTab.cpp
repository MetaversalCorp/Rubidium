// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "AppFrameTab.h"

using namespace RUBIDIUM;

/*******************************************************************************************************************************
**                                                      Class: VIEWPORT_HOST                                                  **
***************************************************************************************************************************** */

class CONTEXT_HOST : public SNEEZE::ICONTEXT
{
public:
   CONTEXT_HOST (RUBIDIUM::INSPECTOR_RML** ppInspectorRml, HWND hWndParent) :
      m_ppInspectorRml (ppInspectorRml),
      m_hWndParent     (hWndParent)
   {
   }

   void OnNetworkCacheCreated (SNEEZE::CACHE* pCache) override
   {
      if (*m_ppInspectorRml)
         (*m_ppInspectorRml)->OnNetworkCacheCreated (pCache);
   }

   void OnNetworkCacheDeleted (SNEEZE::CACHE* pCache) override
   {
      if (*m_ppInspectorRml)
         (*m_ppInspectorRml)->OnNetworkCacheDeleted (pCache);
   }

   bool OnNetworkFileCreated (SNEEZE::FILE* pNotification) override
   {
      bool bResult = false;

      if (*m_ppInspectorRml)
      {
         bResult = (*m_ppInspectorRml)->OnNetworkFileCreated (pNotification);
         PostMessage (m_hWndParent, WM_NULL, 0, 0);
      }

      return bResult;
   }

   void OnNetworkFileChanged (SNEEZE::FILE* pNotification) override
   {
      if (*m_ppInspectorRml)
      {
         (*m_ppInspectorRml)->OnNetworkFileChanged (pNotification);
         PostMessage (m_hWndParent, WM_NULL, 0, 0);
      }
   }

   void OnNetworkFileDeleted (SNEEZE::FILE* pNotification) override
   {
      if (*m_ppInspectorRml)
      {
         (*m_ppInspectorRml)->OnNetworkFileDeleted (pNotification);
         PostMessage (m_hWndParent, WM_NULL, 0, 0);
      }
   }

   void OnConsoleEntryCreated (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr) override
   {
      if (*m_ppInspectorRml)
      {
         (*m_ppInspectorRml)->OnConsoleEntryCreated (pEntryPtr);
         PostMessage (m_hWndParent, WM_NULL, 0, 0);
      }
   }

   void OnConsoleEntryDeleted (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr) override
   {
      if (*m_ppInspectorRml)
      {
         (*m_ppInspectorRml)->OnConsoleEntryDeleted (pEntryPtr);
         PostMessage (m_hWndParent, WM_NULL, 0, 0);
      }
   }

   void OnStorageSiloCreated (SNEEZE::SILO* pSilo) override
   {
      if (*m_ppInspectorRml)
      {
         (*m_ppInspectorRml)->OnStorageSiloCreated (pSilo);
         PostMessage (m_hWndParent, WM_NULL, 0, 0);
      }
   }

   void OnStorageSiloDeleted (SNEEZE::SILO* pSilo) override
   {
      if (*m_ppInspectorRml)
      {
         (*m_ppInspectorRml)->OnStorageSiloDeleted (pSilo);
         PostMessage (m_hWndParent, WM_NULL, 0, 0);
      }
   }

   void OnStorageUnitCreated (SNEEZE::SILO* pSilo, SNEEZE::eSILO_SCOPE eScope) override
   {
      if (*m_ppInspectorRml)
      {
         (*m_ppInspectorRml)->OnStorageUnitChanged (pSilo, eScope);
         PostMessage (m_hWndParent, WM_NULL, 0, 0);
      }
   }

   void OnStorageUnitChanged (SNEEZE::SILO* pSilo, SNEEZE::eSILO_SCOPE eScope, const std::string&) override
   {
      if (*m_ppInspectorRml)
      {
         (*m_ppInspectorRml)->OnStorageUnitChanged (pSilo, eScope);
         PostMessage (m_hWndParent, WM_NULL, 0, 0);
      }
   }

   void OnStorageUnitDeleted (SNEEZE::SILO* pSilo, SNEEZE::eSILO_SCOPE eScope) override
   {
      if (*m_ppInspectorRml)
      {
         (*m_ppInspectorRml)->OnStorageUnitChanged (pSilo, eScope);
         PostMessage (m_hWndParent, WM_NULL, 0, 0);
      }
   }

private:
   INSPECTOR_RML**   m_ppInspectorRml;
   HWND              m_hWndParent;
};

class VIEWPORT_HOST : public SNEEZE::IVIEWPORT
{
public:
   VIEWPORT_HOST (RUBIDIUM::CANVAS* pCanvas) :
      m_pCanvas (pCanvas)
   {
   }

   void* FrameWindow () override
   {
      return m_pCanvas->NativeWindowHandle ();
   }

   bool FrameSize (int& nWidth, int& nHeight) override
   {
      return m_pCanvas->FrameSize (nWidth, nHeight);
   }

   void OnFrameReady (const uint32_t* pFB, int nFbW, int nFbH) override
   {
      m_pCanvas->Present (pFB, nFbW, nFbH);
   }

private:
   CANVAS*           m_pCanvas;
};

/*******************************************************************************************************************************
**                                                      Class: APPFRAME_NATIVE                                                **
***************************************************************************************************************************** */

APPFRAMETAB_NATIVE::APPFRAMETAB_NATIVE (HINSTANCE hInst, LOGGER* pLogger, SNEEZE::ENGINE* pSneeze, std::wstring wsTitle) :
   m_hInst   (hInst),
   m_pLogger (pLogger),
   m_wsTitle (wsTitle),
   m_pSneeze (pSneeze),
   m_pInspectorRml (nullptr),
   m_pSettingsRml (nullptr),
   m_pContext (nullptr),
   m_pViewport (nullptr),
   m_pCtxHost (nullptr),
   m_pVPHost (nullptr),
   m_pCanvas (new CANVAS_NATIVE (pLogger)),
   m_nCanvasPosY (0),
   m_eSession (SNEEZE::CONTEXT::kSESSION_PERSISTENT),
   m_bActive (false),
   m_hwndOwner (NULL)
{
}

APPFRAMETAB_NATIVE::~APPFRAMETAB_NATIVE ()
{
   DestroyContext ();

   if (m_pInspectorRml)
   {
      delete m_pInspectorRml;
      m_pInspectorRml = nullptr;
   }

   if (m_pSettingsRml)
   {
      delete m_pSettingsRml;
      m_pSettingsRml = nullptr;
   }

   delete m_pCanvas;
   m_pCanvas = nullptr;

   delete m_pVPHost;
   delete m_pCtxHost;
}

bool APPFRAMETAB_NATIVE::Init (HWND hwndParent, int nX, int nY, int nWidth, int nHeight, SNEEZE::CONTEXT::eSESSION eSession)
{
   bool bResult = false;

   m_nCanvasPosY = nY;
   m_hwndOwner   = hwndParent;

   if (m_pCanvas->Initialize (hwndParent, nWidth, nHeight))
   {
      NotifyChildReady (hwndParent);

      m_pInspectorRml = new INSPECTOR_RML ();
      if (!m_pInspectorRml->Initialize ())
      {
         m_pLogger->Log (LOGGER::kLOGLEVEL_Warning, "Tab", "RmlUi Inspector failed to initialize");
         delete m_pInspectorRml;
         m_pInspectorRml = nullptr;
      }
      else
      {
         // The inspector's glb Preview tab opens its own preview context/viewport.
         m_pInspectorRml->SetEngine (m_pSneeze);
      }

      m_pCtxHost = new CONTEXT_HOST (&m_pInspectorRml, hwndParent);
      m_pVPHost  = new VIEWPORT_HOST (m_pCanvas);

      nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();

      m_sUrl      = jSettings["home"];
      m_eSession  = eSession;

      APPNATIVE::GetInstance ()->UrlHistory_Add (m_sUrl);

      CreateContext ();
   }

   return bResult;
}

bool APPFRAMETAB_NATIVE::CreateContext (bool bReset)
{
   bool bResult = false;

   if ((m_pContext = m_pSneeze->Context_Open (m_pCtxHost, m_sUrl, m_eSession, bReset)) != nullptr)
   {
      m_pViewport = m_pContext->Viewport ();

      struct ENUM_PURGE : SNEEZE::IENUM_FILE
      {
         void OnAsset (SNEEZE::FILE* pFile) override
         {
            if (pFile->ContentType () == "application/jose+msf")
               pFile->Reset ();
         }
      };
      ENUM_PURGE pEnum_Purge;
      ///      m_pContext->Network ()->File_Enum (&pEnum_Purge);              /// This deadlocks

      if (m_pInspectorRml)
      {
         m_pInspectorRml->SetContext (m_pContext);
         m_pInspectorRml->SetScene   (m_pContext->Scene ());
      }

      bResult = true;
   }
   else
   {
      delete m_pCanvas;
      m_pCanvas = nullptr;
   }

   return bResult;
}

void APPFRAMETAB_NATIVE::DestroyContext ()
{
   if (m_bActive)
      ViewportDetach ();

   if (m_pContext)
   {
      m_pSneeze->Context_Close (m_pContext);
      m_pContext = nullptr;
      m_pViewport = nullptr;
   }
}

const std::wstring& APPFRAMETAB_NATIVE::Title () const
{
   return m_wsTitle;
}

void APPFRAMETAB_NATIVE::Title (std::wstring& wsTitle)
{
   m_wsTitle = std::move (wsTitle);
}

void APPFRAMETAB_NATIVE::NotifyChildReady (HWND hwndParent, bool bRaiseToTop)
{
   HWND hSdlChild = (HWND)m_pCanvas->NativeWindowHandle ();
   if (hSdlChild)
   {
      RECT rcClient;
      GetClientRect (hwndParent, &rcClient);

      int nWidth  =  rcClient.right  - rcClient.left;
      int nHeight = (rcClient.bottom - rcClient.top) - m_nCanvasPosY;

      SetWindowPos (hSdlChild, bRaiseToTop ? HWND_TOP : nullptr, 0, m_nCanvasPosY, nWidth, nHeight, SWP_NOACTIVATE | SWP_NOSIZE);
      m_pCanvas->Resize (nWidth, nHeight);
   }
}

void APPFRAMETAB_NATIVE::Resize (HWND hwndParent, bool bFinal)
{
   HWND hSdlChild = (HWND)m_pCanvas->NativeWindowHandle ();
   if (hSdlChild)
   {
      RECT rcClient;
      GetClientRect (hwndParent, &rcClient);

      int nWidth  =  rcClient.right  - rcClient.left;
      int nHeight = (rcClient.bottom - rcClient.top) - m_nCanvasPosY;

      SetWindowPos (hSdlChild, nullptr, 0, m_nCanvasPosY, nWidth, nHeight, SWP_NOACTIVATE | SWP_NOSIZE);
      m_pCanvas->Resize (nWidth, nHeight);
   }
}

void APPFRAMETAB_NATIVE::ProcessInput ()
{
   m_pCanvas->ProcessInput (m_pViewport);

   if (m_pInspectorRml)
      m_pInspectorRml->ProcessPendingFiles ();
}

void APPFRAMETAB_NATIVE::Show (bool bShow)
{
   m_pCanvas->SetVisible (bShow);
}

void APPFRAMETAB_NATIVE::ViewportAttach ()
{
   m_bActive = true;
   m_pViewport->Activate (m_pVPHost);
}

void APPFRAMETAB_NATIVE::ViewportDetach ()
{
   m_bActive = false;
   m_pViewport->Deactivate ();
}

void APPFRAMETAB_NATIVE::ToggleInspector ()
{
   if (m_pInspectorRml)
      m_pInspectorRml->Toggle ();
}

void APPFRAMETAB_NATIVE::ShowSettings ()
{
   if (!m_pSettingsRml)
   {
      m_pSettingsRml = new SETTINGS_RML ();

      if (!m_pSettingsRml->Initialize ())
      {
         m_pLogger->Log (LOGGER::kLOGLEVEL_Warning, "Tab", "Settings window failed to initialize");
         delete m_pSettingsRml;
         m_pSettingsRml = nullptr;
      }
   }

   if (m_pSettingsRml)
   {
      m_pSettingsRml->SetOwner ((void*) m_hwndOwner);
      m_pSettingsRml->Toggle ();
   }
}

void APPFRAMETAB_NATIVE::UpdateUrl (HWND hWndEdit)
{
   SetWindowText (hWndEdit, m_sUrl.c_str ());
}

void APPFRAMETAB_NATIVE::Url (std::string& sUrl)
{
   bool bActive = m_bActive;

   m_sUrl = sUrl;

   DestroyContext ();

   m_pInspectorRml->Reset ();

   if (CreateContext ())
   {
      if (bActive)
         ViewportAttach ();
   }
}

void APPFRAMETAB_NATIVE::Reload (bool bReset)
{
   bool bActive = m_bActive;

   DestroyContext ();

   m_pInspectorRml->Reset ();

   if (CreateContext (bReset))
   {
      if (bActive)
         ViewportAttach ();

      // Activate rebuilds the renderer from scratch, which comes up with the
      // default backdrop. Re-assert the scene's stored colour so the
      // consume-once changed flag trips again and the compositor pushes it to
      // the new renderer (same pattern as PREVIEW3D::Show).
      if (m_pContext  &&  m_pContext->Scene ())
         m_pContext->Scene ()->Background (m_pContext->Scene ()->Background ());
   }
}
