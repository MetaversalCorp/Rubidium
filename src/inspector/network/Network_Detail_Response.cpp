// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/network/Network.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="hex-toolbar" id="hex-toolbar">
   <div class="hex-tb-btn" id="hex-prev">&#x25C0;</div>
   <div class="hex-tb-addr" id="hex-tb-addr">0x00000000</div>
   <div class="hex-tb-btn" id="hex-next">&#x25B6;</div>
</div>
<div class="hex-area" id="hex-area"><div class="hex-col-addr" id="hex-col-addr"></div><div class="hex-col-bytes" id="hex-col-bytes"></div><div class="hex-col-chars" id="hex-col-chars"></div></div>
)rml";

static std::string EscapeChar (char c)
{
   if (c == '<')  return "&lt;";
   if (c == '>')  return "&gt;";
   if (c == '&')  return "&amp;";
   return std::string (1, c);
}

IW_NETWORK_DETAIL_RESPONSE::IW_NETWORK_DETAIL_RESPONSE () :
   m_pColAddr   (nullptr),
   m_pColBytes  (nullptr),
   m_pColChars  (nullptr),
   m_pTbAddr    (nullptr),
   m_pBtnPrev   (nullptr),
   m_pBtnNext   (nullptr),
   m_nPageOffset (0),
   m_nSelected  (-1)
{
}

IW_NETWORK_DETAIL_RESPONSE::~IW_NETWORK_DETAIL_RESPONSE ()
{
   if (m_pBtnPrev)   m_pBtnPrev->RemoveEventListener (Rml::EventId::Click, this);
   if (m_pBtnNext)   m_pBtnNext->RemoveEventListener (Rml::EventId::Click, this);
   if (m_pColBytes)  m_pColBytes->RemoveEventListener (Rml::EventId::Click, this);
   if (m_pColChars)  m_pColChars->RemoveEventListener (Rml::EventId::Click, this);
}

bool IW_NETWORK_DETAIL_RESPONSE::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pColAddr  = m_pContainer->GetElementById ("hex-col-addr");
   m_pColBytes = m_pContainer->GetElementById ("hex-col-bytes");
   m_pColChars = m_pContainer->GetElementById ("hex-col-chars");
   m_pTbAddr   = m_pContainer->GetElementById ("hex-tb-addr");
   m_pBtnPrev  = m_pContainer->GetElementById ("hex-prev");
   m_pBtnNext  = m_pContainer->GetElementById ("hex-next");

   if (m_pBtnPrev)   m_pBtnPrev->AddEventListener (Rml::EventId::Click, this);
   if (m_pBtnNext)   m_pBtnNext->AddEventListener (Rml::EventId::Click, this);
   if (m_pColBytes)  m_pColBytes->AddEventListener (Rml::EventId::Click, this);
   if (m_pColChars)  m_pColChars->AddEventListener (Rml::EventId::Click, this);

   return (m_pColAddr && m_pColBytes && m_pColChars);
}

void IW_NETWORK_DETAIL_RESPONSE::ShowFile (SNEEZE::FILE* pFile)
{
   if (!m_pContainer || !pFile)
      return;

   m_aData.clear ();
   pFile->ReadData (m_aData);

   m_nPageOffset = 0;
   m_nSelected = -1;

   RenderPage ();
}

void IW_NETWORK_DETAIL_RESPONSE::RenderPage ()
{
   if (!m_pColAddr || !m_pColBytes || !m_pColChars)
      return;

   if (m_aData.empty ())
   {
      m_pColAddr->SetInnerRML ("");
      m_pColBytes->SetInnerRML ("<div class=\"hex-line\">No response body available</div>");
      m_pColChars->SetInnerRML ("");
      if (m_pTbAddr) m_pTbAddr->SetInnerRML ("0x00000000");
      return;
   }

   int nPageBytes = kROWS_PER_PAGE * kBYTES_PER_ROW;
   int nDataSize  = (int)m_aData.size ();

   if (m_nPageOffset >= nDataSize)
      m_nPageOffset = 0;

   char sTbAddr[16];
   std::snprintf (sTbAddr, sizeof (sTbAddr), "0x%08X", (unsigned)m_nPageOffset);
   if (m_pTbAddr)
      m_pTbAddr->SetInnerRML (sTbAddr);

   std::string sAddrs;
   std::string sBytes;
   std::string sChars;

   for (int nRow = 0; nRow < kROWS_PER_PAGE; nRow++)
   {
      int nRowOffset = m_nPageOffset + (nRow * kBYTES_PER_ROW);

      if (nRowOffset >= nDataSize)
         break;

      char sAddr[16];
      std::snprintf (sAddr, sizeof (sAddr), "%08X", (unsigned)nRowOffset);
      sAddrs += "<div class=\"hex-line\" id=\"addr-";
      sAddrs += std::to_string (nRow);
      sAddrs += "\">";
      sAddrs += sAddr;
      sAddrs += "</div>";

      sBytes += "<div class=\"hex-line\">";
      for (int n = 0; n < kBYTES_PER_ROW; n++)
      {
         if ((n % 4) == 0 && n != 0)
         {
            sBytes += "<div class=\"hex-byte hex-empty\">FF</div>";
         }

         int nByteIx = nRowOffset + n;
         if (nByteIx < nDataSize)
         {
            char sHex[4];
            std::snprintf (sHex, sizeof (sHex), "%02X", m_aData[nByteIx]);

            char sId[16];
            std::snprintf (sId, sizeof (sId), "b-%d", nRow * kBYTES_PER_ROW + n);

            sBytes += "<div class=\"hex-byte\" id=\"";
            sBytes += sId;
            sBytes += "\">";
            sBytes += sHex;
            sBytes += "</div>";
         }
         else
         {
            sBytes += "<div class=\"hex-byte hex-empty\">..</div>";
         }
      }
      sBytes += "</div>";

      sChars += "<div class=\"hex-line\">";
      for (int n = 0; n < kBYTES_PER_ROW; n++)
      {
         int nByteIx = nRowOffset + n;
         if (nByteIx < nDataSize)
         {
            uint8_t nByte = m_aData[nByteIx];
            std::string sChar = (nByte >= 32 && nByte < 127) ? EscapeChar ((char)nByte) : ".";

            char sId[32];
            std::snprintf (sId, sizeof (sId), "c-%d", nRow * kBYTES_PER_ROW + n);
            sChars += "<div class=\"hex-char\" id=\"";
            sChars += sId;
            sChars += "\">";
            sChars += sChar;
            sChars += "</div>";
         }
      }
      sChars += "</div>";
   }

   m_pColAddr->SetInnerRML (sAddrs);
   m_pColBytes->SetInnerRML (sBytes);
   m_pColChars->SetInnerRML (sChars);
}

void IW_NETWORK_DETAIL_RESPONSE::SelectByte (int nByteIndex)
{
   if (m_nSelected == nByteIndex)
      return;

   if (m_nSelected >= 0)
   {
      char sId[32];
      std::snprintf (sId, sizeof (sId), "b-%d", m_nSelected);
      Rml::Element* pOldByte = m_pColBytes->GetElementById (sId);
      if (pOldByte) pOldByte->SetClass ("selected", false);

      std::snprintf (sId, sizeof (sId), "c-%d", m_nSelected);
      Rml::Element* pOldChar = m_pColChars->GetElementById (sId);
      if (pOldChar) pOldChar->SetClass ("selected", false);

      int nOldRow = m_nSelected / kBYTES_PER_ROW;
      std::snprintf (sId, sizeof (sId), "addr-%d", nOldRow);
      Rml::Element* pOldAddr = m_pColAddr->GetElementById (sId);
      if (pOldAddr) pOldAddr->SetClass ("selected", false);
   }

   m_nSelected = nByteIndex;

   if (m_nSelected >= 0)
   {
      char sId[32];
      std::snprintf (sId, sizeof (sId), "b-%d", m_nSelected);
      Rml::Element* pNewByte = m_pColBytes->GetElementById (sId);
      if (pNewByte) pNewByte->SetClass ("selected", true);

      std::snprintf (sId, sizeof (sId), "c-%d", m_nSelected);
      Rml::Element* pNewChar = m_pColChars->GetElementById (sId);
      if (pNewChar) pNewChar->SetClass ("selected", true);

      int nNewRow = m_nSelected / kBYTES_PER_ROW;
      std::snprintf (sId, sizeof (sId), "addr-%d", nNewRow);
      Rml::Element* pNewAddr = m_pColAddr->GetElementById (sId);
      if (pNewAddr) pNewAddr->SetClass ("selected", true);
   }
}

void IW_NETWORK_DETAIL_RESPONSE::ProcessEvent (Rml::Event& Event)
{
   Rml::Element* pCurrent = Event.GetCurrentElement ();

   if (pCurrent == m_pBtnPrev)
   {
      int nPageBytes = kROWS_PER_PAGE * kBYTES_PER_ROW;
      if (m_nPageOffset >= nPageBytes)
      {
         m_nPageOffset -= nPageBytes;
         m_nSelected = -1;
         RenderPage ();
      }
   }
   else if (pCurrent == m_pBtnNext)
   {
      int nPageBytes = kROWS_PER_PAGE * kBYTES_PER_ROW;
      if (m_nPageOffset + nPageBytes < (int)m_aData.size ())
      {
         m_nPageOffset += nPageBytes;
         m_nSelected = -1;
         RenderPage ();
      }
   }
   else if (pCurrent == m_pColBytes || pCurrent == m_pColChars)
   {
      Rml::Element* pTarget = Event.GetTargetElement ();
      std::string sId = pTarget->GetId ();

      int nIndex = -1;
      if (sId.size () > 2 && (sId[0] == 'b' || sId[0] == 'c') && sId[1] == '-')
         nIndex = std::atoi (sId.c_str () + 2);

      if (nIndex >= 0)
         SelectByte (nIndex);
   }
}

} // namespace RUBIDIUM
