// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/console/Console.h"
#include "inspector/common/Button.h"

using namespace RUBIDIUM;

static IW_BUTTON::IW_BUTTON_DATA aButtonData[IW_CONSOLE_TOOLBAR::kBUTTON_COUNT] =
{
   IW_BUTTON::IW_BUTTON_DATA ("btn-clear",    "&#xF08C;", IW_BUTTON::kCLICK),
   IW_BUTTON::IW_BUTTON_DATA ("btn-settings", "&#xE8B8;", IW_BUTTON::kTOGGLE),
};

static const char* s_sRml =
R"rml(
<div class="toolbar-row">
   <div id="btn-clear" class="toolbar-btn"></div>
   <div class="toolbar-sep"></div>
   <input type="text" class="filter-input" id="filter-input" placeholder="Filter" />
   <div class="toolbar-sep"></div>
   <div id="levels-wrap" class="levels-wrap">
      <div id="lbl-levels" class="toolbar-lbl toolbar-lbl-click">All Levels &#x25BC;</div>
      <div id="levels-menu" class="levels-menu">
         <div class="levels-item checked"><span class="levels-check">&#xE5CA;</span>Debug</div>
         <div class="levels-item checked"><span class="levels-check">&#xE5CA;</span>Log</div>
         <div class="levels-item checked"><span class="levels-check">&#xE5CA;</span>Info</div>
         <div class="levels-item checked"><span class="levels-check">&#xE5CA;</span>Warning</div>
         <div class="levels-item checked"><span class="levels-check">&#xE5CA;</span>Error</div>
      </div>
   </div>
   <div class="toolbar-sep"></div>
   <div id="btn-settings" class="toolbar-btn"></div>
</div>
<div class="toolbar-row toolbar-collapsible toolbar-settings-3" id="toolbar-settings-tray">
   Settings placeholder
</div>
)rml";

IW_CONSOLE_TOOLBAR::IW_CONSOLE_TOOLBAR () :
   m_pFilterInput  (nullptr),
   m_pSettingsTray (nullptr),
   m_pLevelsLabel  (nullptr),
   m_pLevelsMenu   (nullptr),
   m_pEntries      (nullptr),
   m_nLevelMask    (kLEVEL_ALL)
{
}

void IW_CONSOLE_TOOLBAR::SetEntries (IW_CONSOLE_ENTRIES* pEntries)
{
   m_pEntries = pEntries;
}

IW_CONSOLE_TOOLBAR::~IW_CONSOLE_TOOLBAR ()
{
   if (m_pFilterInput)
      m_pFilterInput->RemoveEventListener (Rml::EventId::Change, this);

   if (m_pLevelsLabel)
      m_pLevelsLabel->RemoveEventListener (Rml::EventId::Click, this);

   if (m_pLevelsMenu)
      m_pLevelsMenu->RemoveEventListener (Rml::EventId::Click, this);

   if (m_apChildren.size () > kBUTTON_SETTINGS  &&  m_apChildren[kBUTTON_SETTINGS])
      Container (kBUTTON_SETTINGS)->RemoveEventListener (Rml::EventId::Click, this);
}

bool IW_CONSOLE_TOOLBAR::Initialize (Rml::Element* pContainer)
{
   bool bResult = false;

   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pFilterInput  = Rml::ElementUtilities::GetElementById (m_pContainer, "filter-input");
   m_pSettingsTray = Rml::ElementUtilities::GetElementById (m_pContainer, "toolbar-settings-tray");
   m_pLevelsLabel  = Rml::ElementUtilities::GetElementById (m_pContainer, "lbl-levels");
   m_pLevelsMenu   = Rml::ElementUtilities::GetElementById (m_pContainer, "levels-menu");

   if (m_pFilterInput)
      m_pFilterInput->AddEventListener (Rml::EventId::Change, this);

   if (m_pLevelsLabel)
      m_pLevelsLabel->AddEventListener (Rml::EventId::Click, this);

   if (m_pLevelsMenu)
      m_pLevelsMenu->AddEventListener (Rml::EventId::Click, this);

   if (CreateWidgetsEx (aButtonData, kBUTTON_COUNT))
   {
      bResult = true;

      Container (kBUTTON_SETTINGS)->AddEventListener (Rml::EventId::Click, this);
   }

   return bResult;
}

void IW_CONSOLE_TOOLBAR::ProcessEvent (Rml::Event& Event)
{
   // Text typed into the filter-input box. Fired on every keystroke.
   if (Event.GetId () == Rml::EventId::Change  &&  Event.GetCurrentElement () == m_pFilterInput)
   {
      if (m_pEntries)
         m_pEntries->FilterText (Event.GetParameter<Rml::String> ("value", Rml::String ()));
      return;
   }

   // Clicking the "Levels" label opens/closes the level-filter dropdown.
   if (Event.GetCurrentElement () == m_pLevelsLabel)
   {
      m_pLevelsMenu->SetClass ("visible", !m_pLevelsMenu->IsClassSet ("visible"));
      return;
   }

   // Clicking a row inside the dropdown toggles that level's checkbox. The row
   // index equals its eENTRY_LEVEL value, so the mask bit is 1 << index.
   if (Event.GetCurrentElement () == m_pLevelsMenu)
   {
      Rml::Element* pItem = Event.GetTargetElement ();
      while (pItem  &&  pItem->GetParentNode () != m_pLevelsMenu)
         pItem = pItem->GetParentNode ();

      if (pItem)
      {
         int nCount = m_pLevelsMenu->GetNumChildren ();
         for (int nLevel = 0; nLevel < nCount; nLevel++)
         {
            if (m_pLevelsMenu->GetChild (nLevel) == pItem)
            {
               m_nLevelMask ^= (1u << nLevel);
               pItem->SetClass ("checked", (m_nLevelMask & (1u << nLevel)) != 0);
               UpdateLevels ();
               break;
            }
         }
      }

      m_pLevelsMenu->SetClass ("visible", false);
      return;
   }

   if (Button (kBUTTON_SETTINGS)->IsActive ())
      m_pSettingsTray->SetClass ("visible", true);
   else
      m_pSettingsTray->SetClass ("visible", false);
}

void IW_CONSOLE_TOOLBAR::UpdateLevels ()
{
   if (m_pLevelsLabel)
      m_pLevelsLabel->SetInnerRML ((m_nLevelMask & kLEVEL_ALL) == kLEVEL_ALL ? "All Levels &#x25BC;" : "Custom Levels &#x25BC;");

   if (m_pEntries)
      m_pEntries->FilterLevels (m_nLevelMask);
}

IW_BUTTON* IW_CONSOLE_TOOLBAR::Button (int nIndex) const
{
   return static_cast<IW_BUTTON*> (Widget (nIndex));
}

bool IW_CONSOLE_TOOLBAR::IsSettingsVisible () const { return Button (kBUTTON_SETTINGS)->IsActive (); }
