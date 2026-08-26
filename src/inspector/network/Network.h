// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_NETWORK_H
#define RUBIDIUM_INSPECTOR_NETWORK_H

#include "inspector/InspectorRml.h"
#include "inspector/common/Button.h"

namespace RUBIDIUM
{

// Network file filter categories. Shared by the toolbar (button mapping) and
// the file list (row classification); the two must stay in the same order.
enum eNET_FILTER
{
   kFILTER_ALL,
   kFILTER_MSF,
   kFILTER_WASM,
   kFILTER_GLTF,
   kFILTER_SOCKET,
   kFILTER_IMG,
   kFILTER_OTHER,

   kFILTER_COUNT
};

/*******************************************************************************************************************************
**                                               Detail Sub-Tab Components                                                    **
*******************************************************************************************************************************/

class IW_NETWORK_DETAIL_HEADERS : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_NETWORK_DETAIL_HEADERS ();
   ~IW_NETWORK_DETAIL_HEADERS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK_DETAIL_HEADERS (); }
   const char* Id () override { return "network-detail-headers"; }
   bool Initialize (Rml::Element* pContainer) override;

   void ShowFile (SNEEZE::FILE* pFile);
   void ProcessEvent (Rml::Event& Event) override;

private:
   std::vector<Rml::Element*> m_apSections;
};

class IW_NETWORK_DETAIL_PREVIEW : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_NETWORK_DETAIL_PREVIEW ();
   ~IW_NETWORK_DETAIL_PREVIEW () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK_DETAIL_PREVIEW (); }
   const char* Id () override { return "network-detail-preview"; }
   bool Initialize (Rml::Element* pContainer) override;

   void ShowFile (SNEEZE::FILE* pFile);
   void ProcessEvent (Rml::Event& Event) override;

   // Live 3D preview plumbing (INSPECTOR_RML drives the actual PREVIEW3D window
   // from these). IsGlb reports whether the current selection is a glTF/GLB;
   // GlbData holds its bytes; GlbGeneration bumps on every new glb load so the
   // driver knows when to re-inject; PreviewBody is the pane the child render
   // window docks over.
   bool                        IsGlb         () const { return m_bIsGlb; }
   const std::vector<uint8_t>& GlbData       () const { return m_aGlbData; }
   uint64_t                    GlbGeneration () const { return m_nGlbGen; }
   Rml::Element*               PreviewBody   () const;
   Rml::Element*               PreviewHost   () const;

private:
   std::vector<Rml::Element*> m_apSections;

   void ShowMsf      (Rml::Element* pBody, SNEEZE::FILE* pFile);
   void ShowMsfJSON  (Rml::Element* pBody, SNEEZE::FILE* pFile);
   void ShowJSON     (Rml::Element* pBody, SNEEZE::FILE* pFile);

   bool                 m_bIsGlb  = false;
   uint64_t             m_nGlbGen = 0;
   std::vector<uint8_t> m_aGlbData;
};

class IW_NETWORK_DETAIL_RESPONSE : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_NETWORK_DETAIL_RESPONSE ();
   ~IW_NETWORK_DETAIL_RESPONSE () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK_DETAIL_RESPONSE (); }
   const char* Id () override { return "network-detail-response"; }
   bool Initialize (Rml::Element* pContainer) override;

   void ShowFile (SNEEZE::FILE* pFile);
   void ProcessEvent (Rml::Event& Event) override;

private:
   static const int kROWS_PER_PAGE = 16;
   static const int kBYTES_PER_ROW = 16;

   void RenderPage ();
   void SelectByte (int nByteIndex);

   Rml::Element*         m_pColAddr;
   Rml::Element*         m_pColBytes;
   Rml::Element*         m_pColChars;
   Rml::Element*         m_pTbAddr;
   Rml::Element*         m_pBtnPrev;
   Rml::Element*         m_pBtnNext;

   std::vector<uint8_t>  m_aData;
   int                   m_nPageOffset;
   int                   m_nSelected;
};

class IW_NETWORK_DETAIL_TIMING : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_NETWORK_DETAIL_TIMING ();
   ~IW_NETWORK_DETAIL_TIMING () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK_DETAIL_TIMING (); }
   const char* Id () override { return "network-detail-timing"; }
   bool Initialize (Rml::Element* pContainer) override;

   void ShowFile (SNEEZE::FILE* pFile);
   void ProcessEvent (Rml::Event& Event) override;

private:
   std::vector<Rml::Element*> m_apSections;
};

/*******************************************************************************************************************************
**                                                     Details Pane                                                           **
*******************************************************************************************************************************/

class IW_NETWORK_BODY;

class IW_NETWORK_DETAILS : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_NETWORK_DETAILS ();
   ~IW_NETWORK_DETAILS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK_DETAILS (); }
   const char* Id () override { return "network-details"; }
   bool Initialize (Rml::Element* pContainer) override;

   void SetBody (IW_NETWORK_BODY* pBody);

   void ShowFile (SNEEZE::FILE* pFile);

   void SetActiveTab (int nTab);
   int  ActiveTab () const;

   IW_NETWORK_DETAIL_HEADERS*  TabHeaders ()  const;
   IW_NETWORK_DETAIL_PREVIEW*  TabPreview ()  const;
   IW_NETWORK_DETAIL_RESPONSE* TabResponse () const;
   IW_NETWORK_DETAIL_TIMING*   TabTiming ()   const;

   void ProcessEvent (Rml::Event& Event) override;

   enum eWIDGET
   {
      kWIDGET_HEADERS  = 0,
      kWIDGET_PREVIEW  = 1,
      kWIDGET_RESPONSE = 2,
      kWIDGET_TIMING   = 3,
      kWIDGET_COUNT    = 4
   };

private:
   static const FN_CREATEWIDGET s_afnCreateWidget[kWIDGET_COUNT];

   Rml::Element*      m_pTabbar;
   Rml::Element*      m_pBtnClose;
   Rml::Element*      m_apTabs[kWIDGET_COUNT];
   Rml::Element*      m_pContent;
   IW_NETWORK_BODY*   m_pBody;
   int                m_nActiveTab;
};

/*******************************************************************************************************************************
**                                                   Container Sidebar                                                        **
*******************************************************************************************************************************/

class IW_NETWORK_FILES;

class IW_NETWORK_CONTAINERS : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_NETWORK_CONTAINERS ();
   ~IW_NETWORK_CONTAINERS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK_CONTAINERS (); }
   const char* Id () override { return "network-containers"; }
   bool Initialize (Rml::Element* pContainer) override;

   void SetFiles (IW_NETWORK_FILES* pFiles);

   void AddContainer (const std::string& sName);
   void Clear ();

   const std::string& SelectedContainer () const;

   void ProcessEvent (Rml::Event& Event) override;

private:
   void UpdateSelection (Rml::Element* pItem);

   Rml::Element*            m_pItems;
   Rml::Element*            m_pItemAll;
   IW_NETWORK_FILES*        m_pFiles;
   std::vector<std::string> m_asContainers;
   std::string              m_sSelected;
};

/*******************************************************************************************************************************
**                                                      File List                                                             **
*******************************************************************************************************************************/

class IW_NETWORK_BODY;

class IW_NETWORK_FILES : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_NETWORK_FILES ();
   ~IW_NETWORK_FILES () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK_FILES (); }
   const char* Id () override { return "network-files"; }
   bool Initialize (Rml::Element* pContainer) override;

   void SetBody (IW_NETWORK_BODY* pBody);

   void AddFile    (SNEEZE::FILE* pFile);
   void UpdateFile (SNEEZE::FILE* pFile);
   void RemoveFile (SNEEZE::FILE* pFile);
   void Clear      ();

   void Filter          (int nFilter);
   void FilterText      (const std::string& sText);
   void FilterContainer (const std::string& sContainer);

   int  FileCount () const;
   SNEEZE::FILE* SelectedFile () const;
   void Deselect ();

   void ProcessEvent (Rml::Event& Event) override;

private:
   void SelectRow    (int nIndex);
   void ApplyFilter  ();
   int  FileCategory  (SNEEZE::FILE* pFile) const;
   bool MatchesFilter (SNEEZE::FILE* pFile) const;

   Rml::Element*       m_pHeader;
   Rml::Element*       m_pRows;
   IW_NETWORK_BODY*    m_pBody;
   int                 m_nSelected;
   int                 m_nFilter;
   std::string         m_sTextFilter;
   std::string         m_sContainerFilter;
   std::vector<SNEEZE::FILE*> m_apFiles;
};

/*******************************************************************************************************************************
**                                                      Body Area                                                             **
*******************************************************************************************************************************/

class IW_NETWORK_BODY : public INSPECTOR_WIDGET
{
public:
   IW_NETWORK_BODY ();
   ~IW_NETWORK_BODY () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK_BODY (); }
   const char* Id () override { return "network-body"; }
   bool Initialize (Rml::Element* pContainer) override;

   IW_NETWORK_CONTAINERS* Containers () const;
   IW_NETWORK_FILES*      Files ()      const;
   IW_NETWORK_DETAILS*    Details ()    const;

   void ShowDetails ();
   void HideDetails ();
   void ResetDetails ();
   void OnFileDeleted (SNEEZE::FILE* pFile);
   bool IsDetailsVisible () const;

   enum eWIDGET
   {
      kWIDGET_CONTAINERS = 0,
      kWIDGET_FILES      = 1,
      kWIDGET_DETAILS    = 2,
      kWIDGET_COUNT      = 3
   };

private:
   static const FN_CREATEWIDGET s_afnCreateWidget[kWIDGET_COUNT];

   bool          m_bDetailsVisible;
   SNEEZE::FILE* m_pDetailFile;
};

/*******************************************************************************************************************************
**                                                       Toolbar                                                              **
*******************************************************************************************************************************/

class IW_BUTTON;

class IW_NETWORK_TOOLBAR : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   enum eBUTTON
   {
      kBUTTON_RECORD,
      kBUTTON_CLEAR,
      kBUTTON_RESET,
      kBUTTON_FILTER,
      kBUTTON_SETTINGS,

      kBUTTON_COUNT
   };

public:
   IW_NETWORK_TOOLBAR ();
   ~IW_NETWORK_TOOLBAR () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK_TOOLBAR (); }
   const char* Id () override { return "network-toolbar"; }
   bool Initialize (Rml::Element* pContainer) override;

   bool IsRecording ()      const;
   bool IsFilterVisible ()  const;
   bool IsSettingsVisible () const;
   bool IsCacheDisabled ()  const;

   void SetContext (SNEEZE::CONTEXT* pContext);
   void SetFiles (IW_NETWORK_FILES* pFiles);

   void ProcessEvent (Rml::Event& Event) override;

   IW_BUTTON* Button (int nIndex) const;

private:
   SNEEZE::CONTEXT*  m_pContext;
   IW_NETWORK_FILES* m_pFiles;

   Rml::Element* m_pChkDisableCache;
   Rml::Element* m_pFilterBar;
   Rml::Element* m_pSettingsTray;
   Rml::Element* m_pFilterInput;
   Rml::Element* m_apBtnTypeFilter[kFILTER_COUNT];

   bool m_bCacheDisabled;
};

/*******************************************************************************************************************************
**                                                      Waterfall                                                             **
*******************************************************************************************************************************/

class IW_NETWORK_WATERFALL : public INSPECTOR_WIDGET
{
public:
   IW_NETWORK_WATERFALL ();
   ~IW_NETWORK_WATERFALL () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK_WATERFALL (); }
   const char* Id () override { return "network-waterfall"; }
   bool Initialize (Rml::Element* pContainer) override;

private:
   Rml::Element* m_pHeader;
   Rml::Element* m_pBars;
};

/*******************************************************************************************************************************
**                                                      Status Bar                                                            **
*******************************************************************************************************************************/

class IW_NETWORK_STATUSBAR : public INSPECTOR_WIDGET
{
public:
   IW_NETWORK_STATUSBAR ();
   ~IW_NETWORK_STATUSBAR () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK_STATUSBAR (); }
   const char* Id () override { return "network-statusbar"; }
   bool Initialize (Rml::Element* pContainer) override;

   void Update (int nTotal, int nFiltered, uint64_t nTransferred, bool bFilterActive);

private:
   Rml::Element* m_pLabel;
};

/*******************************************************************************************************************************
**                                                    Network Frame                                                           **
*******************************************************************************************************************************/

class IW_NETWORK : public INSPECTOR_WIDGET
{
public:
   IW_NETWORK ();
   ~IW_NETWORK () override;

   static INSPECTOR_WIDGET* Create () { return new IW_NETWORK (); }
   const char* Id () override { return "network"; }
   bool Initialize (Rml::Element* pContainer) override;

   enum eWIDGET
   {
      kWIDGET_TOOLBAR   = 0,
      kWIDGET_WATERFALL = 1,
      kWIDGET_BODY      = 2,
      kWIDGET_STATUSBAR = 3,
      kWIDGET_COUNT     = 4
   };

private:
   static const char*            s_sRml;
   static const FN_CREATEWIDGET s_afnCreateWidget[kWIDGET_COUNT];
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_INSPECTOR_NETWORK_H
