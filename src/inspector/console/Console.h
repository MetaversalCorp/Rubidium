// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_CONSOLE_H
#define RUBIDIUM_INSPECTOR_CONSOLE_H

#include "inspector/InspectorRml.h"

namespace RUBIDIUM
{

/*******************************************************************************************************************************
**                                                   Container Sidebar                                                        **
*******************************************************************************************************************************/

class IW_CONSOLE_ENTRIES;

class IW_CONSOLE_CONTAINERS : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_CONSOLE_CONTAINERS ();
   ~IW_CONSOLE_CONTAINERS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_CONSOLE_CONTAINERS (); }
   const char* Id () override { return "console-containers"; }
   bool Initialize (Rml::Element* pContainer) override;

   void SetEntries (IW_CONSOLE_ENTRIES* pEntries);

   void AddContainer (const std::string& sName);
   void Clear ();

   const std::string& SelectedContainer () const;

   void ProcessEvent (Rml::Event& Event) override;

private:
   void UpdateSelection (Rml::Element* pItem);

   Rml::Element*            m_pItems;
   Rml::Element*            m_pItemAll;
   IW_CONSOLE_ENTRIES*      m_pEntries;
   std::vector<std::string> m_asContainers;
   std::string              m_sSelected;
};

/*******************************************************************************************************************************
**                                                      Entries List                                                           **
*******************************************************************************************************************************/

class IW_CONSOLE_ENTRIES : public INSPECTOR_WIDGET
{
public:
   IW_CONSOLE_ENTRIES ();
   ~IW_CONSOLE_ENTRIES () override;

   static INSPECTOR_WIDGET* Create () { return new IW_CONSOLE_ENTRIES (); }
   const char* Id () override { return "console-entries"; }
   bool Initialize (Rml::Element* pContainer) override;

   void AddEntry    (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr);
   void RemoveEntry (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr);
   void Clear       ();
   int EntryCount  ();

   void FilterContainer (const std::string& sContainer);
   void FilterText      (const std::string& sText);
   void FilterLevels    (unsigned nMask);

private:
   Rml::Element* m_pRows;
   std::vector<std::shared_ptr<const SNEEZE::ENTRY>> m_apEntries;
   std::string   m_sContainerFilter;
   std::string   m_sTextFilter;
   unsigned      m_nLevelMask;

   bool MatchesFilter (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr) const;
   void ApplyFilter ();
};

/*******************************************************************************************************************************
**                                                      Body Area                                                             **
*******************************************************************************************************************************/

class IW_CONSOLE_BODY : public INSPECTOR_WIDGET
{
public:
   IW_CONSOLE_BODY ();
   ~IW_CONSOLE_BODY () override;

   static INSPECTOR_WIDGET* Create () { return new IW_CONSOLE_BODY (); }
   const char* Id () override { return "console-body"; }
   bool Initialize (Rml::Element* pContainer) override;

   IW_CONSOLE_CONTAINERS* Containers () const;
   IW_CONSOLE_ENTRIES*    Entries ()    const;

   void onEntryDeleted (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr);

   enum eWIDGET
   {
      kWIDGET_CONTAINERS = 0,
      kWIDGET_ENTRIES    = 1,
      kWIDGET_COUNT      = 2
   };

private:
   static const FN_CREATEWIDGET s_afnCreateWidget[kWIDGET_COUNT];
};

/*******************************************************************************************************************************
**                                                       Toolbar                                                              **
*******************************************************************************************************************************/

class IW_BUTTON;

class IW_CONSOLE_TOOLBAR : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_CONSOLE_TOOLBAR ();
   ~IW_CONSOLE_TOOLBAR () override;

   static INSPECTOR_WIDGET* Create () { return new IW_CONSOLE_TOOLBAR (); }
   const char* Id () override { return "console-toolbar"; }
   bool Initialize (Rml::Element* pContainer) override;

   void SetEntries (IW_CONSOLE_ENTRIES* pEntries);

   bool IsSettingsVisible () const;

   void ProcessEvent (Rml::Event& Event) override;

   enum eBUTTON
   {
      kBUTTON_CLEAR,
      kBUTTON_SETTINGS,

      kBUTTON_COUNT
   };

   IW_BUTTON* Button (int nIndex) const;

private:
   void UpdateLevels ();

   static const unsigned kLEVEL_COUNT = 5;
   static const unsigned kLEVEL_ALL   = (1u << kLEVEL_COUNT) - 1;

   Rml::Element*       m_pFilterInput;
   Rml::Element*       m_pSettingsTray;
   Rml::Element*       m_pLevelsLabel;
   Rml::Element*       m_pLevelsMenu;
   IW_CONSOLE_ENTRIES* m_pEntries;
   unsigned            m_nLevelMask;
};

/*******************************************************************************************************************************
**                                                    Console Frame                                                           **
*******************************************************************************************************************************/

class IW_CONSOLE : public INSPECTOR_WIDGET
{
public:
   IW_CONSOLE ();
   ~IW_CONSOLE () override;

   static INSPECTOR_WIDGET* Create () { return new IW_CONSOLE (); }
   const char* Id () override { return "console"; }
   bool Initialize (Rml::Element* pContainer) override;

   enum eWIDGET
   {
      kWIDGET_TOOLBAR = 0,
      kWIDGET_BODY    = 1,
      kWIDGET_COUNT   = 2
   };

private:
   static const char*           s_sRml;
   static const FN_CREATEWIDGET s_afnCreateWidget[kWIDGET_COUNT];
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_INSPECTOR_CONSOLE_H
