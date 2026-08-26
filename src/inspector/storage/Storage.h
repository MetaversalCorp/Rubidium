// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_STORAGE_H
#define RUBIDIUM_INSPECTOR_STORAGE_H

#include "inspector/InspectorRml.h"

namespace RUBIDIUM
{

class IW_STORAGE_PREVIEW;

/*******************************************************************************************************************************
**                                                   Container Sidebar                                                        **
*******************************************************************************************************************************/

class IW_STORAGE_CONTAINERS : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_STORAGE_CONTAINERS ();
   ~IW_STORAGE_CONTAINERS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_STORAGE_CONTAINERS (); }
   const char* Id () override { return "storage-containers"; }
   bool Initialize (Rml::Element* pContainer) override;

   void SetPreview (IW_STORAGE_PREVIEW* pPreview);

   void AddSilo    (SNEEZE::SILO* pSilo);
   void RemoveSilo (SNEEZE::SILO* pSilo);
   void Clear ();

   void ProcessEvent (Rml::Event& Event) override;

private:
   struct ROW
   {
      Rml::Element* pElement;
      SNEEZE::SILO* pSilo;
   };

   void UpdateSelection (Rml::Element* pItem);

   IW_STORAGE_PREVIEW* m_pPreview;
   Rml::Element*       m_pItems;
   Rml::Element*       m_pItemAll;
   std::vector<ROW>    m_aRows;
};

/*******************************************************************************************************************************
**                                                     Preview Pane                                                           **
*******************************************************************************************************************************/

class IW_BUTTON;

class IW_STORAGE_PREVIEW : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_STORAGE_PREVIEW ();
   ~IW_STORAGE_PREVIEW () override;

   static INSPECTOR_WIDGET* Create () { return new IW_STORAGE_PREVIEW (); }
   const char* Id () override { return "storage-preview"; }
   bool Initialize (Rml::Element* pContainer) override;

   void ProcessEvent (Rml::Event& Event) override;

   // Attach + display a silo (detaches the previously shown one). nullptr clears.
   // Interactive use only -- the previously shown silo must still be alive.
   void ShowSilo (SNEEZE::SILO* pSilo);
   // Drop the shown silo WITHOUT detaching -- for teardown paths where the silo
   // has already been (or is about to be) deleted by the engine.
   void DropSilo ();
   // Re-render the active tab in place (e.g. after a live unit change).
   void Refresh ();

   SNEEZE::SILO* CurrentSilo () const { return m_pSilo; }

   enum eTAB
   {
      kTAB_OVERVIEW   = 0,
      kTAB_PERM_ORG   = 1,
      kTAB_TEMP_ORG   = 2,
      kTAB_PERM_CTR   = 3,
      kTAB_TEMP_CTR   = 4,
      kTAB_COUNT      = 5
   };

   enum eBUTTON
   {
      kBUTTON_CLEAR,
      kBUTTON_DELETE,

      kBUTTON_COUNT
   };

   IW_BUTTON* Button (int nIndex) const;

private:
   void RenderActive  ();
   void RenderOverview ();
   void RenderScope   (SNEEZE::eSILO_SCOPE eScope);
   void SelectRow     (Rml::Element* pRow);

   static bool ScopeForTab (int nTab, SNEEZE::eSILO_SCOPE& eScope);

   Rml::Element* m_pTabbar;
   Rml::Element* m_apTabs[kTAB_COUNT];
   Rml::Element* m_pFilterInput;
   Rml::Element* m_pContent;
   Rml::Element* m_pBtnClear;
   Rml::Element* m_pBtnDelete;
   int           m_nActiveTab;

   SNEEZE::SILO* m_pSilo;
   std::string   m_sFilter;
   std::string   m_sSelectedPath;
   Rml::Element* m_pSelectedRow;
};

/*******************************************************************************************************************************
**                                                    Storage Frame                                                           **
*******************************************************************************************************************************/

class IW_STORAGE : public INSPECTOR_WIDGET
{
public:
   IW_STORAGE ();
   ~IW_STORAGE () override;

   static INSPECTOR_WIDGET* Create () { return new IW_STORAGE (); }
   const char* Id () override { return "storage"; }
   bool Initialize (Rml::Element* pContainer) override;

   IW_STORAGE_CONTAINERS* Containers () const;
   IW_STORAGE_PREVIEW*    Preview ()    const;

   enum eWIDGET
   {
      kWIDGET_CONTAINERS = 0,
      kWIDGET_PREVIEW    = 1,
      kWIDGET_COUNT      = 2
   };

private:
   static const char*           s_sRml;
   static const FN_CREATEWIDGET s_afnCreateWidget[kWIDGET_COUNT];
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_INSPECTOR_STORAGE_H
