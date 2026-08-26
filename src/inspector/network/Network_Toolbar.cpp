// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/network/Network.h"

using namespace RUBIDIUM;

static IW_BUTTON::IW_BUTTON_DATA aButtonData[IW_NETWORK_TOOLBAR::kBUTTON_COUNT] =
{
   IW_BUTTON::IW_BUTTON_DATA ("btn-record",   "&#xE061;", IW_BUTTON::kTOGGLE),
   IW_BUTTON::IW_BUTTON_DATA ("btn-clear",    "&#xF08C;", IW_BUTTON::kCLICK ),
   IW_BUTTON::IW_BUTTON_DATA ("btn-reset",    "&#xE5D5;", IW_BUTTON::kCLICK ),
   IW_BUTTON::IW_BUTTON_DATA ("btn-filter",   "&#xEF4F;", IW_BUTTON::kTOGGLE),
   IW_BUTTON::IW_BUTTON_DATA ("btn-settings", "&#xE8B8;", IW_BUTTON::kTOGGLE),
};

static const char* s_sRml =
R"rml(
<div class="toolbar-row">
   <div id="btn-record" class="toolbar-btn"></div>
   <div id="btn-clear" class="toolbar-btn"></div>
   <div id="btn-reset" class="toolbar-btn"></div>
   <div class="toolbar-sep"></div>
   <div id="btn-filter" class="toolbar-btn"></div>
   <div class="toolbar-sep"></div>
   <div id="btn-disable-cache" class="toolbar-chk">Disable cache</div>
   <div class="toolbar-sep"></div>
   <div id="lbl-throttle" class="toolbar-lbl">No throttling</div>
   <div class="toolbar-sep"></div>
   <div id="btn-settings" class="toolbar-btn"></div>
</div>
<div class="toolbar-row toolbar-collapsible" id="toolbar-filter-bar">
   <input type="text" class="filter-input" id="filter-input" placeholder="Filter" />
   <div class="filter-type-btn active" id="filter-type-0">All</div>
   <div class="filter-type-btn" id="filter-type-1">MSF</div>
   <div class="filter-type-btn" id="filter-type-2">WASM</div>
   <div class="filter-type-btn" id="filter-type-3">glTF</div>
   <div class="filter-type-btn" id="filter-type-4">Socket</div>
   <div class="filter-type-btn" id="filter-type-5">Img</div>
   <div class="filter-type-btn" id="filter-type-6">Other</div>
</div>
<div class="toolbar-row toolbar-collapsible toolbar-settings-2" id="toolbar-settings-tray">
   Settings placeholder
</div>
)rml";

IW_NETWORK_TOOLBAR::IW_NETWORK_TOOLBAR () :
   m_pContext         (nullptr),
   m_pFiles           (nullptr),
   m_pChkDisableCache (nullptr),
   m_pFilterBar       (nullptr),
   m_pSettingsTray    (nullptr),
   m_pFilterInput     (nullptr),
   m_apBtnTypeFilter  {},
   m_bCacheDisabled   (false)
{
}

void IW_NETWORK_TOOLBAR::SetContext (SNEEZE::CONTEXT* pContext)
{
   m_pContext = pContext;
}

void IW_NETWORK_TOOLBAR::SetFiles (IW_NETWORK_FILES* pFiles)
{
   m_pFiles = pFiles;
}

IW_NETWORK_TOOLBAR::~IW_NETWORK_TOOLBAR ()
{
   if (m_pChkDisableCache)
      m_pChkDisableCache->RemoveEventListener (Rml::EventId::Click, this);

   if (m_pFilterInput)
      m_pFilterInput->RemoveEventListener (Rml::EventId::Change, this);

   for (int n = 0; n < kBUTTON_COUNT  &&  n < (int) m_apChildren.size (); n++)
   {
      if (m_apChildren[n])
         Container (n)->RemoveEventListener (Rml::EventId::Click, this);
   }

   for (int n = 0; n < kFILTER_COUNT; n++)
   {
      if (m_apBtnTypeFilter[n])
         m_apBtnTypeFilter[n]->RemoveEventListener (Rml::EventId::Click, this);
   }
}

bool IW_NETWORK_TOOLBAR::Initialize (Rml::Element* pContainer)
{
   bool bResult = false;

   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pChkDisableCache = Rml::ElementUtilities::GetElementById (m_pContainer, "btn-disable-cache");
   m_pFilterBar       = Rml::ElementUtilities::GetElementById (m_pContainer, "toolbar-filter-bar");
   m_pSettingsTray    = Rml::ElementUtilities::GetElementById (m_pContainer, "toolbar-settings-tray");
   m_pFilterInput     = Rml::ElementUtilities::GetElementById (m_pContainer, "filter-input");

   for (int n = 0; n < kFILTER_COUNT; n++)
   {
      std::string s;
      s = "filter-type-" + std::to_string (n);
      m_apBtnTypeFilter[n] = Rml::ElementUtilities::GetElementById (m_pContainer, s);

      if (m_apBtnTypeFilter[n])
         m_apBtnTypeFilter[n]->AddEventListener (Rml::EventId::Click, this);
   }

   if (m_pChkDisableCache)
      m_pChkDisableCache->AddEventListener (Rml::EventId::Click, this);

   if (m_pFilterInput)
      m_pFilterInput->AddEventListener (Rml::EventId::Change, this);

   if (CreateWidgetsEx (aButtonData, IW_NETWORK_TOOLBAR::kBUTTON_COUNT))
   {
      bResult = true;

      Button (kBUTTON_RECORD)->SetActive (true);

      for (int n = 0; n < kBUTTON_COUNT; n++)
         Container (n)->AddEventListener (Rml::EventId::Click, this);
   }

   return bResult;
}

void IW_NETWORK_TOOLBAR::ProcessEvent (Rml::Event& Event)
{
   int n;
   Rml::Element* pTarget = Event.GetCurrentElement ();

   // Text typed into the filter-input box. Fired on every keystroke.
   if (Event.GetId () == Rml::EventId::Change  &&  pTarget == m_pFilterInput)
   {
      if (m_pFiles)
         m_pFiles->FilterText (Event.GetParameter<Rml::String> ("value", Rml::String ()));
   }
   else
   {
      if (pTarget == Container (kBUTTON_CLEAR))
      {
         if (m_pContext)
            m_pContext->Clear ();
      }
      else if (pTarget == Container (kBUTTON_RESET))
      {
         if (m_pContext)
            m_pContext->Reset ();
      }
      else if (pTarget == Container (kBUTTON_RECORD))
      {
      }
      else if (pTarget == m_pChkDisableCache)
      {
         m_bCacheDisabled = !m_bCacheDisabled;
         m_pChkDisableCache->SetClass ("checked", m_bCacheDisabled);
      }
      else
      {
         for (n = 0; n < kFILTER_COUNT && m_apBtnTypeFilter[n] != pTarget; n++);

         if (n < kFILTER_COUNT)
         {
            for (int i = 0; i < kFILTER_COUNT; i++)
               if (m_apBtnTypeFilter[i])
                  m_apBtnTypeFilter[i]->SetClass ("active", i == n);

            // The button index matches the eNET_FILTER category order.
            if (m_pFiles)
               m_pFiles->Filter (n);
         }
      }

      if (Button (kBUTTON_FILTER)->IsActive ())
         m_pFilterBar->SetClass ("visible", true);
      else
         m_pFilterBar->SetClass ("visible", false);

      if (Button (kBUTTON_SETTINGS)->IsActive ())
         m_pSettingsTray->SetClass ("visible", true);
      else
         m_pSettingsTray->SetClass ("visible", false);
   }
}

IW_BUTTON* IW_NETWORK_TOOLBAR::Button (int nIndex) const
{
   return static_cast<IW_BUTTON*> (Widget (nIndex));
}

bool IW_NETWORK_TOOLBAR::IsRecording () const 
{ 
   return Button (kBUTTON_RECORD)->IsActive (); 
}

bool IW_NETWORK_TOOLBAR::IsFilterVisible ()  const 
{ 
   return Button (kBUTTON_FILTER)->IsActive (); 
}

bool IW_NETWORK_TOOLBAR::IsSettingsVisible () const 
{ 
   return Button (kBUTTON_SETTINGS)->IsActive (); 
}

bool IW_NETWORK_TOOLBAR::IsCacheDisabled ()  const 
{ 
   return m_bCacheDisabled; 
}
