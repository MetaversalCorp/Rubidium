// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/elements/Elements.h"

namespace RUBIDIUM
{

// Guards against a malformed (cyclic) attachment chain corrupting the UI thread.
static const int kMAX_TREE_DEPTH = 64;

static const char* s_sRml =
R"rml(
<div class="list-body tree-body" id="elements-tree-rows" style="position: relative; top: 0; bottom: auto; flex-grow: 1;"></div>
<div class="panel-bottombar" id="elements-pathbar">html &gt; body</div>
<div class="panel-searchbar" id="elements-searchbar">
   <input type="text" class="filter-input" id="elements-search-input" placeholder="Find by string, selector, or XPath" />
   <div class="panel-searchbar-close" id="elements-search-close">&#xE5CD;</div>
</div>
)rml";

IW_ELEMENTS_TREE::IW_ELEMENTS_TREE () :
   m_pContent      (nullptr),
   m_pRows         (nullptr),
   m_pPathBar      (nullptr),
   m_pSearchBar    (nullptr),
   m_pSearchInput  (nullptr),
   m_pSearchClose  (nullptr),
   m_bSearchVisible (false),
   m_pScene        (nullptr),
   m_pDetails      (nullptr),
   m_pSelectedNode (nullptr),
   m_pSelectedRow  (nullptr)
{
}

IW_ELEMENTS_TREE::~IW_ELEMENTS_TREE ()
{
   if (m_pSearchClose)
      m_pSearchClose->RemoveEventListener (Rml::EventId::Click, this);

   if (m_pRows)
      m_pRows->RemoveEventListener (Rml::EventId::Click, this);
}

bool IW_ELEMENTS_TREE::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pRows        = m_pContainer->GetElementById ("elements-tree-rows");
   m_pPathBar     = m_pContainer->GetElementById ("elements-pathbar");
   m_pSearchBar   = m_pContainer->GetElementById ("elements-searchbar");
   m_pSearchInput = m_pContainer->GetElementById ("elements-search-input");
   m_pSearchClose = m_pContainer->GetElementById ("elements-search-close");

   if (m_pSearchClose)
      m_pSearchClose->AddEventListener (Rml::EventId::Click, this);

   if (m_pRows)
      m_pRows->AddEventListener (Rml::EventId::Click, this);

   Rebuild ();

   return (m_pPathBar != nullptr);
}

void IW_ELEMENTS_TREE::SetDetails (IW_ELEMENTS_DETAILS* pDetails)
{
   m_pDetails = pDetails;
}

void IW_ELEMENTS_TREE::SetScene (SNEEZE::SCENE* pScene)
{
   m_pScene = pScene;
   Rebuild ();
}

bool IW_ELEMENTS_TREE::Rebuild ()
{
   if (!m_pRows)
      return false;

   std::string sRml;

   // Rebuilt every pass alongside the markup so row index N always maps to the
   // same NODE* as row id "eltree-row-N" (and toggle id "eltree-tog-N").
   m_apRowNode.clear ();
   m_apToggleKey.clear ();

   if (m_pScene)
   {
      // Start at the primary fabric rather than the root. This excludes the
      // synthetic root entities the scene creates above real content: the Root
      // Fabric, its Root Node, and the root-class/subtype-255 Primary Node.
      if (SNEEZE::FABRIC* pFabric_Primary = m_pScene->Fabric_Primary ())
         BuildFabric (sRml, pFabric_Primary, 0);
   }

   if (sRml.empty ())
      sRml = "<div class=\"placeholder\">No fabrics loaded</div>";

   // The tree is polled periodically; only touch the DOM when it actually
   // changed so we don't reset the user's scroll position every refresh.
   if (sRml == m_sLastRml)
      return false;

   m_sLastRml = sRml;

   // SetInnerRML destroys the old row elements, so the cached selection element
   // is dangling until ApplySelectionHighlight resolves it against the new DOM.
   m_pSelectedRow = nullptr;
   m_pRows->SetInnerRML (sRml);

   ApplySelectionHighlight ();

   return true;
}

void IW_ELEMENTS_TREE::BuildFabric (std::string& sRml, SNEEZE::FABRIC* pFabric, int nDepth)
{
   if (!pFabric  ||  nDepth >= kMAX_TREE_DEPTH)
      return;

   const std::string& sUrl  = pFabric->Url ();
   std::string        sText = sUrl.empty () ? std::string ("(fabric)") : sUrl;
   int                nPad  = 6 + nDepth * 14;

   // When an origin filter is active, fabrics from other origins are rendered as
   // dimmed (pink) rows and their nodes are suppressed -- only the matching
   // fabric expands its node tree.
   bool bMatch = FabricMatchesFilter (pFabric);
   bool bOther = (!m_sOriginFilter.empty ()  &&  !bMatch);

   SNEEZE::NODE* pNode_Root = pFabric->Node_Root ();

   // Only the matching fabric exposes a collapse arrow -- a dimmed (pink) fabric
   // shows no node children of its own, so there is nothing to collapse.
   bool bExpandable = (pNode_Root != nullptr)  &&  bMatch;
   bool bCollapsed  = bExpandable  &&  IsCollapsed (pFabric);

   sRml += "<div class=\"tree-row tree-fabric";
   if (bOther)
      sRml += " tree-fabric-other";
   sRml += "\" style=\"padding-left: " + std::to_string (nPad) + "dp;\">";
   sRml += BuildToggle (pFabric, bExpandable, bCollapsed);
   sRml += UTILS::Escape (sText);
   sRml += "</div>";

   // Descend unless this (matching) fabric is collapsed. Dimmed fabrics always
   // descend regardless so nested fabrics from the selected origin still surface;
   // bMatch decides whether this fabric's own nodes are emitted.
   if (pNode_Root  &&  !bCollapsed)
      BuildNode (sRml, pNode_Root, nDepth + 1, bMatch);
}

void IW_ELEMENTS_TREE::BuildNode (std::string& sRml, SNEEZE::NODE* pNode, int nDepth, bool bShowNodes)
{
   if (!pNode  ||  nDepth >= kMAX_TREE_DEPTH)
      return;

   // A node is expandable when it anchors a child fabric or has child nodes.
   bool bExpandable = (pNode->Fabric_Attachment () != nullptr)  ||  (pNode->Node_Count () > 0);
   bool bCollapsed  = bExpandable  &&  IsCollapsed (pNode);

   // Nodes are emitted only for the fabric matching the active origin filter.
   // Other fabrics keep descending (to discover nested matching fabrics) but
   // suppress their own node rows. The row index <-> NODE* map only tracks
   // emitted rows, so "eltree-row-N" stays in lockstep with m_apRowNode.
   if (bShowNodes)
   {
      // Node label: <class, type (subtype), name="Name">
      //   e.g. <celestial, starsystem (3), name="sun">
      std::string sClass = pNode->ClassName ();
      std::string sText;

      if (sClass.empty ())
      {
         // No map object attached yet (e.g. the synthetic root node) -- fall back
         // to the class packed in the object index plus the bare index.
         uint64_t twObjectIx = pNode->ObjectIx ();
         sText = std::string ("<") + RMAP::MAP::MAP_OBJECT::ClassName (static_cast<RMAP::MAP::MAP_OBJECT_CLASS> (twObjectIx >> 48)) + " " + std::to_string (twObjectIx & SNEEZE::TWORD_MAX) + ">";
      }
      else
      {
         sText  = "<" + sClass + ", " + pNode->TypeName ();
         sText += " (" + std::to_string (pNode->Subtype ()) + "), name=\"";
         sText += pNode->Name () + "\">";
      }

      int nPad   = 6 + nDepth * 14;
      int nRowIx = static_cast<int> (m_apRowNode.size ());

      m_apRowNode.push_back (pNode);

      sRml += "<div class=\"tree-row tree-node\" id=\"eltree-row-" + std::to_string (nRowIx) + "\" style=\"padding-left: " + std::to_string (nPad) + "dp;\">";
      sRml += BuildToggle (pNode, bExpandable, bCollapsed);
      sRml += UTILS::Escape (sText);
      sRml += "</div>";
   }

   // A visible, collapsed node hides its whole subtree (child nodes and any
   // attached fabric). Hidden (origin-filtered) nodes always descend so nested
   // matching fabrics still surface.
   bool bDescend = bShowNodes ? !bCollapsed : true;

   if (bDescend)
   {
      // A node may anchor a nested child fabric (its attachment point in the SOM).
      if (SNEEZE::FABRIC* pFabric_Child = pNode->Fabric_Attachment ())
         BuildFabric (sRml, pFabric_Child, nDepth + 1);

      int nCount = pNode->Node_Count ();
      for (int i = 0; i < nCount; i++)
      {
         if (SNEEZE::NODE* pChild = pNode->Child (i))
            BuildNode (sRml, pChild, nDepth + 1, bShowNodes);
      }
   }
}

std::string IW_ELEMENTS_TREE::BuildToggle (const void* pKey, bool bExpandable, bool bCollapsed)
{
   std::string sResult;

   if (bExpandable)
   {
      int nToggleIx = static_cast<int> (m_apToggleKey.size ());

      m_apToggleKey.push_back (pKey);

      sResult  = "<span class=\"tree-toggle";
      if (bCollapsed)
         sResult += " collapsed";
      sResult += "\" id=\"eltree-tog-" + std::to_string (nToggleIx) + "\">&#xE5C5;</span>";
   }
   else
   {
      // Leaf: empty, transparent spacer keeps labels aligned under expandable rows.
      sResult = "<span class=\"tree-toggle leaf\"></span>";
   }

   return sResult;
}

bool IW_ELEMENTS_TREE::IsCollapsed (const void* pKey) const
{
   return (m_setCollapsed.find (pKey) != m_setCollapsed.end ());
}

void IW_ELEMENTS_TREE::SetOriginFilter (const std::string& sOrigin)
{
   if (sOrigin != m_sOriginFilter)
   {
      m_sOriginFilter = sOrigin;
      Rebuild ();
   }
}

bool IW_ELEMENTS_TREE::FabricMatchesFilter (SNEEZE::FABRIC* pFabric) const
{
   bool bResult = true;   // "(all)" -- no filter active.

   if (!m_sOriginFilter.empty ())
   {
      bResult = false;

      if (pFabric)
      {
         if (SNEEZE::CONTAINER* pContainer = pFabric->Container ())
         {
            if (const SNEEZE::CONTAINER::CID* pIdentity = pContainer->Identity ())
               bResult = (pIdentity->DisplayName () == m_sOriginFilter);
         }
      }
   }

   return bResult;
}

void IW_ELEMENTS_TREE::ShowSearch ()
{
   if (!m_bSearchVisible && m_pSearchBar)
   {
      m_bSearchVisible = true;
      m_pSearchBar->SetClass ("visible", true);
   }
}

void IW_ELEMENTS_TREE::HideSearch ()
{
   if (m_bSearchVisible && m_pSearchBar)
   {
      m_bSearchVisible = false;
      m_pSearchBar->SetClass ("visible", false);
   }
}

bool IW_ELEMENTS_TREE::IsSearchVisible () const
{
   return m_bSearchVisible;
}

void IW_ELEMENTS_TREE::ProcessEvent (Rml::Event& Event)
{
   Rml::Element* pCurrent = Event.GetCurrentElement ();

   if (pCurrent == m_pSearchClose)
   {
      HideSearch ();
   }
   else if (pCurrent == m_pRows)
   {
      // A click on a disclosure arrow toggles collapse and never changes the
      // selection. Walk up to the nearest non-leaf "tree-toggle" first.
      Rml::Element* pToggle = Event.GetTargetElement ();

      while (pToggle  &&  pToggle != m_pRows  &&  !pToggle->IsClassSet ("tree-toggle"))
         pToggle = pToggle->GetParentNode ();

      if (pToggle  &&  pToggle->IsClassSet ("tree-toggle")  &&  !pToggle->IsClassSet ("leaf"))
      {
         const std::string  sPrefix = "eltree-tog-";
         const std::string& sId     = pToggle->GetId ();

         if (sId.compare (0, sPrefix.size (), sPrefix) == 0)
         {
            int nToggleIx = std::atoi (sId.c_str () + sPrefix.size ());

            if (nToggleIx >= 0  &&  nToggleIx < static_cast<int> (m_apToggleKey.size ()))
            {
               const void* pKey = m_apToggleKey[nToggleIx];

               if (IsCollapsed (pKey))
                  m_setCollapsed.erase (pKey);
               else
                  m_setCollapsed.insert (pKey);

               Rebuild ();
            }
         }

         return;
      }

      // Walk up from the clicked element to the owning node row. Fabric rows
      // carry no map object, so only "tree-node" rows are selectable.
      Rml::Element* pRow = Event.GetTargetElement ();

      while (pRow  &&  pRow != m_pRows  &&  !pRow->IsClassSet ("tree-node"))
         pRow = pRow->GetParentNode ();

      if (pRow  &&  pRow->IsClassSet ("tree-node"))
      {
         const std::string  sPrefix = "eltree-row-";
         const std::string& sId     = pRow->GetId ();

         if (sId.compare (0, sPrefix.size (), sPrefix) == 0)
         {
            int nRowIx = std::atoi (sId.c_str () + sPrefix.size ());

            if (nRowIx >= 0  &&  nRowIx < static_cast<int> (m_apRowNode.size ()))
            {
               if (m_pSelectedRow)
                  m_pSelectedRow->SetClass ("selected", false);

               m_pSelectedRow  = pRow;
               m_pSelectedNode = m_apRowNode[nRowIx];

               m_pSelectedRow->SetClass ("selected", true);

               if (m_pDetails)
                  m_pDetails->ShowNode (m_pSelectedNode);
            }
         }
      }
   }
}

void IW_ELEMENTS_TREE::ApplySelectionHighlight ()
{
   if (m_pSelectedNode  &&  m_pRows)
   {
      for (size_t n = 0; n < m_apRowNode.size (); n++)
      {
         if (m_apRowNode[n] == m_pSelectedNode)
         {
            Rml::Element* pRow = m_pRows->GetElementById ("eltree-row-" + std::to_string (n));

            if (pRow)
            {
               pRow->SetClass ("selected", true);
               m_pSelectedRow = pRow;
            }

            break;
         }
      }
   }
}

} // namespace RUBIDIUM
