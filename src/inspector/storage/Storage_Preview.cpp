// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/storage/Storage.h"
#include "inspector/common/Button.h"

using namespace RUBIDIUM;

static IW_BUTTON::IW_BUTTON_DATA aButtonData[IW_STORAGE_PREVIEW::kBUTTON_COUNT] =
{
   IW_BUTTON::IW_BUTTON_DATA ("btn-clear",    "&#xF08C;", IW_BUTTON::kCLICK),
   IW_BUTTON::IW_BUTTON_DATA ("btn-delete",   "&#xE872;", IW_BUTTON::kCLICK),
};

static const char* s_sRml =
R"rml(
<div class="panel-tabbar" id="storage-tabbar">
   <div class="tab active" id="storage-tab-0">Overview</div>
   <div class="tab" id="storage-tab-1">Permanent/Organization</div>
   <div class="tab" id="storage-tab-2">Temporary/Organization</div>
   <div class="tab" id="storage-tab-3">Permanent/Container</div>
   <div class="tab" id="storage-tab-4">Temporary/Container</div>
</div>
<div class="panel-toolbar">
   <div class="toolbar-row">
      <input type="text" class="filter-input" id="storage-search" placeholder="Filter" />
      <div class="toolbar-sep"></div>
      <div id="btn-clear" class="toolbar-btn"></div>
      <div id="btn-delete" class="toolbar-btn"></div>
   </div>
</div>
<div class="panel-content" id="storage-content"></div>
)rml";

static const char* s_asTabIds[IW_STORAGE_PREVIEW::kTAB_COUNT] =
{
   "storage-tab-0",
   "storage-tab-1",
   "storage-tab-2",
   "storage-tab-3",
   "storage-tab-4",
};

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

namespace
{
   std::string EscapeHtml (const std::string& sIn)
   {
      std::string sOut;
      sOut.reserve (sIn.size ());

      for (char c : sIn)
      {
         switch (c)
         {
            case '&': sOut += "&amp;";  break;
            case '<': sOut += "&lt;";   break;
            case '>': sOut += "&gt;";   break;
            case '"': sOut += "&quot;"; break;
            default:  sOut += c;        break;
         }
      }

      return sOut;
   }

   std::string ToLower (const std::string& sIn)
   {
      std::string sOut (sIn);
      for (char& c : sOut)
      {
         if (c >= 'A' && c <= 'Z')
            c = (char) (c + ('a' - 'A'));
      }
      return sOut;
   }

   const char* TrustName (SNEEZE::eTRUST eTrust)
   {
      const char* sResult = "None";

      switch (eTrust)
      {
         case SNEEZE::kTRUST_NONE:       sResult = "None";       break;
         case SNEEZE::kTRUST_UNTRUSTED:  sResult = "Untrusted";  break;
         case SNEEZE::kTRUST_UNVERIFIED: sResult = "Unverified"; break;
         case SNEEZE::kTRUST_EXPIRED:    sResult = "Expired";    break;
         case SNEEZE::kTRUST_VERIFIED:   sResult = "Verified";   break;
         case SNEEZE::kTRUST_ROOT:       sResult = "Root";       break;
         default:                                                break;
      }

      return sResult;
   }

   // Recursively render a JSON value as a collapsible tree node, appending HTML
   // to sOut. Objects/arrays get a { }/[ ] icon and a toggle; leaves get a small
   // square icon and a "key : value" row. Every row carries its dot/bracket path
   // (matching SILO::Remove) so a selected node can be deleted.
   //
   // Filtering: sFilterLower is the lowercased needle ("" == no filter). A node
   // is shown when the filter is empty, an ancestor's key already matched
   // (bForceShow), this node's key matches, a leaf's value matches, or any
   // descendant matched. Returns whether this subtree was shown.
   bool BuildJsonNode (const nlohmann::json& jNode, const std::string& sKey, const std::string& sPath, const std::string& sFilterLower, bool bForceShow, std::string& sOut)
   {
      bool bKeyMatch = (!sFilterLower.empty () && ToLower (sKey).find (sFilterLower) != std::string::npos);
      bool bResult   = false;

      if (jNode.is_object () || jNode.is_array ())
      {
         bool        bForceChildren = bForceShow || bKeyMatch;
         bool        bAnyChild      = false;
         std::string sChildren;

         if (jNode.is_object ())
         {
            for (auto it = jNode.begin (); it != jNode.end (); ++it)
            {
               std::string sChildPath = sPath.empty () ? it.key () : sPath + "." + it.key ();
               if (BuildJsonNode (it.value (), it.key (), sChildPath, sFilterLower, bForceChildren, sChildren))
                  bAnyChild = true;
            }
         }
         else
         {
            for (size_t n = 0; n < jNode.size (); n++)
            {
               std::string sChildPath = sPath + "[" + std::to_string (n) + "]";
               if (BuildJsonNode (jNode[n], std::to_string (n), sChildPath, sFilterLower, bForceChildren, sChildren))
                  bAnyChild = true;
            }
         }

         if (sFilterLower.empty () || bForceShow || bKeyMatch || bAnyChild)
         {
            bResult = true;

            bool        bEmpty     = jNode.empty ();
            const char* sIconClass = jNode.is_object () ? "jtn-obj" : "jtn-arr";
            const char* sIconText  = jNode.is_object () ? "{ }" : "[ ]";

            sOut += "<div class=\"jtn\">";
            sOut += "<div class=\"jtn-row\" path=\"" + EscapeHtml (sPath) + "\">";

            if (bEmpty)
               sOut += "<span class=\"jtn-toggle jtn-spacer\"></span>";
            else
               sOut += "<span class=\"jtn-toggle\"><span class=\"tg-o\">&#x2212;</span><span class=\"tg-c\">+</span></span>";

            sOut += "<span class=\"jtn-icon ";
            sOut += sIconClass;
            sOut += "\">";
            sOut += sIconText;
            sOut += "</span>";
            sOut += "<span class=\"jtn-key\">" + EscapeHtml (sKey) + "</span>";
            sOut += "</div>";

            if (!bEmpty)
               sOut += "<div class=\"jtn-children\">" + sChildren + "</div>";

            sOut += "</div>";
         }
      }
      else
      {
         std::string sValRaw   = jNode.is_string () ? jNode.get<std::string> () : jNode.dump ();
         bool        bValMatch = (!sFilterLower.empty () && ToLower (sValRaw).find (sFilterLower) != std::string::npos);

         if (sFilterLower.empty () || bForceShow || bKeyMatch || bValMatch)
         {
            bResult = true;

            std::string sValClass = "jtn-val";
            std::string sValDisp;

            if (jNode.is_string ())       { sValClass += " jtn-str";  sValDisp = "\"" + EscapeHtml (sValRaw) + "\""; }
            else if (jNode.is_number ())  { sValClass += " jtn-num";  sValDisp = EscapeHtml (sValRaw); }
            else if (jNode.is_boolean ()) { sValClass += " jtn-bool"; sValDisp = EscapeHtml (sValRaw); }
            else                          { sValClass += " jtn-null"; sValDisp = EscapeHtml (sValRaw); }

            sOut += "<div class=\"jtn jtn-leaf\">";
            sOut += "<div class=\"jtn-row\" path=\"" + EscapeHtml (sPath) + "\">";
            sOut += "<span class=\"jtn-toggle jtn-spacer\"></span>";
            sOut += "<span class=\"jtn-icon jtn-icon-leaf\"></span>";
            sOut += "<span class=\"jtn-key\">" + EscapeHtml (sKey) + "</span>";
            sOut += "<span class=\"jtn-colon\"> : </span>";
            sOut += "<span class=\"" + sValClass + "\">" + sValDisp + "</span>";
            sOut += "</div></div>";
         }
      }

      return bResult;
   }
}

// ---------------------------------------------------------------------------
// IW_STORAGE_PREVIEW
// ---------------------------------------------------------------------------

IW_STORAGE_PREVIEW::IW_STORAGE_PREVIEW () :
   m_pTabbar      (nullptr),
   m_apTabs       {},
   m_pFilterInput (nullptr),
   m_pContent     (nullptr),
   m_pBtnClear    (nullptr),
   m_pBtnDelete   (nullptr),
   m_nActiveTab   (0),
   m_pSilo        (nullptr),
   m_pSelectedRow (nullptr)
{
}

IW_STORAGE_PREVIEW::~IW_STORAGE_PREVIEW ()
{
   for (int n = 0; n < kTAB_COUNT; n++)
   {
      if (m_apTabs[n])
         m_apTabs[n]->RemoveEventListener (Rml::EventId::Click, this);
   }

   if (m_pContent)
      m_pContent->RemoveEventListener (Rml::EventId::Click, this);

   if (m_pFilterInput)
      m_pFilterInput->RemoveEventListener (Rml::EventId::Change, this);

   if (m_pBtnClear)
      m_pBtnClear->RemoveEventListener (Rml::EventId::Click, this);

   if (m_pBtnDelete)
      m_pBtnDelete->RemoveEventListener (Rml::EventId::Click, this);
}

bool IW_STORAGE_PREVIEW::Initialize (Rml::Element* pContainer)
{
   bool bResult = false;

   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pTabbar      = m_pContainer->GetElementById ("storage-tabbar");
   m_pFilterInput = m_pContainer->GetElementById ("storage-search");
   m_pContent     = m_pContainer->GetElementById ("storage-content");

   for (int n = 0; n < kTAB_COUNT; n++)
   {
      m_apTabs[n] = m_pContainer->GetElementById (s_asTabIds[n]);

      if (m_apTabs[n])
         m_apTabs[n]->AddEventListener (Rml::EventId::Click, this);
   }

   if (CreateWidgetsEx (aButtonData, kBUTTON_COUNT))
   {
      // The buttons manage their own glyph/hover, but the preview owns their
      // action -- listen on the same elements for the action click.
      m_pBtnClear  = m_pContainer->GetElementById ("btn-clear");
      m_pBtnDelete = m_pContainer->GetElementById ("btn-delete");

      if (m_pBtnClear)
         m_pBtnClear->AddEventListener (Rml::EventId::Click, this);
      if (m_pBtnDelete)
         m_pBtnDelete->AddEventListener (Rml::EventId::Click, this);

      if (m_pContent)
         m_pContent->AddEventListener (Rml::EventId::Click, this);
      if (m_pFilterInput)
         m_pFilterInput->AddEventListener (Rml::EventId::Change, this);

      RenderActive ();

      bResult = true;
   }

   return bResult;
}

bool IW_STORAGE_PREVIEW::ScopeForTab (int nTab, SNEEZE::eSILO_SCOPE& eScope)
{
   bool bResult = true;

   switch (nTab)
   {
      case kTAB_PERM_ORG: eScope = SNEEZE::kSILO_SCOPE_PERMANENT_ORG;         break;
      case kTAB_TEMP_ORG: eScope = SNEEZE::kSILO_SCOPE_TEMPORARY_ORG;         break;
      case kTAB_PERM_CTR: eScope = SNEEZE::kSILO_SCOPE_PERMANENT_CONTAINER;   break;
      case kTAB_TEMP_CTR: eScope = SNEEZE::kSILO_SCOPE_TEMPORARY_CONTAINER;   break;
      default:            bResult = false;                                    break;
   }

   return bResult;
}

void IW_STORAGE_PREVIEW::ShowSilo (SNEEZE::SILO* pSilo)
{
   if (pSilo != m_pSilo)
   {
      if (m_pSilo)
         m_pSilo->Detach ();

      m_pSilo = pSilo;
      m_sSelectedPath.clear ();
      m_pSelectedRow = nullptr;

      if (m_pSilo)
         m_pSilo->Attach ();

      RenderActive ();
   }
}

void IW_STORAGE_PREVIEW::DropSilo ()
{
   m_pSilo        = nullptr;
   m_pSelectedRow = nullptr;
   m_sSelectedPath.clear ();

   RenderActive ();
}

void IW_STORAGE_PREVIEW::Refresh ()
{
   m_pSelectedRow = nullptr;
   RenderActive ();
}

void IW_STORAGE_PREVIEW::RenderActive ()
{
   SNEEZE::eSILO_SCOPE eScope;

   if (m_nActiveTab == kTAB_OVERVIEW)
      RenderOverview ();
   else if (ScopeForTab (m_nActiveTab, eScope))
      RenderScope (eScope);
}

void IW_STORAGE_PREVIEW::RenderOverview ()
{
   if (m_pContent == nullptr)
      return;

   if (m_pSilo == nullptr)
   {
      m_pContent->SetInnerRML (
         "<div class=\"placeholder-icon\">&#xEA3C;</div>"
         "<div class=\"placeholder\">Select an origin to inspect its storage.</div>");
      return;
   }

   const SNEEZE::CONTAINER* pContainer = m_pSilo->Container ();
   const SNEEZE::CONTAINER::CID* pCID  = pContainer ? pContainer->Identity () : nullptr;

   std::string sHtml;
   sHtml += "<div class=\"kv-section\">";
   sHtml += "<div class=\"kv-section-title\">Identity</div>";

   auto AddRow = [&sHtml] (const char* sKey, const std::string& sValue)
   {
      sHtml += "<div class=\"ov-row\"><div class=\"ov-key\">";
      sHtml += sKey;
      sHtml += "</div><div class=\"ov-val\">";
      sHtml += EscapeHtml (sValue.empty () ? std::string ("(none)") : sValue);
      sHtml += "</div></div>";
   };

   AddRow ("Display Name", m_pSilo->DisplayName ());

   if (pCID)
   {
      AddRow ("Organization", pCID->sOrganization);
      AddRow ("Container",    pCID->sContainer);
      AddRow ("Fingerprint",  pCID->sFingerprint);
      AddRow ("Trust",        TrustName (pCID->eTrust));
   }

   sHtml += "</div>";

   // Per-scope summaries.
   static const struct { const char* sName; SNEEZE::eSILO_SCOPE eScope; } aScopes[] =
   {
      { "Permanent / Organization", SNEEZE::kSILO_SCOPE_PERMANENT_ORG     },
      { "Temporary / Organization", SNEEZE::kSILO_SCOPE_TEMPORARY_ORG     },
      { "Permanent / Container",    SNEEZE::kSILO_SCOPE_PERMANENT_CONTAINER },
      { "Temporary / Container",    SNEEZE::kSILO_SCOPE_TEMPORARY_CONTAINER },
   };

   sHtml += "<div class=\"kv-section\">";
   sHtml += "<div class=\"kv-section-title\">Scopes</div>";

   for (const auto& Scope : aScopes)
   {
      std::string sJson = m_pSilo->Json (Scope.eScope);
      size_t      nKeys = 0;

      try
      {
         nlohmann::json jDoc = nlohmann::json::parse (sJson, nullptr, false);
         if (jDoc.is_object () || jDoc.is_array ())
            nKeys = jDoc.size ();
      }
      catch (...) {}

      char szMeta[128];
      std::snprintf (szMeta, sizeof (szMeta), "%zu key%s &middot; %zu bytes",
                     nKeys, (nKeys == 1 ? "" : "s"), sJson.size ());

      sHtml += "<div class=\"ov-scope\"><div class=\"ov-scope-name\">";
      sHtml += Scope.sName;
      sHtml += "</div><div class=\"ov-scope-meta\">";
      sHtml += szMeta;
      sHtml += "</div><div class=\"ov-scope-path\">";
      sHtml += EscapeHtml (m_pSilo->Path (Scope.eScope));
      sHtml += "</div></div>";
   }

   sHtml += "</div>";

   m_pContent->SetInnerRML (sHtml);
}

void IW_STORAGE_PREVIEW::RenderScope (SNEEZE::eSILO_SCOPE eScope)
{
   if (m_pContent == nullptr)
      return;

   m_pSelectedRow = nullptr;

   if (m_pSilo == nullptr)
   {
      m_pContent->SetInnerRML (
         "<div class=\"placeholder-icon\">&#xEA3C;</div>"
         "<div class=\"placeholder\">Select an origin to inspect its storage.</div>");
      return;
   }


   nlohmann::json jDoc  = nlohmann::json::parse (m_pSilo->Json (eScope), nullptr, false);
   std::string    sTree;
   bool           bAny = false;

//   nlohmann::json jRoot = m_pSilo->Get (eScope, sTree);
//   sTree = jRoot.dump ();

   if (!jDoc.is_discarded ())
      bAny = BuildJsonNode (jDoc, "JSON", "", ToLower (m_sFilter), false, sTree);

   std::string sHtml;

   if (jDoc.is_discarded ())
      sHtml = "<div class=\"placeholder-icon\">&#xEA3C;</div><div class=\"placeholder\">Unable to read this scope.</div>";
   else if (!bAny)
      sHtml = "<div class=\"placeholder\">No keys match the filter.</div>";
   else
      sHtml = "<div class=\"jtree\">" + sTree + "</div>";

   m_pContent->SetInnerRML (sHtml);
}

void IW_STORAGE_PREVIEW::SelectRow (Rml::Element* pRow)
{
   if (m_pSelectedRow)
      m_pSelectedRow->SetClass ("selected", false);

   m_pSelectedRow = pRow;

   if (m_pSelectedRow)
   {
      m_pSelectedRow->SetClass ("selected", true);
      m_sSelectedPath = m_pSelectedRow->GetAttribute<Rml::String> ("path", "");
   }
   else
   {
      m_sSelectedPath.clear ();
   }
}

void IW_STORAGE_PREVIEW::ProcessEvent (Rml::Event& Event)
{
   Rml::Element* pTarget = Event.GetCurrentElement ();

   // --- Tab switch ---
   for (int n = 0; n < kTAB_COUNT; n++)
   {
      if (pTarget == m_apTabs[n] && n != m_nActiveTab)
      {
         m_apTabs[m_nActiveTab]->SetClass ("active", false);
         m_nActiveTab = n;
         m_apTabs[m_nActiveTab]->SetClass ("active", true);
         RenderActive ();
         return;
      }
   }

   // --- Filter text changed ---
   if (pTarget == m_pFilterInput && Event.GetId () == Rml::EventId::Change)
   {
      m_sFilter = Event.GetParameter<Rml::String> ("value", "");
      RenderActive ();
      return;
   }

   // --- Clear: wipe the active scope's whole document ---
   if (pTarget == m_pBtnClear)
   {
      SNEEZE::eSILO_SCOPE eScope;
      if (m_pSilo && ScopeForTab (m_nActiveTab, eScope))
      {
         m_pSilo->Json (eScope, "{}");
         m_sSelectedPath.clear ();
         RenderActive ();
      }
      return;
   }

   // --- Delete: remove the selected key from the active scope ---
   if (pTarget == m_pBtnDelete)
   {
      SNEEZE::eSILO_SCOPE eScope;
      if (m_pSilo && ScopeForTab (m_nActiveTab, eScope))
      {
         m_pSilo->Remove (eScope, m_sSelectedPath);
         m_sSelectedPath.clear ();
         RenderActive ();
      }
      return;
   }

   // --- JSON tree clicks (bubble up to the content container) ---
   // A click on the toggle box expands/collapses that node; a click anywhere
   // else on the row selects it (so the Delete button can act on its path).
   if (m_pContent && pTarget == m_pContent)
   {
      Rml::Element* pEl     = Event.GetTargetElement ();
      Rml::Element* pRow    = nullptr;
      bool          bToggle = false;

      while (pEl && pEl != m_pContent)
      {
         if (pEl->IsClassSet ("jtn-toggle") && !pEl->IsClassSet ("jtn-spacer"))
            bToggle = true;

         if (pEl->IsClassSet ("jtn-row"))
         {
            pRow = pEl;
            break;
         }

         pEl = pEl->GetParentNode ();
      }

      if (pRow)
      {
         if (bToggle)
         {
            Rml::Element* pNode = pRow->GetParentNode ();
            if (pNode)
               pNode->SetClass ("collapsed", !pNode->IsClassSet ("collapsed"));
         }
         else
         {
            SelectRow (pRow);
         }
      }
   }
}

IW_BUTTON* IW_STORAGE_PREVIEW::Button (int nIndex) const
{
   return static_cast<IW_BUTTON*> (Widget (nIndex));
}
