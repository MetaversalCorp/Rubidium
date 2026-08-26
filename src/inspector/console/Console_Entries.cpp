// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/console/Console.h"

using namespace RUBIDIUM;

static const char* s_sRml =
R"rml(
<div id="console-rows" class="list-body" style="top: 0;"></div>
)rml";

IW_CONSOLE_ENTRIES::IW_CONSOLE_ENTRIES () :
   m_pRows      (nullptr),
   m_nLevelMask (0xFFFFFFFFu)
{
}

IW_CONSOLE_ENTRIES::~IW_CONSOLE_ENTRIES ()
{
}

bool IW_CONSOLE_ENTRIES::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pRows = m_pContainer->GetElementById ("console-rows");

   return (m_pRows != nullptr);
}

static std::string EntryContainerName (const SNEEZE::ENTRY* pEntry)
{
   if (!pEntry)
      return "";

   SNEEZE::CONTAINER* pContainer = pEntry->Container ();
   if (!pContainer)
      return "";

   const SNEEZE::CONTAINER::CID* pIdentity = pContainer->Identity ();
   if (!pIdentity)
      return "";

   return pIdentity->DisplayName ();
}

bool IW_CONSOLE_ENTRIES::MatchesFilter (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr) const
{
   // Level (Levels dropdown), origin (Origins sidebar), and text (filter-input)
   // filters are ANDed. An empty origin filter means "(all)".
   if (((m_nLevelMask >> pEntryPtr->Level ()) & 1u) == 0)
      return false;

   if (!m_sContainerFilter.empty ()  &&  EntryContainerName (pEntryPtr.get ()) != m_sContainerFilter)
      return false;

   if (!m_sTextFilter.empty ()  &&  UTILS::ToLowerEx (pEntryPtr->Message ()).find (m_sTextFilter) == std::string::npos)
      return false;

   return true;
}

void IW_CONSOLE_ENTRIES::ApplyFilter ()
{
   if (!m_pRows)
      return;

   for (size_t nIndex = 0; nIndex < m_apEntries.size (); nIndex++)
   {
      Rml::Element* pRow = m_pRows->GetChild ((int) nIndex);
      if (!pRow)
         continue;

      if (MatchesFilter (m_apEntries[nIndex]))
         pRow->RemoveProperty ("display");
      else
         pRow->SetProperty ("display", "none");
   }
}

void IW_CONSOLE_ENTRIES::FilterContainer (const std::string& sContainer)
{
   m_sContainerFilter = sContainer;
   ApplyFilter ();
}

void IW_CONSOLE_ENTRIES::FilterText (const std::string& sText)
{
   UTILS::ToLower (sText, m_sTextFilter);

   ApplyFilter ();
}

void IW_CONSOLE_ENTRIES::FilterLevels (unsigned nMask)
{
   m_nLevelMask = nMask;

   ApplyFilter ();
}

void IW_CONSOLE_ENTRIES::AddEntry (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr)
{
   m_apEntries.push_back (pEntryPtr);

   std::string sMessage = pEntryPtr->Message ();

   int nRowIndex = (int)m_apEntries.size () - 1;
   std::string sClass = (nRowIndex % 2 == 0) ? "entries-row" : "entries-row alt";

   switch (pEntryPtr->Level ())
   {
      case SNEEZE::kENTRY_LEVEL_ERROR: sClass += " level-error"; break;
      case SNEEZE::kENTRY_LEVEL_WARN:  sClass += " level-warn";  break;
      default:                                                   break;
   }

   std::string sRml;
   sRml += "<div class=\"" + sClass + "\">";
   sRml += "<div class=\"files-cell\" style=\"width: 100%;\">" + sMessage + "</div>";
   sRml += "</div>";

   Rml::ElementPtr pRow = m_pRows->GetOwnerDocument ()->CreateElement ("div");
   pRow->SetInnerRML (sRml);

   if (pRow->GetFirstChild ())
   {
      Rml::ElementPtr pInner = pRow->GetFirstChild ()->GetParentNode ()->RemoveChild (pRow->GetFirstChild ());
      m_pRows->AppendChild (std::move (pInner));
   }

   if (!MatchesFilter (pEntryPtr))
   {
      Rml::Element* pNewRow = m_pRows->GetChild ((int)m_apEntries.size () - 1);
      if (pNewRow)
         pNewRow->SetProperty ("display", "none");
   }
}

void IW_CONSOLE_ENTRIES::RemoveEntry (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr)
{
   for (size_t nIndex = 0; nIndex < m_apEntries.size (); nIndex++)
   {
      if (m_apEntries[nIndex] == pEntryPtr)
      {
         Rml::Element* pRow = m_pRows->GetChild ((int)nIndex);
         if (pRow)
            m_pRows->RemoveChild (pRow);
         m_apEntries.erase (m_apEntries.begin () + nIndex);
         break;
      }
   }
}

void IW_CONSOLE_ENTRIES::Clear ()
{
   m_apEntries.clear ();
   m_sContainerFilter.clear ();
   if (m_pRows)
      m_pRows->SetInnerRML ("");
}

int IW_CONSOLE_ENTRIES::EntryCount ()
{
   return (int)m_apEntries.size ();
}
