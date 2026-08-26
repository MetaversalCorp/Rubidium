// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/network/Network.h"
#include "inspector/common/Collapse.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> General</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">Request URL:</div><div class="detail-value" id="hdr-url">Select a request to view details</div></div>
   <div class="detail-kv"><div class="detail-key">Request Method:</div><div class="detail-value" id="hdr-method"></div></div>
   <div class="detail-kv"><div class="detail-key">Status Code:</div><div class="detail-value" id="hdr-status"></div></div>
   <div class="detail-kv"><div class="detail-key">Remote Address:</div><div class="detail-value" id="hdr-ipaddress"></div></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Response Headers</div>
<div class="detail-section-body">
   <div id="hdr-response-headers"></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Request Headers</div>
<div class="detail-section-body">
   <div id="hdr-request-headers"></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Cache Info</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">Container:</div><div class="detail-value" id="hdr-container"></div></div>
   <div class="detail-kv"><div class="detail-key">Served from cache:</div><div class="detail-value" id="hdr-cached"></div></div>
   <div class="detail-kv"><div class="detail-key">File Index:</div><div class="detail-value" id="hdr-file-ix"></div></div>
   <div class="detail-kv"><div class="detail-key">Asset Index:</div><div class="detail-value" id="hdr-asset-ix"></div></div>
</div>
)rml";

static std::string FormatBytes (uint64_t nBytes)
{
   if (nBytes == 0)
      return "0 B";
   if (nBytes < 1024)
      return std::to_string (nBytes) + " B";
   if (nBytes < 1024 * 1024)
      return std::to_string (nBytes / 1024) + " KB";
   return std::to_string (nBytes / (1024 * 1024)) + " MB";
}

IW_NETWORK_DETAIL_HEADERS::IW_NETWORK_DETAIL_HEADERS ()
{
}

bool IW_NETWORK_DETAIL_HEADERS::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   COLLAPSE::Attach (m_pContainer, this, m_apSections);

   return true;
}

void IW_NETWORK_DETAIL_HEADERS::ShowFile (SNEEZE::FILE* pFile)
{
   if (!m_pContainer || !pFile)
      return;

   Rml::Element* pUrl         = m_pContainer->GetElementById ("hdr-url");
   Rml::Element* pMethod      = m_pContainer->GetElementById ("hdr-method");
   Rml::Element* pStatus      = m_pContainer->GetElementById ("hdr-status");
   Rml::Element* pAddress     = m_pContainer->GetElementById ("hdr-ipaddress");
   Rml::Element* pContainer   = m_pContainer->GetElementById ("hdr-container");
   Rml::Element* pCached      = m_pContainer->GetElementById ("hdr-cached");
   Rml::Element* pFileIx      = m_pContainer->GetElementById ("hdr-file-ix");
   Rml::Element* pAssetIx     = m_pContainer->GetElementById ("hdr-asset-ix");
   Rml::Element* pRspHeaders  = m_pContainer->GetElementById ("hdr-response-headers");
   Rml::Element* pReqHeaders  = m_pContainer->GetElementById ("hdr-request-headers");

   if (pUrl)         pUrl->SetInnerRML (pFile->Url ());
   if (pMethod)      pMethod->SetInnerRML ("GET");
   if (pStatus)      pStatus->SetInnerRML (pFile->HttpStatus () ? std::to_string (pFile->HttpStatus ()) : "");
   if (pAddress)     pAddress->SetInnerRML (pFile->RemoteAddress ());
   if (pCached)      pCached->SetInnerRML (pFile->IsServedFromCache () ? "Yes" : "No");
   if (pFileIx)      pFileIx->SetInnerRML (std::to_string (pFile->FileIx ()));
   if (pAssetIx)     pAssetIx->SetInnerRML (std::to_string (pFile->AssetIx ()));
   if (pContainer)   pContainer->SetInnerRML ("");

   if (pRspHeaders)
   {
      auto mapHeaders = pFile->RspHeaders ();
      if (mapHeaders.empty ())
      {
         pRspHeaders->SetInnerRML ("<div class=\"detail-kv\"><div class=\"detail-value\">No headers available</div></div>");
      }
      else
      {
         std::string sRml;
         for (auto& pair : mapHeaders)
            sRml += "<div class=\"detail-kv\"><div class=\"detail-key\">" + pair.first + ":</div><div class=\"detail-value\">" + pair.second + "</div></div>";
         pRspHeaders->SetInnerRML (sRml);
      }
   }

   if (pReqHeaders)
   {
      auto mapHeaders = pFile->ReqHeaders ();
      if (mapHeaders.empty ())
      {
         pReqHeaders->SetInnerRML ("<div class=\"detail-kv\"><div class=\"detail-value\">No headers available</div></div>");
      }
      else
      {
         std::string sRml;
         for (auto& pair : mapHeaders)
            sRml += "<div class=\"detail-kv\"><div class=\"detail-key\">" + pair.first + ":</div><div class=\"detail-value\">" + pair.second + "</div></div>";
         pReqHeaders->SetInnerRML (sRml);
      }
   }
}

void IW_NETWORK_DETAIL_HEADERS::ProcessEvent (Rml::Event& Event)
{
   Rml::Element* pHeader = Event.GetCurrentElement ();
   std::string s;

   bool bToggled = COLLAPSE::Toggle (pHeader);

   if (bToggled)
   {
      Rml::Element* pArrow = pHeader->GetFirstChild ();

      if (pArrow)
      {
         if (pHeader->IsClassSet ("collapsed"))
            s = "&#xE5C6;";
         else s = "&#xE5C5;";

         pArrow->SetInnerRML (s);
      }
   }
}

IW_NETWORK_DETAIL_HEADERS::~IW_NETWORK_DETAIL_HEADERS ()
{
   COLLAPSE::Detach (this, m_apSections);
}

} // namespace RUBIDIUM
