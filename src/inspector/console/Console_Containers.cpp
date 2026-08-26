// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/console/Console.h"

using namespace RUBIDIUM;

static const char* s_sRml =
R"rml(
<div class="container-header">Origins</div>
<div id="console-container-items" class="list-body"></div>
)rml";

IW_CONSOLE_CONTAINERS::IW_CONSOLE_CONTAINERS () :
   m_pItems   (nullptr),
   m_pItemAll (nullptr),
   m_pEntries (nullptr)
{
}

void IW_CONSOLE_CONTAINERS::SetEntries (IW_CONSOLE_ENTRIES* pEntries)
{
   m_pEntries = pEntries;
}

IW_CONSOLE_CONTAINERS::~IW_CONSOLE_CONTAINERS ()
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

bool IW_CONSOLE_CONTAINERS::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pItems = m_pContainer->GetElementById ("console-container-items");

   if (m_pItems)
   {
      m_pItems->SetInnerRML ("<div class=\"container-item selected\">(all)</div>");
      m_pItemAll = m_pItems->GetChild (0);

      if (m_pItemAll)
         m_pItemAll->AddEventListener (Rml::EventId::Click, this);
   }

   return (m_pItems != nullptr);
}

void IW_CONSOLE_CONTAINERS::AddContainer (const std::string& sName)
{
   if (!(sName.empty () || std::find (m_asContainers.begin (), m_asContainers.end (), sName) != m_asContainers.end ()))
   {
      m_asContainers.push_back (sName);

      Rml::ElementPtr pOwned = m_pItems->GetOwnerDocument ()->CreateElement ("div");
      Rml::Element* pItem = pOwned.get ();
      pItem->SetClass ("container-item", true);
      pItem->SetInnerRML (sName);
      pItem->AddEventListener (Rml::EventId::Click, this);
      m_pItems->AppendChild (std::move (pOwned));
   }
}

void IW_CONSOLE_CONTAINERS::Clear ()
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

const std::string& IW_CONSOLE_CONTAINERS::SelectedContainer () const
{
   return m_sSelected;
}

void IW_CONSOLE_CONTAINERS::UpdateSelection (Rml::Element* pItem)
{
   for (int n = 0; n < m_pItems->GetNumChildren (); n++)
   {
      Rml::Element* pChild = m_pItems->GetChild (n);
      pChild->SetClass ("selected", pChild == pItem);
   }
}

void IW_CONSOLE_CONTAINERS::ProcessEvent (Rml::Event& Event)
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

   // Empty selection ("(all)") clears the origin filter and shows every entry.
   if (m_pEntries)
      m_pEntries->FilterContainer (m_sSelected);
}
