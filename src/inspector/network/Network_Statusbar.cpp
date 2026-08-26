// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/network/Network.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div id="statusbar-label">0 requests</div>
)rml";

IW_NETWORK_STATUSBAR::IW_NETWORK_STATUSBAR () :
   m_pLabel (nullptr)
{
}

bool IW_NETWORK_STATUSBAR::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pLabel = m_pContainer->GetElementById ("statusbar-label");

   return (m_pLabel != nullptr);
}

IW_NETWORK_STATUSBAR::~IW_NETWORK_STATUSBAR ()
{
}

static std::string FormatSize (uint64_t nBytes)
{
   char szBuf[64];
   std::string sResult;

   if (nBytes == 0)
      sResult = "";
   else if (nBytes < 1024)
   {
      std::snprintf (szBuf, sizeof (szBuf), "%llu B", (unsigned long long)nBytes);
      sResult = szBuf;
   }
   else if (nBytes < 1024 * 1024)
   {
      std::snprintf (szBuf, sizeof (szBuf), "%.1f KB", nBytes / 1024.0);
      sResult = szBuf;
   }
   else
   {
      std::snprintf (szBuf, sizeof (szBuf), "%.1f MB", nBytes / (1024.0 * 1024.0));
      sResult = szBuf;
   }

   return sResult;
}

void IW_NETWORK_STATUSBAR::Update (int nTotal, int nFiltered, uint64_t nTransferred, bool bFilterActive)
{
   if (!m_pLabel)
      return;

   char szBuf[256];
   std::string sTransferred = FormatSize (nTransferred);

   if (bFilterActive)
   {
      if (nTransferred > 0)
         std::snprintf (szBuf, sizeof (szBuf), "%d / %d request%s  |  %s transferred", nFiltered, nTotal, (nTotal == 1 ? "" : "s"), sTransferred.c_str ());
      else
         std::snprintf (szBuf, sizeof (szBuf), "%d / %d request%s", nFiltered, nTotal, (nTotal == 1 ? "" : "s"));
   }
   else
   {
      if (nTransferred > 0)
         std::snprintf (szBuf, sizeof (szBuf), "%d request%s  |  %s transferred", nTotal, (nTotal == 1 ? "" : "s"), sTransferred.c_str ());
      else
         std::snprintf (szBuf, sizeof (szBuf), "%d request%s", nTotal, (nTotal == 1 ? "" : "s"));
   }

   m_pLabel->SetInnerRML (szBuf);
}

} // namespace RUBIDIUM
