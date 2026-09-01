// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "AppFrameTab_SDL.h"

using namespace RUBIDIUM;

/*******************************************************************************************************************************
**                                                      Class: CONTEXT_HOST                                                   **
*******************************************************************************************************************************/

class CONTEXT_HOST : public SNEEZE::ICONTEXT
{
public:
   CONTEXT_HOST (RUBIDIUM::INSPECTOR_RML** ppInspectorRml) :
      m_ppInspectorRml (ppInspectorRml)
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
         bResult = (*m_ppInspectorRml)->OnNetworkFileCreated (pNotification);

      return bResult;
   }

   void OnNetworkFileChanged (SNEEZE::FILE* pNotification) override
   {
      if (*m_ppInspectorRml)
         (*m_ppInspectorRml)->OnNetworkFileChanged (pNotification);
   }

   void OnNetworkFileDeleted (SNEEZE::FILE* pNotification) override
   {
      if (*m_ppInspectorRml)
         (*m_ppInspectorRml)->OnNetworkFileDeleted (pNotification);
   }

   void OnConsoleEntryCreated (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr) override
   {
      if (*m_ppInspectorRml)
         (*m_ppInspectorRml)->OnConsoleEntryCreated (pEntryPtr);
   }

   void OnConsoleEntryDeleted (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr) override
   {
      if (*m_ppInspectorRml)
         (*m_ppInspectorRml)->OnConsoleEntryDeleted (pEntryPtr);
   }

   void OnStorageSiloCreated (SNEEZE::SILO* pSilo) override
   {
      if (*m_ppInspectorRml)
         (*m_ppInspectorRml)->OnStorageSiloCreated (pSilo);
   }

   void OnStorageSiloDeleted (SNEEZE::SILO* pSilo) override
   {
      if (*m_ppInspectorRml)
         (*m_ppInspectorRml)->OnStorageSiloDeleted (pSilo);
   }

   void OnStorageUnitCreated (SNEEZE::SILO* pSilo, SNEEZE::eSILO_SCOPE eScope) override
   {
      if (*m_ppInspectorRml)
         (*m_ppInspectorRml)->OnStorageUnitChanged (pSilo, eScope);
   }

   void OnStorageUnitChanged (SNEEZE::SILO* pSilo, SNEEZE::eSILO_SCOPE eScope, const std::string&) override
   {
      if (*m_ppInspectorRml)
         (*m_ppInspectorRml)->OnStorageUnitChanged (pSilo, eScope);
   }

   void OnStorageUnitDeleted (SNEEZE::SILO* pSilo, SNEEZE::eSILO_SCOPE eScope) override
   {
      if (*m_ppInspectorRml)
         (*m_ppInspectorRml)->OnStorageUnitChanged (pSilo, eScope);
   }

private:
   INSPECTOR_RML** m_ppInspectorRml;
};

/*******************************************************************************************************************************
**                                                      Class: VIEWPORT_HOST                                                  **
*******************************************************************************************************************************/

class VIEWPORT_HOST_SDL : public SNEEZE::IVIEWPORT
{
public:
   VIEWPORT_HOST_SDL (RUBIDIUM::CANVAS* pCanvas) :
      m_pCanvas (pCanvas)
   {
   }

   void* FrameWindow () override                    { return m_pCanvas->NativeWindowHandle (); }

   bool FrameSize (int& nWidth, int& nHeight) override
   {
      return m_pCanvas->FrameSize (nWidth, nHeight);
   }

   void OnFrameReady (const uint32_t* pFB, int nFbW, int nFbH) override
   {
      m_pCanvas->Present (pFB, nFbW, nFbH);
   }

private:
   RUBIDIUM::CANVAS* m_pCanvas;
};

/*******************************************************************************************************************************
**                                                      Class: APPFRAMETAB_SDL                                                **
*******************************************************************************************************************************/

APPFRAMETAB_SDL::APPFRAMETAB_SDL (SNEEZE::ENGINE* pSneeze, LOGGER* pLogger) :
   m_pLogger       (pLogger),
   m_pSneeze       (pSneeze),
   m_pInspectorRml (nullptr),
   m_pSettingsRml  (nullptr),
   m_pCanvas       (new CANVAS_NATIVE (pLogger)),
   m_pContext      (nullptr),
   m_pViewport     (nullptr),
   m_pCtxHost      (nullptr),
   m_pVPHost       (nullptr),
   m_sTitle        ("New Tab"),
   m_eSession      (SNEEZE::CONTEXT::kSESSION_PERSISTENT),
   m_bActive       (false),
   m_bInputBlocked (false),
   m_nContentTop   (0)
{
}

APPFRAMETAB_SDL::~APPFRAMETAB_SDL ()
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

bool APPFRAMETAB_SDL::Init (void* pParentNative, int nContentTop, int nWidth, int nHeight, SNEEZE::CONTEXT::eSESSION eSession)
{
   bool bResult = false;

   m_nContentTop = nContentTop;
   m_eSession    = eSession;

   if (m_pCanvas->Initialize (pParentNative, nWidth, nHeight - nContentTop))
   {
      m_pCanvas->SetChildGeometry (0, nContentTop, nWidth, nHeight - nContentTop);

#ifndef RUBIDIUM_PLATFORM_QUEST
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
#endif

      m_pCtxHost = new CONTEXT_HOST (&m_pInspectorRml);
      m_pVPHost  = new VIEWPORT_HOST_SDL (m_pCanvas);

      nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();

      m_sUrl = jSettings["home"];

      bResult = CreateContext ();
   }

   return bResult;
}

bool APPFRAMETAB_SDL::CreateContext (bool bReset)
{
   bool bResult = false;

   if ((m_pContext = m_pSneeze->Context_Open (m_pCtxHost, m_sUrl, m_eSession, bReset)) != nullptr)
   {
      m_pViewport = m_pContext->Viewport ();

      if (m_pInspectorRml)
      {
         m_pInspectorRml->SetContext (m_pContext);
         m_pInspectorRml->SetScene   (m_pContext->Scene ());
      }

      bResult = true;
   }

   return bResult;
}

void APPFRAMETAB_SDL::DestroyContext ()
{
   if (m_bActive)
      ViewportDetach ();

   if (m_pContext)
   {
      m_pSneeze->Context_Close (m_pContext);
      m_pContext  = nullptr;
      m_pViewport = nullptr;
   }
}

void APPFRAMETAB_SDL::ProcessInput ()
{
   if (m_pViewport  &&  !m_bInputBlocked)
      m_pCanvas->ProcessInput (m_pViewport);

   if (m_pInspectorRml)
      m_pInspectorRml->ProcessPendingFiles ();
}

void APPFRAMETAB_SDL::ApplyMovementKeyEvent (SDL_Event& Event)
{
   if (m_pCanvas)
      m_pCanvas->ApplyMovementKeyEvent (Event);
}

void APPFRAMETAB_SDL::BlockInput (bool bBlocked)
{
   m_bInputBlocked = bBlocked;
}

void APPFRAMETAB_SDL::Show (bool bShow)
{
   m_pCanvas->SetVisible (bShow);
}

void APPFRAMETAB_SDL::Resize (int nWidth, int nHeight, int nContentTop)
{
   m_nContentTop = nContentTop;

   // CANVAS::Resize (invoked by SetChildGeometry) stages the new size; Sneeze
   // picks it up through the VIEWPORT_HOST_SDL::FrameSize callback, exactly as
   // the Win32 tab does -- no direct viewport resize call needed.
   m_pCanvas->SetChildGeometry (0, nContentTop, nWidth, nHeight - nContentTop);
}

void APPFRAMETAB_SDL::ViewportAttach ()
{
   m_bActive = true;

   if (m_pViewport)
      m_pViewport->Activate (m_pVPHost);
}

void APPFRAMETAB_SDL::ViewportDetach ()
{
   m_bActive = false;

   if (m_pViewport)
      m_pViewport->Deactivate ();
}

void APPFRAMETAB_SDL::ToggleInspector ()
{
   if (m_pInspectorRml)
      m_pInspectorRml->Toggle ();
}

void APPFRAMETAB_SDL::ShowSettings (void* pOwner)
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
      m_pSettingsRml->SetOwner (pOwner);

      // Always bring the (single) Settings window to the front rather than
      // toggling it -- de-minimizes it if the user had minimized it. Closing is
      // done via the window's own close button, matching the Win32 build.
      m_pSettingsRml->Show ();
   }
}

bool APPFRAMETAB_SDL::IsSettingsOpen () const
{
   return m_pSettingsRml  &&  m_pSettingsRml->IsOpen ();
}

const std::string& APPFRAMETAB_SDL::Title () const { return m_sTitle; }
const std::string& APPFRAMETAB_SDL::Url   () const { return m_sUrl; }
CANVAS*            APPFRAMETAB_SDL::Canvas () const { return m_pCanvas; }

void APPFRAMETAB_SDL::Title (const std::string& sTitle) { m_sTitle = sTitle; }

void APPFRAMETAB_SDL::Url (const std::string& sUrl)
{
   bool bActive = m_bActive;

   m_sUrl = sUrl;

   DestroyContext ();

   if (m_pInspectorRml)
      m_pInspectorRml->Reset ();

   if (CreateContext ())
   {
      if (bActive)
         ViewportAttach ();
   }
}

void APPFRAMETAB_SDL::Reload (bool bReset)
{
   bool bActive = m_bActive;

   DestroyContext ();

   if (m_pInspectorRml)
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
