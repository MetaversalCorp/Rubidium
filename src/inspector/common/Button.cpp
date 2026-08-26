// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/common/Button.h"

using namespace RUBIDIUM;

IW_BUTTON::IW_BUTTON_DATA::IW_BUTTON_DATA (const char* _sId, const char* _sIcon, eBEHAVIOR _eBehavior) :
   sId (_sId),
   sIcon (_sIcon),
   eBehavior (_eBehavior)
{
}

INSPECTOR_WIDGET* IW_BUTTON::IW_BUTTON_DATA::CreateWidget ()
{
   return new IW_BUTTON (this);
}

// -----------------------------------------------------

IW_BUTTON::IW_BUTTON (const IW_BUTTON_DATA* pBD) :
   m_sId       (pBD->sId),
   m_sIcon     (pBD->sIcon),
   m_eBehavior (pBD->eBehavior),
   m_bActive   (false)
{
}

IW_BUTTON::~IW_BUTTON ()
{
   if (m_pContainer)
      m_pContainer->RemoveEventListener (Rml::EventId::Click, this);
}

bool IW_BUTTON::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   if (!m_pContainer)
      return false;

   m_pContainer->SetInnerRML (m_sIcon);
   m_pContainer->AddEventListener (Rml::EventId::Click, this);

   return true;
}

const char* IW_BUTTON::Id ()
{ 
   return m_sId; 
}

bool IW_BUTTON::IsActive () const
{
   return m_bActive;
}

void IW_BUTTON::SetActive (bool bActive)
{
   m_bActive = bActive;

   if (m_pContainer)
      m_pContainer->SetClass ("active", m_bActive);
}

void IW_BUTTON::ProcessEvent (Rml::Event& Event)
{
   if (m_eBehavior == kTOGGLE)
   {
      m_bActive = !m_bActive;
      m_pContainer->SetClass ("active", m_bActive);
   }
}
