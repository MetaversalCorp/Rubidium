// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/network/Network.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="files-header">
   <div class="files-header-cell" style="width: 33%;">Name</div>
   <div class="files-header-cell" style="width: 10%;">Status</div>
   <div class="files-header-cell" style="width: 18%;">Type</div>
   <div class="files-header-cell" style="width: 19%;">Size</div>
   <div class="files-header-cell" style="width: 20%;">Time</div>
</div>
<div id="files-rows" class="list-body files-body"></div>
<div class="files-col-seps">
   <div class="files-col-sep" style="left: 33%;"></div>
   <div class="files-col-sep" style="left: 43%;"></div>
   <div class="files-col-sep" style="left: 61%;"></div>
   <div class="files-col-sep" style="left: 80%;"></div>
</div>
)rml";

static std::string FormatSize (uint64_t nBytes)
{
   if (nBytes == 0)
      return "";
   if (nBytes < 1024)
      return std::to_string (nBytes) + " B";
   if (nBytes < 1024 * 1024)
      return std::to_string (nBytes / 1024) + " KB";
   return std::to_string (nBytes / (1024 * 1024)) + " MB";
}

static std::string FormatTime (SNEEZE::FILE* pFile)
{
   if (pFile->IsServedFromCache ())
      return "(cache)";

   double dDuration = pFile->FetchDuration ();
   if (dDuration <= 0.0)
      return "pending";

   double dMs = dDuration * 1000.0;
   if (dMs < 1.0)
      return "<1 ms";
   if (dMs < 1000.0)
      return std::to_string ((int)dMs) + " ms";
   char sBuf[32];
   std::snprintf (sBuf, sizeof (sBuf), "%.2f s", dMs / 1000.0);
   return sBuf;
}

static std::string ExtractFilename (const std::string& sUrl)
{
   size_t nQuery = sUrl.find ('?');
   std::string sPath = (nQuery != std::string::npos) ? sUrl.substr (0, nQuery) : sUrl;

   size_t nSlash = sPath.rfind ('/');
   if (nSlash != std::string::npos && nSlash + 1 < sPath.size ())
      return sPath.substr (nSlash + 1);

   return sPath;
}

static std::string FormatStatus (long nStatus)
{
   if (nStatus == 0)
      return "";
   return std::to_string (nStatus);
}

IW_NETWORK_FILES::IW_NETWORK_FILES () :
   m_pHeader   (nullptr),
   m_pRows     (nullptr),
   m_pBody     (nullptr),
   m_nSelected (-1),
   m_nFilter   (kFILTER_ALL)
{
}

bool IW_NETWORK_FILES::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pHeader = m_pContainer->GetChild (0);
   m_pRows   = m_pContainer->GetElementById ("files-rows");

   if (m_pRows)
      m_pRows->AddEventListener (Rml::EventId::Click, this);

   return (m_pHeader  &&  m_pRows);
}

void IW_NETWORK_FILES::SetBody (IW_NETWORK_BODY* pBody)
{
   m_pBody = pBody;
}

IW_NETWORK_FILES::~IW_NETWORK_FILES ()
{
   if (m_pRows)
      m_pRows->RemoveEventListener (Rml::EventId::Click, this);
}

void IW_NETWORK_FILES::AddFile (SNEEZE::FILE* pFile)
{
   m_apFiles.push_back (pFile);

   std::string sName   = ExtractFilename (pFile->Url ());
   std::string sStatus = FormatStatus (pFile->HttpStatus ());
   std::string sType   = pFile->ContentType ();
   std::string sSize   = FormatSize (pFile->SizeBytes ());
   std::string sTime   = FormatTime (pFile);

   int nRowIndex = (int)m_apFiles.size () - 1;

   std::string sCells;
   sCells += "<div class=\"files-cell\" style=\"width: 33%;\">" + sName + "</div>";
   sCells += "<div class=\"files-cell\" style=\"width: 10%;\">" + sStatus + "</div>";
   sCells += "<div class=\"files-cell\" style=\"width: 18%;\">" + sType + "</div>";
   sCells += "<div class=\"files-cell\" style=\"width: 19%;\">" + sSize + "</div>";
   sCells += "<div class=\"files-cell\" style=\"width: 20%;\">" + sTime + "</div>";

   // Attach the row before populating it so RmlUi resolves the flex/percentage
   // column widths against the real containing-block width. Building the cells
   // on a detached element collapses the columns until a later relayout.
   Rml::ElementPtr pOwned = m_pRows->GetOwnerDocument ()->CreateElement ("div");
   Rml::Element* pRow = pOwned.get ();
   pRow->SetClass ("files-row", true);
   if (nRowIndex % 2 != 0)
      pRow->SetClass ("alt", true);
   m_pRows->AppendChild (std::move (pOwned));

   pRow->SetInnerRML (sCells);

   if (!MatchesFilter (pFile))
      pRow->SetProperty ("display", "none");
}

void IW_NETWORK_FILES::UpdateFile (SNEEZE::FILE* pFile)
{
   for (size_t nIndex = 0; nIndex < m_apFiles.size (); nIndex++)
   {
      if (m_apFiles[nIndex] == pFile)
      {
         Rml::Element* pRow = m_pRows->GetChild ((int)nIndex);
         if (pRow)
         {
            std::string sStatus = FormatStatus (pFile->HttpStatus ());
            std::string sType   = pFile->ContentType ();
            std::string sSize   = FormatSize (pFile->SizeBytes ());
            std::string sTime   = FormatTime (pFile);

            if (pRow->GetChild (1)) pRow->GetChild (1)->SetInnerRML (sStatus);
            if (pRow->GetChild (2)) pRow->GetChild (2)->SetInnerRML (sType);
            if (pRow->GetChild (3)) pRow->GetChild (3)->SetInnerRML (sSize);
            if (pRow->GetChild (4)) pRow->GetChild (4)->SetInnerRML (sTime);

            // Content type can arrive after the row is created, so re-evaluate
            // visibility against the active filter.
            if (MatchesFilter (pFile))
               pRow->RemoveProperty ("display");
            else
               pRow->SetProperty ("display", "none");
         }
         break;
      }
   }
}

void IW_NETWORK_FILES::RemoveFile (SNEEZE::FILE* pFile)
{
   for (size_t nIndex = 0; nIndex < m_apFiles.size (); nIndex++)
   {
      if (m_apFiles[nIndex] == pFile)
      {
         Rml::Element* pRow = m_pRows->GetChild ((int)nIndex);
         if (pRow)
            m_pRows->RemoveChild (pRow);
         m_apFiles.erase (m_apFiles.begin () + nIndex);
         break;
      }
   }
}

void IW_NETWORK_FILES::Clear ()
{
   m_apFiles.clear ();
   m_nSelected = -1;
   if (m_pRows)
      m_pRows->SetInnerRML ("");
}

static bool StartsWith (const std::string& s, const char* sPrefix)
{
   return s.rfind (sPrefix, 0) == 0;
}

int IW_NETWORK_FILES::FileCategory (SNEEZE::FILE* pFile) const
{
   const std::string sType = pFile->ContentType ();

   if (sType == "application/jose+msf" || sType == "application/jose+json")
      return kFILTER_MSF;

   if (sType == "application/wasm")
      return kFILTER_WASM;

   if (sType == "model/gltf+json"  ||  sType == "model/gltf-binary"  ||  sType == "application/gltf-buffer")
      return kFILTER_GLTF;

   if (StartsWith (sType, "image/"))
      return kFILTER_IMG;

   // No explicit socket flag on FILE; infer from the URL scheme.
   const std::string sUrl = pFile->Url ();
   if (StartsWith (sUrl, "ws://")  ||  StartsWith (sUrl, "wss://"))
      return kFILTER_SOCKET;

   return kFILTER_OTHER;
}

bool IW_NETWORK_FILES::MatchesFilter (SNEEZE::FILE* pFile) const
{
   // Category (filter buttons), text (filter-input), and origin (Origins
   // sidebar) filters are ANDed. An empty origin filter means "(all)".
   if (m_nFilter != kFILTER_ALL  &&  FileCategory (pFile) != m_nFilter)
      return false;

   std::string sName = ExtractFilename (pFile->Url ());

   if (!m_sTextFilter.empty ()  &&  UTILS::ToLowerEx (sName).find (m_sTextFilter) == std::string::npos)
      return false;

   if (!m_sContainerFilter.empty ()  &&  pFile->ContainerName () != m_sContainerFilter)
      return false;

   return true;
}

void IW_NETWORK_FILES::ApplyFilter ()
{
   if (!m_pRows)
      return;

   for (size_t nIndex = 0; nIndex < m_apFiles.size (); nIndex++)
   {
      Rml::Element* pRow = m_pRows->GetChild ((int) nIndex);
      if (!pRow)
         continue;

      if (MatchesFilter (m_apFiles[nIndex]))
         pRow->RemoveProperty ("display");
      else
         pRow->SetProperty ("display", "none");
   }
}

void IW_NETWORK_FILES::Filter (int nFilter)
{
   m_nFilter = nFilter;
   ApplyFilter ();
}

void IW_NETWORK_FILES::FilterText (const std::string& sText)
{
   UTILS::ToLower (sText, m_sTextFilter);
   ApplyFilter ();
}

void IW_NETWORK_FILES::FilterContainer (const std::string& sContainer)
{
   m_sContainerFilter = sContainer;
   ApplyFilter ();
}

int IW_NETWORK_FILES::FileCount () const
{
   return (int)m_apFiles.size ();
}

SNEEZE::FILE* IW_NETWORK_FILES::SelectedFile () const
{
   if (m_nSelected >= 0 && m_nSelected < (int)m_apFiles.size ())
      return m_apFiles[m_nSelected];
   return nullptr;
}

void IW_NETWORK_FILES::Deselect ()
{
   if (m_nSelected >= 0)
   {
      Rml::Element* pRow = m_pRows->GetChild (m_nSelected);
      if (pRow)
         pRow->SetClass ("selected", false);
      m_nSelected = -1;
   }
}

void IW_NETWORK_FILES::SelectRow (int nIndex)
{
   if (m_nSelected >= 0)
   {
      Rml::Element* pOldRow = m_pRows->GetChild (m_nSelected);
      if (pOldRow)
         pOldRow->SetClass ("selected", false);
   }

   m_nSelected = nIndex;

   if (m_nSelected >= 0)
   {
      Rml::Element* pNewRow = m_pRows->GetChild (m_nSelected);
      if (pNewRow)
         pNewRow->SetClass ("selected", true);

      if (m_pBody)
         m_pBody->ShowDetails ();
   }
}

void IW_NETWORK_FILES::ProcessEvent (Rml::Event& Event)
{
   Rml::Element* pTarget = Event.GetTargetElement ();

   while (pTarget && pTarget != m_pRows)
   {
      if (pTarget->GetParentNode () == m_pRows)
      {
         int nChildCount = m_pRows->GetNumChildren ();
         for (int nIndex = 0; nIndex < nChildCount; nIndex++)
         {
            if (m_pRows->GetChild (nIndex) == pTarget)
            {
               SelectRow (nIndex);
               break;
            }
         }
         break;
      }
      pTarget = pTarget->GetParentNode ();
   }
}

} // namespace RUBIDIUM
