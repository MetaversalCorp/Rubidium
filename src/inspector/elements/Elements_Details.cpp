// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/elements/Elements.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="detail-tabbar" id="elements-detail-tabbar">
   <div class="tab active" id="elements-tab-details">Details</div>
   <div class="tab" id="elements-tab-computed">Computed</div>
   <div class="tab" id="elements-tab-events">Event Listeners</div>
</div>
<div class="detail-content" id="elements-detail-details"></div>
<div class="detail-content" id="elements-detail-computed" style="display: none;"></div>
<div class="detail-content" id="elements-detail-events" style="display: none;"></div>
)rml";

const FN_CREATEWIDGET IW_ELEMENTS_DETAILS::s_afnCreateWidget[kWIDGET_COUNT] =
{
   IW_ELEMENTS_DETAIL_COMPUTED::Create,
   IW_ELEMENTS_DETAIL_DETAILS::Create,
   IW_ELEMENTS_DETAIL_EVENTS::Create,
};

static const char* s_asTabIds[IW_ELEMENTS_DETAILS::kWIDGET_COUNT] =
{
   "elements-tab-computed",
   "elements-tab-details",
   "elements-tab-events",
};

static const char* s_asPanelIds[IW_ELEMENTS_DETAILS::kWIDGET_COUNT] =
{
   "elements-detail-computed",
   "elements-detail-details",
   "elements-detail-events",
};

IW_ELEMENTS_DETAILS::IW_ELEMENTS_DETAILS () :
   m_pTabbar    (nullptr),
   m_apTabs     {},
   m_apPanels   {},
   m_nActiveTab (kWIDGET_DETAILS)        // matches the RML default-active "Details" tab
{
}

IW_ELEMENTS_DETAILS::~IW_ELEMENTS_DETAILS ()
{
   for (int n = 0; n < kWIDGET_COUNT; n++)
   {
      if (m_apTabs[n])
         m_apTabs[n]->RemoveEventListener (Rml::EventId::Click, this);
   }
}

bool IW_ELEMENTS_DETAILS::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pTabbar = m_pContainer->GetElementById ("elements-detail-tabbar");

   for (int n = 0; n < kWIDGET_COUNT; n++)
   {
      m_apTabs[n]   = m_pContainer->GetElementById (s_asTabIds[n]);
      m_apPanels[n] = m_pContainer->GetElementById (s_asPanelIds[n]);

      if (m_apTabs[n])
         m_apTabs[n]->AddEventListener (Rml::EventId::Click, this);
   }

   if (!CreateWidgets (s_afnCreateWidget, kWIDGET_COUNT))
      return false;

   return (m_pTabbar != nullptr);
}

void IW_ELEMENTS_DETAILS::ShowNode (SNEEZE::NODE* pNode)
{
   if (IW_ELEMENTS_DETAIL_DETAILS* pDetails = static_cast<IW_ELEMENTS_DETAIL_DETAILS*> (Widget (kWIDGET_DETAILS)))
      pDetails->ShowNode (pNode);

   if (IW_ELEMENTS_DETAIL_COMPUTED* pComputed = static_cast<IW_ELEMENTS_DETAIL_COMPUTED*> (Widget (kWIDGET_COMPUTED)))
      pComputed->ShowNode (pNode);
}

void IW_ELEMENTS_DETAILS::ProcessEvent (Rml::Event& Event)
{
   Rml::Element* pTarget = Event.GetCurrentElement ();

   for (int n = 0; n < kWIDGET_COUNT; n++)
   {
      if (pTarget == m_apTabs[n] && n != m_nActiveTab)
      {
         m_apTabs[m_nActiveTab]->SetClass ("active", false);
         m_apPanels[m_nActiveTab]->SetProperty ("display", "none");

         m_nActiveTab = n;

         m_apTabs[m_nActiveTab]->SetClass ("active", true);
         m_apPanels[m_nActiveTab]->SetProperty ("display", "block");
         break;
      }
   }
}

} // namespace RUBIDIUM
