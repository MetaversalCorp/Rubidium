// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/storage/Storage.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="container-header">Origins</div>
<div id="storage-container-items" class="list-body"></div>
)rml";

IW_STORAGE_CONTAINERS::IW_STORAGE_CONTAINERS () :
   m_pPreview (nullptr),
   m_pItems   (nullptr),
   m_pItemAll (nullptr)
{
}

IW_STORAGE_CONTAINERS::~IW_STORAGE_CONTAINERS ()
{
   // Every row -- the static "(all)" item and each AddSilo row -- registered
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

bool IW_STORAGE_CONTAINERS::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pItems = m_pContainer->GetElementById ("storage-container-items");

   if (m_pItems)
   {
      m_pItems->SetInnerRML ("<div class=\"container-item selected\">(all)</div>");
      m_pItemAll = m_pItems->GetChild (0);

      if (m_pItemAll)
         m_pItemAll->AddEventListener (Rml::EventId::Click, this);
   }

   return (m_pItems != nullptr);
}

void IW_STORAGE_CONTAINERS::SetPreview (IW_STORAGE_PREVIEW* pPreview)
{
   m_pPreview = pPreview;
}

void IW_STORAGE_CONTAINERS::AddSilo (SNEEZE::SILO* pSilo)
{
   if (pSilo == nullptr)
      return;

   for (const ROW& Row : m_aRows)
   {
      if (Row.pSilo == pSilo)
         return;
   }

   std::string sName = pSilo->DisplayName ();
   if (sName.empty ())
      sName = "(unnamed)";

   Rml::ElementPtr pOwned = m_pItems->GetOwnerDocument ()->CreateElement ("div");
   Rml::Element* pItem = pOwned.get ();
   pItem->SetClass ("container-item", true);
   pItem->SetInnerRML (sName);
   pItem->AddEventListener (Rml::EventId::Click, this);
   m_pItems->AppendChild (std::move (pOwned));

   ROW Row;
   Row.pElement = pItem;
   Row.pSilo    = pSilo;
   m_aRows.push_back (Row);
}

void IW_STORAGE_CONTAINERS::RemoveSilo (SNEEZE::SILO* pSilo)
{
   for (auto it = m_aRows.begin (); it != m_aRows.end (); ++it)
   {
      if (it->pSilo == pSilo)
      {
         // If the removed origin was selected, fall back to "(all)". The engine
         // deletes the SILO right after firing OnStorageSiloDeleted, so it is
         // already gone by the time we drain -- drop the handle without detaching.
         if (m_pPreview && m_pPreview->CurrentSilo () == pSilo)
         {
            m_pPreview->DropSilo ();
            UpdateSelection (m_pItemAll);
         }

         it->pElement->RemoveEventListener (Rml::EventId::Click, this);
         m_pItems->RemoveChild (it->pElement);
         m_aRows.erase (it);
         break;
      }
   }
}

void IW_STORAGE_CONTAINERS::Clear ()
{
   if (m_pItemAll)
      m_pItemAll->RemoveEventListener (Rml::EventId::Click, this);

   for (const ROW& Row : m_aRows)
      Row.pElement->RemoveEventListener (Rml::EventId::Click, this);

   while (m_pItems->GetNumChildren () > 0)
      m_pItems->RemoveChild (m_pItems->GetFirstChild ());

   m_aRows.clear ();

   m_pItems->SetInnerRML ("<div class=\"container-item selected\">(all)</div>");
   m_pItemAll = m_pItems->GetChild (0);

   if (m_pItemAll)
      m_pItemAll->AddEventListener (Rml::EventId::Click, this);
}

void IW_STORAGE_CONTAINERS::UpdateSelection (Rml::Element* pItem)
{
   for (int n = 0; n < m_pItems->GetNumChildren (); n++)
   {
      Rml::Element* pChild = m_pItems->GetChild (n);
      pChild->SetClass ("selected", pChild == pItem);
   }
}

void IW_STORAGE_CONTAINERS::ProcessEvent (Rml::Event& Event)
{
   Rml::Element* pTarget = Event.GetCurrentElement ();

   SNEEZE::SILO* pSilo = nullptr;

   if (pTarget != m_pItemAll)
   {
      for (const ROW& Row : m_aRows)
      {
         if (Row.pElement == pTarget)
         {
            pSilo = Row.pSilo;
            break;
         }
      }
   }

   UpdateSelection (pTarget);

   if (m_pPreview)
      m_pPreview->ShowSilo (pSilo);
}

} // namespace RUBIDIUM
