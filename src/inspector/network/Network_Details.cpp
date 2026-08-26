// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/network/Network.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="detail-tabbar">
   <div class="detail-close" id="detail-close">&#x00D7;</div>
   <div class="tab active" id="detail-tab-0">Headers</div>
   <div class="tab" id="detail-tab-1">Preview</div>
   <div class="tab" id="detail-tab-2">Response</div>
   <div class="tab" id="detail-tab-3">Timing</div>
</div>
<div class="detail-content">
   <div id="network-detail-headers"></div>
   <div id="network-detail-preview" style="display: none;"></div>
   <div id="network-detail-response" style="display: none;"></div>
   <div id="network-detail-timing" style="display: none;"></div>
</div>
)rml";

const FN_CREATEWIDGET IW_NETWORK_DETAILS::s_afnCreateWidget[kWIDGET_COUNT] =
{
   IW_NETWORK_DETAIL_HEADERS::Create,
   IW_NETWORK_DETAIL_PREVIEW::Create,
   IW_NETWORK_DETAIL_RESPONSE::Create,
   IW_NETWORK_DETAIL_TIMING::Create,
};

IW_NETWORK_DETAILS::IW_NETWORK_DETAILS () :
   m_pTabbar    (nullptr),
   m_pBtnClose  (nullptr),
   m_pContent   (nullptr),
   m_pBody      (nullptr),
   m_nActiveTab (0),
   m_apTabs     {}
{
}

void IW_NETWORK_DETAILS::SetBody (IW_NETWORK_BODY* pBody)
{
   m_pBody = pBody;
}

IW_NETWORK_DETAILS::~IW_NETWORK_DETAILS ()
{
   if (m_pBtnClose)
      m_pBtnClose->RemoveEventListener (Rml::EventId::Click, this);

   for (int n = 0; n < kWIDGET_COUNT; n++)
   {
      if (m_apTabs[n])
         m_apTabs[n]->RemoveEventListener (Rml::EventId::Click, this);
   }
}

bool IW_NETWORK_DETAILS::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pBtnClose = m_pContainer->GetElementById ("detail-close");

   if (m_pBtnClose)
      m_pBtnClose->AddEventListener (Rml::EventId::Click, this);

   for (int n = 0; n < kWIDGET_COUNT; n++)
   {
      Rml::String sId;

      sId = "detail-tab-" + std::to_string (n);
      m_apTabs[n] = m_pContainer->GetElementById (sId);

      if (m_apTabs[n])
         m_apTabs[n]->AddEventListener (Rml::EventId::Click, this);
   }

   return CreateWidgets (s_afnCreateWidget, kWIDGET_COUNT);
}

void IW_NETWORK_DETAILS::SetActiveTab (int nTab)
{
   if (nTab >= 0  &&  nTab < kWIDGET_COUNT  &&  nTab != m_nActiveTab)
   {
      m_nActiveTab = nTab;

      for (int n = 0; n < kWIDGET_COUNT; n++)
      {
         if (m_apTabs[n])
            m_apTabs[n]->SetClass ("active", n == nTab);

         Container (n)->SetProperty ("display", n == nTab ? "block" : "none");
      }
   }
}

int IW_NETWORK_DETAILS::ActiveTab () const { return m_nActiveTab; }

void IW_NETWORK_DETAILS::ProcessEvent (Rml::Event& Event)
{
   Rml::Element* pTarget = Event.GetCurrentElement ();

   if (pTarget == m_pBtnClose)
   {
      if (m_pBody)
         m_pBody->HideDetails ();
   }
   else
   {
      for (int n = 0; n < kWIDGET_COUNT; n++)
      {
         if (pTarget == m_apTabs[n])
         {
            SetActiveTab (n);
            break;
         }
      }
   }
}

void IW_NETWORK_DETAILS::ShowFile (SNEEZE::FILE* pFile)
{
   if (TabHeaders ())
      TabHeaders ()->ShowFile (pFile);
   if (TabPreview ())
      TabPreview ()->ShowFile (pFile);
   if (TabResponse ())
      TabResponse ()->ShowFile (pFile);
   if (TabTiming ())
      TabTiming ()->ShowFile (pFile);
}

IW_NETWORK_DETAIL_HEADERS*  IW_NETWORK_DETAILS::TabHeaders ()  const { return static_cast<IW_NETWORK_DETAIL_HEADERS*>  (Widget (kWIDGET_HEADERS)); }
IW_NETWORK_DETAIL_PREVIEW*  IW_NETWORK_DETAILS::TabPreview ()  const { return static_cast<IW_NETWORK_DETAIL_PREVIEW*>  (Widget (kWIDGET_PREVIEW)); }
IW_NETWORK_DETAIL_RESPONSE* IW_NETWORK_DETAILS::TabResponse () const { return static_cast<IW_NETWORK_DETAIL_RESPONSE*> (Widget (kWIDGET_RESPONSE)); }
IW_NETWORK_DETAIL_TIMING*   IW_NETWORK_DETAILS::TabTiming ()   const { return static_cast<IW_NETWORK_DETAIL_TIMING*>   (Widget (kWIDGET_TIMING)); }

} // namespace RUBIDIUM
