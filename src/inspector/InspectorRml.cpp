// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "rmlui_sdl/RmlUi_SDL.h"

#include "inspector/InspectorRml.h"
#include "inspector/preview3d/Preview3D.h"
#include "inspector/elements/Elements.h"
#include "inspector/console/Console.h"
#include "inspector/sources/Sources.h"
#include "inspector/network/Network.h"
#include "inspector/performance/Performance.h"
#include "inspector/memory/Memory.h"
#include "inspector/storage/Storage.h"
#include "inspector/security/Security.h"

using namespace RUBIDIUM;

INSPECTOR_WIDGET::~INSPECTOR_WIDGET ()
{
   for (auto* pChild : m_apChildren)
      delete pChild;
}

bool INSPECTOR_WIDGET::CreateWidgets (const FN_CREATEWIDGET* afnCreateWidget, int nCount)
{
   bool bResult = true;

   m_apChildren.resize (nCount);

   for (int nWidget = 0; nWidget < nCount  &&  bResult; nWidget++)
   {
      m_apChildren[nWidget] = afnCreateWidget[nWidget] ();

      bResult = m_apChildren[nWidget]->Initialize (Rml::ElementUtilities::GetElementById (m_pContainer, m_apChildren[nWidget]->Id ()));
   }

   return bResult;
}

bool INSPECTOR_WIDGET::AddWidget (INSPECTOR_WIDGET* pWidget)
{
   m_apChildren.push_back (pWidget);

   return pWidget->Initialize (Rml::ElementUtilities::GetElementById (m_pContainer, pWidget->Id ()));
}

/*******************************************************************************************************************************
**                                                 Inspector Tab                                                              **
*******************************************************************************************************************************/

class INSPECTOR_TAB
{
public:
   INSPECTOR_TAB (Rml::EventListener* pRml) :
      m_pRml              (pRml),
      m_pPanel            (nullptr),
      m_pRmlElement_Tab   (nullptr),
      m_pRmlElement_Panel (nullptr)
   {
   }

   bool Initialize (Rml::ElementDocument* pRmlDocument, FN_CREATEWIDGET fnCreateWidget)
   {
      bool bResult = false;

      m_pPanel = fnCreateWidget ();

      m_pRmlElement_Tab   = pRmlDocument->GetElementById (Rml::String ("tab-"  ) + m_pPanel->Id ());
      m_pRmlElement_Panel = pRmlDocument->GetElementById (Rml::String ("panel-") + m_pPanel->Id ());

      if (m_pRmlElement_Tab  &&  m_pRmlElement_Panel)
      {
         m_pRmlElement_Tab->AddEventListener (Rml::EventId::Click, m_pRml);

         if (m_pPanel->Initialize (m_pRmlElement_Panel))
         {
            bResult = true;
         }
      }

      return bResult;
   }

   ~INSPECTOR_TAB ()
   {
      if (m_pPanel)
      {
         delete m_pPanel;
         m_pPanel = nullptr;
      }
   }

   bool Activate (Rml::Element* pRmlElement)
   {
      bool bActivated = (pRmlElement == m_pRmlElement_Tab);
      m_pRmlElement_Tab  ->SetClass ("active", bActivated);
      m_pRmlElement_Panel->SetClass ("active", bActivated);
      return bActivated;
   }

   INSPECTOR_WIDGET* Panel () const { return m_pPanel; }

private:
   Rml::EventListener*     m_pRml;
   INSPECTOR_WIDGET*       m_pPanel;
   Rml::Element*           m_pRmlElement_Tab;
   Rml::Element*           m_pRmlElement_Panel;
};

/*******************************************************************************************************************************
**                                                     Impl                                                                   **
*******************************************************************************************************************************/

class INSPECTOR_RML::Impl : public Rml::EventListener
{
public:
   enum
   {
      kTAB_ELEMENTS    = 0,
      kTAB_CONSOLE     = 1,
      kTAB_SOURCES     = 2,
      kTAB_NETWORK     = 3,
      kTAB_PERFORMANCE = 4,
      kTAB_MEMORY      = 5,
      kTAB_STORAGE     = 6,
      kTAB_SECURITY    = 7,
      kTAB_COUNT       = 8
   };

   static inline const FN_CREATEWIDGET s_afnCreateWidget[kTAB_COUNT] =
   {
      IW_ELEMENTS::Create,
      IW_CONSOLE::Create,
      IW_SOURCES::Create,
      IW_NETWORK::Create,
      IW_PERFORMANCE::Create,
      IW_MEMORY::Create,
      IW_STORAGE::Create,
      IW_SECURITY::Create,
   };

   static inline const char* s_sRmlStyle =
#include "inspector/InspectorRml_Style.inl"
   ;

   static inline const char* s_sRmlDocument = 
R"rml(
<rml>
<head>
<style>
[{STYLE}]
</style>
</head>
<body>
<div class="tabbar">
<div class="tab"          id="tab-elements">Elements</div>
<div class="tab"          id="tab-console">Console</div>
<div class="tab"          id="tab-sources">Sources</div>
<div class="tab   active" id="tab-network">Network</div>
<div class="tab"          id="tab-performance">Performance</div>
<div class="tab"          id="tab-memory">Memory</div>
<div class="tab"          id="tab-storage">Storage</div>
<div class="tab"          id="tab-security">Security</div>
</div>
<div class="panel"        id="panel-elements"><div class="placeholder">Elements</div></div>
<div class="panel"        id="panel-console"><div class="placeholder">Console</div></div>
<div class="panel"        id="panel-sources"><div class="placeholder">Sources</div></div>
<div class="panel active" id="panel-network"></div>
<div class="panel"        id="panel-performance"></div>
<div class="panel"        id="panel-memory"><div class="placeholder">Memory</div></div>
<div class="panel"        id="panel-storage"></div>
<div class="panel"        id="panel-security"></div>
</body>
</rml>
)rml";

   Impl () :
      m_nActiveTab (kTAB_NETWORK),
      m_pContext            (nullptr),
      m_pNetwork_Toolbar    (nullptr),
      m_pScene              (nullptr),
      m_pEngine             (nullptr),
      m_pPreview3D          (nullptr),
      m_nLoadedGlbGen       (0),
      m_nElementsTick       (0),
      m_bElementsWasVisible (false),
      m_bPreviewNeedsLayout (false),
      m_nLastNetworkDetailTab (-1)
   {
      for (int nTab = 0; nTab < kTAB_COUNT; nTab++)
         m_apTab[nTab] = nullptr;
   }

   static void OnGeometryChanged (int nX, int nY, int nWidth, int nHeight, bool bMaximized, void* pUserData)
   {
      auto& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();
      jSettings["inspector_rml"]["x"]         = nX;
      jSettings["inspector_rml"]["y"]         = nY;
      jSettings["inspector_rml"]["width"]     = nWidth;
      jSettings["inspector_rml"]["height"]    = nHeight;
      jSettings["inspector_rml"]["maximized"] = bMaximized;
   }

   static void OnVisibilityChanged (bool bVisible, void* pUserData)
   {
      Impl* pThis = static_cast<Impl*> (pUserData);

      if (!bVisible  &&  pThis->m_pPreview3D)
         pThis->m_pPreview3D->Hide ();
   }

   bool Initialize ()
   {
      bool bResult = false;
      size_t nPos;

      auto& jInsp = APPNATIVE::GetInstance ()->SettingToJSON ()["inspector_rml"];
      int nWidth  = jInsp.value ("width",  1280);
      int nHeight = jInsp.value ("height", 720);

      if (m_Window.Initialize ("Rubidium Inspector", nWidth, nHeight))
      {
         int nX = jInsp.value ("x", -1);
         int nY = jInsp.value ("y", -1);
         if (nX >= 0 && nY >= 0)
            m_Window.SetPosition (nX, nY);

         m_Window.SetGeometryCallback (OnGeometryChanged, this);
         m_Window.SetVisibilityCallback (OnVisibilityChanged, this);
         std::string sRmlStyle (s_sRmlStyle);
         nPos = sRmlStyle.find ("[{FONT-FAMILY}]");
         if (nPos != std::string::npos)
            sRmlStyle.replace (nPos, 15, APPNATIVE::GetInstance ()->sFontFamily ());

         std::string sRmlDocument (s_sRmlDocument);
         nPos = sRmlDocument.find ("[{STYLE}]");
         if (nPos != std::string::npos)
            sRmlDocument.replace (nPos, 9, sRmlStyle);

         if (m_Window.LoadDocument (sRmlDocument))
         {
            bResult = true;

            for (int nTab = 0; nTab < kTAB_COUNT  &&  bResult; nTab++)
            {
               m_apTab[nTab] = new INSPECTOR_TAB (this);

               bResult = m_apTab[nTab]->Initialize (m_Window.Document (), s_afnCreateWidget[nTab]);
            }

            if (bResult)
               m_Window.Document ()->AddEventListener (Rml::EventId::Keydown, this);
         }
      }

      return bResult;
   }

   ~Impl ()
   {
      // Tear the live preview down before the inspector window (m_Window, a
      // member destroyed after this body) so the child render window still has
      // a valid parent while Context_Close winds down its renderer.
      if (m_pPreview3D)
      {
         delete m_pPreview3D;
         m_pPreview3D = nullptr;
      }

      if (m_Window.Document ())
         m_Window.Document ()->RemoveEventListener (Rml::EventId::Keydown, this);

      // Widgets detach their own RmlUi event listeners in their destructors, so
      // they must run while the document's elements are still alive. The RmlUi
      // document (m_Window member) is destroyed after this body, so deleting the
      // widgets here is correct -- do NOT tear the document down first.
      for (int nTab = 0; nTab < kTAB_COUNT; nTab++)
      {
         if (m_apTab[nTab])
         {
            delete m_apTab[nTab];
            m_apTab[nTab] = nullptr;
         }
      }
   }

   void ProcessEvent (Rml::Event& Event) override
   {
      if (Event.GetId () == Rml::EventId::Click)
      {
         Rml::Element* pRmlElement = Event.GetCurrentElement ();

         for (int nTab = 0; nTab < kTAB_COUNT; nTab++)
         {
            if (m_apTab[nTab] && m_apTab[nTab]->Activate (pRmlElement))
               m_nActiveTab = nTab;
         }
      }
      else if (Event.GetId () == Rml::EventId::Keydown)
      {
         bool bCtrl = Event.GetParameter<bool> ("ctrl_key", false);
         auto nKey  = Event.GetParameter<int> ("key_identifier", 0);

         if (nKey == Rml::Input::KI_F12)
         {
            // F12 never closes the inspector -- it only opens / raises it. The
            // sole close path is the window's own close (X) button, matching the
            // Windows build. When the inspector already has focus this is just a
            // harmless raise. (On Win32 the accelerator consumes F12 before it
            // reaches RmlUi; on SDL platforms App_SDL forwards it here.)
            m_Window.Show ();
         }
         else if (bCtrl && nKey == Rml::Input::KI_F)
         {
            if (m_nActiveTab == kTAB_ELEMENTS)
            {
               auto* pTree = static_cast<IW_ELEMENTS*> (m_apTab[kTAB_ELEMENTS]->Panel ())->Tree ();
               if (pTree->IsSearchVisible ())
                  pTree->HideSearch ();
               else
                  pTree->ShowSearch ();
            }
         }
      }
   }

   IW_NETWORK_FILES* NetworkFiles ()
   {
      IW_NETWORK_FILES* pResult = nullptr;

      if (m_apTab[kTAB_NETWORK] && m_apTab[kTAB_NETWORK]->Panel ())
      {
         auto* pNetwork = static_cast<IW_NETWORK*> (m_apTab[kTAB_NETWORK]->Panel ());
         auto* pBody    = static_cast<IW_NETWORK_BODY*> (pNetwork->Widget (IW_NETWORK::kWIDGET_BODY));
         pResult = pBody->Files ();
      }

      return pResult;
   }

   IW_NETWORK_BODY* NetworkBody ()
   {
      IW_NETWORK_BODY* pResult = nullptr;

      if (m_apTab[kTAB_NETWORK] && m_apTab[kTAB_NETWORK]->Panel ())
      {
         auto* pNetwork = static_cast<IW_NETWORK*> (m_apTab[kTAB_NETWORK]->Panel ());
         pResult = static_cast<IW_NETWORK_BODY*> (pNetwork->Widget (IW_NETWORK::kWIDGET_BODY));
      }

      return pResult;
   }

   // Lazily spin up the preview viewport on first use -- most sessions never
   // preview a glb, so we avoid the cost of a second Halogen device until one
   // is actually selected. Needs the engine (SetEngine) and the inspector
   // window's native handle (available once m_Window.Initialize has run).
   bool EnsurePreview3D ()
   {
      if (!m_pPreview3D  &&  m_pEngine)
      {
         m_pPreview3D = new PREVIEW3D (APPNATIVE::GetInstance ()->Logger ());

         if (!m_pPreview3D->Initialize (m_pEngine, m_Window.ParentNativeHandle ()))
         {
            delete m_pPreview3D;
            m_pPreview3D = nullptr;
         }
      }

      return m_pPreview3D != nullptr;
   }

   // Measure the dock rectangle for the live glb preview. Uses the sized
   // #preview-3d-host block when present, adjusts for scrollable ancestors, and
   // falls back to the preview panel width + authored 360dp height when layout
   // has not caught up yet (common right after un-hiding the detail pane).
   bool PreviewDockRect (Rml::Element* pMeasureEl, Rml::Element* pPanel, Rml::Context* pContext,
                         int& nX, int& nY, int& nW, int& nH)
   {
      bool bResult = false;

      if (pMeasureEl)
      {
         Rml::Vector2f vOffset = pMeasureEl->GetAbsoluteOffset (Rml::BoxArea::Border);
         Rml::Vector2f vSize   = pMeasureEl->GetBox ().GetSize (Rml::BoxArea::Border);

         float dX = vOffset.x;
         float dY = vOffset.y;

         for (Rml::Element* pNode = pMeasureEl->GetParentNode (); pNode; pNode = pNode->GetParentNode ())
         {
            dX -= pNode->GetScrollLeft ();
            dY -= pNode->GetScrollTop  ();
         }

         nX = static_cast<int> (dX + 0.5f);
         nY = static_cast<int> (dY + 0.5f);
         nW = static_cast<int> (vSize.x + 0.5f);
         nH = static_cast<int> (vSize.y + 0.5f);

         if ((nW < 8 || nH < 8) && pPanel)
         {
            Rml::Vector2f vPanelSize = pPanel->GetBox ().GetSize (Rml::BoxArea::Border);

            if (nW < 8 && vPanelSize.x > 0.0f)
               nW = static_cast<int> (vPanelSize.x + 0.5f);

            if (nH < 8 && pContext)
            {
               float dDpi = pContext->GetDensityIndependentPixelRatio ();
               nH = static_cast<int> (360.0f * dDpi + 0.5f);
            }
         }

         bResult = (nW >= 8 && nH >= 8);
      }

      return bResult;
   }

   // Per-frame driver for the live glb preview. Shows + docks the child render
   // window only when the inspector is visible, the Network tab is up, the
   // detail pane is open on its Preview sub-tab, and the selected file is a
   // glb; otherwise it hides the window. Called once per frame from
   // ProcessPendingFiles.
   void UpdatePreview3D ()
   {
      IW_NETWORK_DETAIL_PREVIEW* pPreview = nullptr;
      IW_NETWORK_BODY*           pBody    = NetworkBody ();
      IW_NETWORK_DETAILS*        pDetails = pBody ? pBody->Details () : nullptr;

      if (pDetails)
         pPreview = pDetails->TabPreview ();

      bool bWantShow =
         m_pEngine != nullptr &&
         m_Window.IsVisible () &&
         m_nActiveTab == kTAB_NETWORK &&
         pBody     && pBody->IsDetailsVisible () &&
         pDetails  && pDetails->ActiveTab () == IW_NETWORK_DETAILS::kWIDGET_PREVIEW &&
         pPreview  && pPreview->IsGlb ();

      if (bWantShow  &&  EnsurePreview3D ())
      {
         Rml::ElementDocument* pDocument = m_Window.Document ();
         Rml::Context*         pContext  = pDocument ? pDocument->GetContext () : nullptr;
         Rml::Element*         pHost     = pPreview->PreviewHost ();
         Rml::Element*         pElement  = pHost ? pHost : pPreview->PreviewBody ();

         if (pDetails->ActiveTab () != m_nLastNetworkDetailTab)
         {
            if (pDetails->ActiveTab () == IW_NETWORK_DETAILS::kWIDGET_PREVIEW)
               m_bPreviewNeedsLayout = true;

            m_nLastNetworkDetailTab = pDetails->ActiveTab ();
         }

         if (pElement)
         {
            if (m_nLoadedGlbGen != pPreview->GlbGeneration ())
            {
               const std::vector<uint8_t>& aData = pPreview->GlbData ();
               m_pPreview3D->LoadGlb (aData.data (), aData.size ());
               m_nLoadedGlbGen       = pPreview->GlbGeneration ();
               m_bPreviewNeedsLayout = true;
            }

            if (m_bPreviewNeedsLayout || !m_pPreview3D->IsShown ())
            {
               m_Window.Render ();
               m_bPreviewNeedsLayout = false;
            }

            int nX = 0;
            int nY = 0;
            int nW = 0;
            int nH = 0;

            if (!PreviewDockRect (pElement, pPreview->Container (), pContext, nX, nY, nW, nH))
            {
               m_Window.Render ();
               PreviewDockRect (pElement, pPreview->Container (), pContext, nX, nY, nW, nH);
            }

            // Clamp to the inspector's client rect so a partly-scrolled pane
            // doesn't let the child window spill past the window edges.
            if (pContext)
            {
               Rml::Vector2i vDim = pContext->GetDimensions ();

               if (nX < 0)            { nW += nX; nX = 0; }
               if (nY < 0)            { nH += nY; nY = 0; }
               if (nX + nW > vDim.x)  { nW = vDim.x - nX; }
               if (nY + nH > vDim.y)  { nH = vDim.y - nY; }
            }

            if (nW >= 8  &&  nH >= 8)
            {
               m_pPreview3D->SetGeometry (nX, nY, nW, nH);
               m_pPreview3D->Show ();
               m_pPreview3D->Tick ();
            }
            else
               m_pPreview3D->Hide ();
         }
      }
      else
      {
         m_nLastNetworkDetailTab = pDetails ? pDetails->ActiveTab () : -1;

         if (m_pPreview3D)
            m_pPreview3D->Hide ();
      }
   }

   IW_NETWORK_STATUSBAR* NetworkStatusbar ()
   {
      IW_NETWORK_STATUSBAR* pResult = nullptr;

      if (m_apTab[kTAB_NETWORK] && m_apTab[kTAB_NETWORK]->Panel ())
      {
         auto* pNetwork = static_cast<IW_NETWORK*> (m_apTab[kTAB_NETWORK]->Panel ());
         pResult = static_cast<IW_NETWORK_STATUSBAR*> (pNetwork->Widget (IW_NETWORK::kWIDGET_STATUSBAR));
      }

      return pResult;
   }

   IW_CONSOLE_ENTRIES* ConsoleEntries ()
   {
      IW_CONSOLE_ENTRIES* pResult = nullptr;

      if (m_apTab[kTAB_CONSOLE] && m_apTab[kTAB_CONSOLE]->Panel ())
      {
         auto* pConsole = static_cast<IW_CONSOLE*> (m_apTab[kTAB_CONSOLE]->Panel ());
         auto* pBody = static_cast<IW_CONSOLE_BODY*> (pConsole->Widget (IW_CONSOLE::kWIDGET_BODY));
         pResult = pBody->Entries ();
      }

      return pResult;
   }

   IW_CONSOLE_BODY* ConsoleBody ()
   {
      IW_CONSOLE_BODY* pResult = nullptr;

      if (m_apTab[kTAB_CONSOLE] && m_apTab[kTAB_CONSOLE]->Panel ())
      {
         auto* pConsole = static_cast<IW_CONSOLE*> (m_apTab[kTAB_CONSOLE]->Panel ());
         pResult = static_cast<IW_CONSOLE_BODY*> (pConsole->Widget (IW_CONSOLE::kWIDGET_BODY));
      }

      return pResult;
   }

   IW_ELEMENTS_CONTAINERS* ElementsContainers ()
   {
      IW_ELEMENTS_CONTAINERS* pResult = nullptr;

      if (m_apTab[kTAB_ELEMENTS] && m_apTab[kTAB_ELEMENTS]->Panel ())
      {
         auto* pElements = static_cast<IW_ELEMENTS*> (m_apTab[kTAB_ELEMENTS]->Panel ());
         pResult = pElements->Containers ();
      }

      return pResult;
   }

   IW_ELEMENTS_TREE* ElementsTree ()
   {
      IW_ELEMENTS_TREE* pResult = nullptr;

      if (m_apTab[kTAB_ELEMENTS] && m_apTab[kTAB_ELEMENTS]->Panel ())
      {
         auto* pElements = static_cast<IW_ELEMENTS*> (m_apTab[kTAB_ELEMENTS]->Panel ());
         pResult = pElements->Tree ();
      }

      return pResult;
   }

   IW_STORAGE_CONTAINERS* StorageContainers ()
   {
      IW_STORAGE_CONTAINERS* pResult = nullptr;

      if (m_apTab[kTAB_STORAGE] && m_apTab[kTAB_STORAGE]->Panel ())
      {
         auto* pStorage = static_cast<IW_STORAGE*> (m_apTab[kTAB_STORAGE]->Panel ());
         pResult = pStorage->Containers ();
      }

      return pResult;
   }

   IW_STORAGE_PREVIEW* StoragePreview ()
   {
      IW_STORAGE_PREVIEW* pResult = nullptr;

      if (m_apTab[kTAB_STORAGE] && m_apTab[kTAB_STORAGE]->Panel ())
      {
         auto* pStorage = static_cast<IW_STORAGE*> (m_apTab[kTAB_STORAGE]->Panel ());
         pResult = pStorage->Preview ();
      }

      return pResult;
   }

   RMLUI_SDL       m_Window;
   INSPECTOR_TAB*  m_apTab[kTAB_COUNT];
   int             m_nActiveTab;

   IW_NETWORK_TOOLBAR*                 m_pNetwork_Toolbar;

   SNEEZE::CONTEXT*                    m_pContext;
   std::vector<SNEEZE::CACHE*>         m_apCache;
   SNEEZE::SCENE*                      m_pScene;
   SNEEZE::ENGINE*                     m_pEngine;
   PREVIEW3D*                          m_pPreview3D;
   uint64_t                            m_nLoadedGlbGen;
   int                                 m_nElementsTick;
   bool                                m_bElementsWasVisible;
   bool                                m_bPreviewNeedsLayout;
   int                                 m_nLastNetworkDetailTab;
   std::mutex                          m_mutex;
   std::vector<SNEEZE::FILE*> m_aFilesCreated;
   std::vector<SNEEZE::FILE*> m_aFilesChanged;
   std::vector<SNEEZE::FILE*> m_aFilesDeleted;
   std::vector<std::shared_ptr<const SNEEZE::ENTRY>> m_aEntryCreated;
   std::vector<std::shared_ptr<const SNEEZE::ENTRY>> m_aEntryDeleted;
   std::vector<SNEEZE::SILO*> m_aSiloCreated;
   std::vector<SNEEZE::SILO*> m_aSiloDeleted;
   std::vector<SNEEZE::SILO*> m_aSiloChanged;
};


/*******************************************************************************************************************************
**                                                 INSPECTOR_RML                                                              **
*******************************************************************************************************************************/

INSPECTOR_RML::INSPECTOR_RML () :
   m_pImpl (new Impl ())
{
}

bool INSPECTOR_RML::Initialize ()
{
   return m_pImpl->Initialize ();
}

INSPECTOR_RML::~INSPECTOR_RML ()
{
   delete m_pImpl;
   m_pImpl = nullptr;
}

bool INSPECTOR_RML::IsVisible  () const { return m_pImpl->m_Window.IsVisible (); }

void INSPECTOR_RML::Render ()            {        m_pImpl->m_Window.Render (); }
// F12 and the ellipsis-menu "Inspector" item open / raise the window but never
// close it -- the only close path is the window's close (X) button. Named
// Toggle for its callers (ToggleInspector), but intentionally show-only.
void INSPECTOR_RML::Toggle ()            {        m_pImpl->m_Window.Show ();        }

void INSPECTOR_RML::SetContext (SNEEZE::CONTEXT* pContext)
{
   m_pImpl->m_pContext = pContext;

   if (m_pImpl->m_apTab[Impl::kTAB_NETWORK]  &&  m_pImpl->m_apTab[Impl::kTAB_NETWORK]->Panel ())
   {
      auto* pNetworkPanel = static_cast<IW_NETWORK*> (m_pImpl->m_apTab[Impl::kTAB_NETWORK]->Panel ());
      m_pImpl->m_pNetwork_Toolbar = static_cast<IW_NETWORK_TOOLBAR*> (pNetworkPanel->Widget (IW_NETWORK::kWIDGET_TOOLBAR));

      if (m_pImpl->m_pNetwork_Toolbar)
      {
         m_pImpl->m_pNetwork_Toolbar->SetContext (pContext);
      }
   }

   // Storage tab: wire the sidebar to the preview pane, then seed the sidebar
   // with any silos that already exist. Silos created later arrive live via
   // OnStorageSiloCreated. (Silo_Enum may find nothing yet -- WASM containers
   // often instantiate their silos after the context opens.)
   IW_STORAGE_CONTAINERS* pStorageContainers = m_pImpl->StorageContainers ();
   IW_STORAGE_PREVIEW*    pStoragePreview    = m_pImpl->StoragePreview ();

   if (pStorageContainers && pStoragePreview)
      pStorageContainers->SetPreview (pStoragePreview);

   if (pContext && pContext->Storage () && pStorageContainers)
   {
      struct ENUM_SILO : SNEEZE::IENUM_SILO
      {
         IW_STORAGE_CONTAINERS* m_pContainers;

         void OnSilo (SNEEZE::SILO* pSilo) override
         {
            m_pContainers->AddSilo (pSilo);
         }
      };

      ENUM_SILO Enum;
      Enum.m_pContainers = pStorageContainers;
      pContext->Storage ()->Silo_Enum (&Enum);
   }
}

void INSPECTOR_RML::SetScene (SNEEZE::SCENE* pScene)
{
   m_pImpl->m_pScene = pScene;

   if (IW_ELEMENTS_TREE* pTree = m_pImpl->ElementsTree ())
      pTree->SetScene (pScene);
}

void INSPECTOR_RML::SetEngine (SNEEZE::ENGINE* pEngine)
{
   m_pImpl->m_pEngine = pEngine;
}

void INSPECTOR_RML::Reset ()
{
   // Drop queued notifications referencing the old network's FILE/ENTRY objects
   // so ProcessPendingFiles () never touches them after the network is swapped.
   {
      std::lock_guard<std::mutex> Lock (m_pImpl->m_mutex);
      m_pImpl->m_aFilesCreated.clear ();
      m_pImpl->m_aFilesChanged.clear ();
      m_pImpl->m_aFilesDeleted.clear ();
      m_pImpl->m_aEntryCreated.clear ();
      m_pImpl->m_aEntryDeleted.clear ();
      m_pImpl->m_aSiloCreated.clear ();
      m_pImpl->m_aSiloDeleted.clear ();
      m_pImpl->m_aSiloChanged.clear ();
   }

   // Network panel: detach the pinned detail file, empty the list + origins,
   // and reset the origin filter and status bar.
   if (IW_NETWORK_BODY* pBody = m_pImpl->NetworkBody ())
   {
      pBody->ResetDetails ();

      if (IW_NETWORK_FILES* pFiles = pBody->Files ())
      {
         pFiles->Clear ();
         pFiles->FilterContainer ("");
      }
      if (IW_NETWORK_CONTAINERS* pContainers = pBody->Containers ())
         pContainers->Clear ();
   }

   if (IW_NETWORK_STATUSBAR* pStatusbar = m_pImpl->NetworkStatusbar ())
      pStatusbar->Update (0, 0, 0, false);

   // Console panel: empty entries + origins.
   if (IW_CONSOLE_BODY* pConsoleBody = m_pImpl->ConsoleBody ())
   {
      if (IW_CONSOLE_ENTRIES* pEntries = pConsoleBody->Entries ())
         pEntries->Clear ();
      if (IW_CONSOLE_CONTAINERS* pContainers = pConsoleBody->Containers ())
         pContainers->Clear ();
   }

   // Storage panel: detach + drop the shown silo, empty origins. The silos
   // themselves belong to the (soon-to-be-closed) context; we only drop handles.
   if (IW_STORAGE_PREVIEW* pStoragePreview = m_pImpl->StoragePreview ())
      pStoragePreview->DropSilo ();

   if (IW_STORAGE_CONTAINERS* pStorageContainers = m_pImpl->StorageContainers ())
      pStorageContainers->Clear ();

   // Elements panel: empty origins and the fabric/node tree.
   if (IW_ELEMENTS_CONTAINERS* pElementsContainers = m_pImpl->ElementsContainers ())
      pElementsContainers->Clear ();

   if (IW_ELEMENTS_TREE* pElementsTree = m_pImpl->ElementsTree ())
      pElementsTree->SetScene (nullptr);

   m_pImpl->m_pContext = nullptr;
   m_pImpl->m_pScene   = nullptr;

   m_pImpl->m_Window.Render ();
}

void INSPECTOR_RML::OnNetworkCacheCreated (SNEEZE::CACHE* pCache)
{
   if (pCache)
   {
      auto& apCache = m_pImpl->m_apCache;

      if (std::find (apCache.begin (), apCache.end (), pCache) == apCache.end ())
         apCache.push_back (pCache);
   }
}

void INSPECTOR_RML::OnNetworkCacheDeleted (SNEEZE::CACHE* pCache)
{
   if (pCache)
   {
      auto& apCache = m_pImpl->m_apCache;
      auto  it      = std::find (apCache.begin (), apCache.end (), pCache);

      if (it != apCache.end ())
         apCache.erase (it);
   }
}

/*
File_Enum was moved from NETWORK to CACHE, for which we have no way to call at the moment.
Fortunately, no one calls this function, so it's easiest to just comment it out.

void INSPECTOR_RML::LoadHistory ()
{
   IW_NETWORK_FILES* pFiles = m_pImpl->NetworkFiles ();

   if (pFiles)
   {
      IW_NETWORK_BODY* pBody = m_pImpl->NetworkBody ();
      IW_NETWORK_CONTAINERS* pContainers = pBody ? pBody->Containers () : nullptr;

      IW_ELEMENTS_CONTAINERS* pElementsContainers = m_pImpl->ElementsContainers ();

      struct ENUM_LOAD : SNEEZE::IENUM_FILE
      {
         IW_NETWORK_FILES* m_pFiles;
         IW_NETWORK_CONTAINERS* m_pContainers;
         IW_ELEMENTS_CONTAINERS* m_pElementsContainers;

         void OnAsset (SNEEZE::FILE* pFile) override
         {
            m_pFiles->AddFile (pFile);

            if (m_pContainers)
               m_pContainers->AddContainer (pFile->ContainerName ());
            if (m_pElementsContainers)
               m_pElementsContainers->AddContainer (pFile->ContainerName ());
         }
      };

      ENUM_LOAD Enum;
      Enum.m_pFiles = pFiles;
      Enum.m_pContainers = pContainers;
      Enum.m_pElementsContainers = pElementsContainers;

      // File_Enum lives on CACHE (per-container), not NETWORK, since the
      // network refactor split file handles by container.
      for (SNEEZE::CACHE* pCache : m_pImpl->m_apCache)
         pCache->File_Enum (&Enum);
   }
}
*/

bool INSPECTOR_RML::OnNetworkFileCreated (SNEEZE::FILE* pFile)
{
   std::lock_guard<std::mutex> Lock (m_pImpl->m_mutex);
   m_pImpl->m_aFilesCreated.push_back (pFile);

   return (m_pImpl->m_pNetwork_Toolbar == nullptr || m_pImpl->m_pNetwork_Toolbar->IsRecording ());
}

void INSPECTOR_RML::OnNetworkFileChanged (SNEEZE::FILE* pFile)
{
   std::lock_guard<std::mutex> Lock (m_pImpl->m_mutex);
   m_pImpl->m_aFilesChanged.push_back (pFile);
}

void INSPECTOR_RML::OnNetworkFileDeleted (SNEEZE::FILE* pFile)
{
   std::lock_guard<std::mutex> Lock (m_pImpl->m_mutex);
   m_pImpl->m_aFilesDeleted.push_back (pFile);
}

void INSPECTOR_RML::OnConsoleEntryCreated (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr)
{
   std::lock_guard<std::mutex> Lock (m_pImpl->m_mutex);
   m_pImpl->m_aEntryCreated.push_back (pEntryPtr);
}

void INSPECTOR_RML::OnConsoleEntryDeleted (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr)
{
   std::lock_guard<std::mutex> Lock (m_pImpl->m_mutex);
   m_pImpl->m_aEntryDeleted.push_back (pEntryPtr);
}

void INSPECTOR_RML::OnStorageSiloCreated (SNEEZE::SILO* pSilo)
{
   std::lock_guard<std::mutex> Lock (m_pImpl->m_mutex);
   m_pImpl->m_aSiloCreated.push_back (pSilo);
}

void INSPECTOR_RML::OnStorageSiloDeleted (SNEEZE::SILO* pSilo)
{
   std::lock_guard<std::mutex> Lock (m_pImpl->m_mutex);
   m_pImpl->m_aSiloDeleted.push_back (pSilo);
}

void INSPECTOR_RML::OnStorageUnitChanged (SNEEZE::SILO* pSilo, SNEEZE::eSILO_SCOPE /*eScope*/)
{
   std::lock_guard<std::mutex> Lock (m_pImpl->m_mutex);
   m_pImpl->m_aSiloChanged.push_back (pSilo);
}

void INSPECTOR_RML::ProcessPendingFiles ()
{
   std::vector<SNEEZE::FILE*> aCreated;
   std::vector<SNEEZE::FILE*> aChanged;
   std::vector<SNEEZE::FILE*> aDeleted;
   std::vector<std::shared_ptr<const SNEEZE::ENTRY>> aEntryCreated;
   std::vector<std::shared_ptr<const SNEEZE::ENTRY>> aEntryDeleted;
   std::vector<SNEEZE::SILO*> aSiloCreated;
   std::vector<SNEEZE::SILO*> aSiloDeleted;
   std::vector<SNEEZE::SILO*> aSiloChanged;

   {
      std::lock_guard<std::mutex> Lock (m_pImpl->m_mutex);
      aCreated.swap (m_pImpl->m_aFilesCreated);
      aChanged.swap (m_pImpl->m_aFilesChanged);
      aDeleted.swap (m_pImpl->m_aFilesDeleted);
      aEntryCreated.swap (m_pImpl->m_aEntryCreated);
      aEntryDeleted.swap (m_pImpl->m_aEntryDeleted);
      aSiloCreated.swap (m_pImpl->m_aSiloCreated);
      aSiloDeleted.swap (m_pImpl->m_aSiloDeleted);
      aSiloChanged.swap (m_pImpl->m_aSiloChanged);
   }

   if (aCreated.empty () == false || aChanged.empty () == false || aDeleted.empty () == false)
   {
      IW_NETWORK_FILES* pFiles = m_pImpl->NetworkFiles ();
      if (pFiles)
      {
         IW_NETWORK_BODY* pBody = m_pImpl->NetworkBody ();
         IW_NETWORK_CONTAINERS* pContainers = pBody ? pBody->Containers () : nullptr;
         IW_ELEMENTS_CONTAINERS* pElementsContainers = m_pImpl->ElementsContainers ();

         for (auto* pFile : aCreated)
         {
            pFiles->AddFile (pFile);

            // Surface the file's origin in the Origins sidebar (AddContainer de-dupes).
            if (pContainers)
               pContainers->AddContainer (pFile->ContainerName ());
            if (pElementsContainers)
               pElementsContainers->AddContainer (pFile->ContainerName ());
         }

         for (auto* pFile : aChanged)
            pFiles->UpdateFile (pFile);

         for (auto* pFile : aDeleted)
         {
            if (pBody)
               pBody->OnFileDeleted (pFile);
            pFiles->RemoveFile (pFile);
         }

         IW_NETWORK_STATUSBAR* pStatusbar = m_pImpl->NetworkStatusbar ();
         if (pStatusbar)
            pStatusbar->Update (pFiles->FileCount (), 0, 0, false);

         m_pImpl->m_Window.Render ();
      }
   }

   if (aEntryCreated.empty () == false || aEntryDeleted.empty () == false)
   {
      IW_CONSOLE_ENTRIES* pConsoleEntries = m_pImpl->ConsoleEntries ();

      if (pConsoleEntries)
      {
         IW_CONSOLE_BODY* pBody = m_pImpl->ConsoleBody ();
         IW_CONSOLE_CONTAINERS* pContainers = pBody ? pBody->Containers () : nullptr;

         for (std::shared_ptr<const SNEEZE::ENTRY> pEntry : aEntryCreated)
         {
            pConsoleEntries->AddEntry (pEntry);

            // Surface the file's origin in the Origins sidebar (AddContainer de-dupes).
            if (pContainers)
               pContainers->AddContainer (pEntry->Container ()->Identity ()->DisplayName ());
         }

         for (std::shared_ptr<const SNEEZE::ENTRY> pEntry : aEntryDeleted)
         {
            if (pBody)
               pBody->onEntryDeleted (pEntry);
            pConsoleEntries->RemoveEntry (pEntry);
         }

         IW_NETWORK_STATUSBAR* pStatusbar = m_pImpl->NetworkStatusbar ();
         if (pStatusbar)
            pStatusbar->Update (pConsoleEntries->EntryCount (), 0, 0, false);

         m_pImpl->m_Window.Render ();
      }
   }

   if (aSiloCreated.empty () == false || aSiloDeleted.empty () == false || aSiloChanged.empty () == false)
   {
      IW_STORAGE_CONTAINERS* pStorageContainers = m_pImpl->StorageContainers ();
      IW_STORAGE_PREVIEW*    pStoragePreview    = m_pImpl->StoragePreview ();

      if (pStorageContainers)
      {
         for (auto* pSilo : aSiloCreated)
            pStorageContainers->AddSilo (pSilo);

         for (auto* pSilo : aSiloDeleted)
            pStorageContainers->RemoveSilo (pSilo);
      }

      // A unit-change on the silo currently shown in the preview means its
      // key/value view is stale -- re-render it in place.
      if (pStoragePreview)
      {
         for (auto* pSilo : aSiloChanged)
         {
            if (pSilo == pStoragePreview->CurrentSilo ())
            {
               pStoragePreview->Refresh ();
               break;
            }
         }
      }

      m_pImpl->m_Window.Render ();
   }

   // Refresh the Elements fabric/node tree. The scene is built/mutated on other
   // threads with no change notification, so we poll: rebuild immediately when
   // the inspector becomes visible, then periodically (~every 30 ticks) while it
   // stays open. Throttled so we don't rebuild the DOM every frame.
   bool bVisible = m_pImpl->m_Window.IsVisible ();

   if (m_pImpl->m_pScene  &&  bVisible)
   {
      bool bForce = !m_pImpl->m_bElementsWasVisible;

      if (bForce  ||  ++m_pImpl->m_nElementsTick >= 30)
      {
         m_pImpl->m_nElementsTick = 0;

         if (IW_ELEMENTS_TREE* pTree = m_pImpl->ElementsTree ())
         {
            if (pTree->Rebuild ())
               m_pImpl->m_Window.Render ();
         }
      }
   }

   m_pImpl->m_bElementsWasVisible = bVisible;

   // Drive the live glb preview window (show / dock / hide) once per frame.
   m_pImpl->UpdatePreview3D ();
}
