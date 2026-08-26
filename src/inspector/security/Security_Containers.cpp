// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/security/Security.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="container-header">Origins</div>
<div id="security-container-items" class="list-body"></div>
)rml";

IW_SECURITY_CONTAINERS::IW_SECURITY_CONTAINERS () :
   m_pItems   (nullptr),
   m_pItemAll (nullptr)
{
}

IW_SECURITY_CONTAINERS::~IW_SECURITY_CONTAINERS ()
{
   // Every row -- the static "(all)" item and each AddContainer row -- registered
   // this widget as a click listener. RmlUi calls EventListener::OnDetach on every
   // still-attached listener when the element is destroyed at document teardown;
   // if this widget is already gone that's a dangling vtable call -> shutdown crash.
   // Detach from all rows here while this is still alive.
   if (m_pItems)
   {
      for (int n = 0; n < m_pItems->GetNumChildren (); n++)
         m_pItems->GetChild (n)->RemoveEventListener (Rml::EventId::Click, this);
   }

   m_pItemAll = nullptr;
}

bool IW_SECURITY_CONTAINERS::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pItems = m_pContainer->GetElementById ("security-container-items");

   if (m_pItems)
   {
      m_pItems->SetInnerRML ("<div class=\"container-item selected\">(all)</div>");
      m_pItemAll = m_pItems->GetChild (0);

      if (m_pItemAll)
         m_pItemAll->AddEventListener (Rml::EventId::Click, this);
   }

   return (m_pItems != nullptr);
}

void IW_SECURITY_CONTAINERS::AddContainer (const std::string& sName)
{
   if (sName.empty ())
      return;

   bool bExists = (std::find (m_asContainers.begin (), m_asContainers.end (), sName) != m_asContainers.end ());
   if (bExists)
      return;

   m_asContainers.push_back (sName);

   Rml::ElementPtr pOwned = m_pItems->GetOwnerDocument ()->CreateElement ("div");
   Rml::Element* pItem = pOwned.get ();
   pItem->SetClass ("container-item", true);
   pItem->SetInnerRML (sName);
   pItem->AddEventListener (Rml::EventId::Click, this);
   m_pItems->AppendChild (std::move (pOwned));
}

void IW_SECURITY_CONTAINERS::Clear ()
{
   if (m_pItemAll)
      m_pItemAll->RemoveEventListener (Rml::EventId::Click, this);

   while (m_pItems->GetNumChildren () > 0)
      m_pItems->RemoveChild (m_pItems->GetFirstChild ());

   m_asContainers.clear ();
   m_sSelected.clear ();

   m_pItems->SetInnerRML ("<div class=\"container-item selected\">(all)</div>");
   m_pItemAll = m_pItems->GetChild (0);

   if (m_pItemAll)
      m_pItemAll->AddEventListener (Rml::EventId::Click, this);
}

const std::string& IW_SECURITY_CONTAINERS::SelectedContainer () const
{
   return m_sSelected;
}

void IW_SECURITY_CONTAINERS::UpdateSelection (Rml::Element* pItem)
{
   for (int n = 0; n < m_pItems->GetNumChildren (); n++)
   {
      Rml::Element* pChild = m_pItems->GetChild (n);
      pChild->SetClass ("selected", pChild == pItem);
   }
}

void IW_SECURITY_CONTAINERS::ProcessEvent (Rml::Event& Event)
{
   Rml::Element* pTarget = Event.GetCurrentElement ();

   if (pTarget == m_pItemAll)
   {
      m_sSelected.clear ();
      UpdateSelection (pTarget);
   }
   else
   {
      Rml::String sText = pTarget->GetInnerRML ();
      m_sSelected = sText;
      UpdateSelection (pTarget);
   }
}

} // namespace RUBIDIUM
