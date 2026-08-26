// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/network/Network.h"
#include "inspector/common/Collapse.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Timing</div>
<div class="detail-section-body">
   <div id="timing-body" class="detail-timing-body">Select a request to view timing</div>
</div>
)rml";

static std::string FormatMs (double dSeconds)
{
   double dMs = dSeconds * 1000.0;
   if (dMs < 1.0)
      return "<1 ms";
   if (dMs < 1000.0)
   {
      char sBuf[32];
      std::snprintf (sBuf, sizeof (sBuf), "%.1f ms", dMs);
      return sBuf;
   }
   char sBuf[32];
   std::snprintf (sBuf, sizeof (sBuf), "%.2f s", dMs / 1000.0);
   return sBuf;
}

IW_NETWORK_DETAIL_TIMING::IW_NETWORK_DETAIL_TIMING ()
{
}

bool IW_NETWORK_DETAIL_TIMING::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   COLLAPSE::Attach (m_pContainer, this, m_apSections);

   return true;
}

void IW_NETWORK_DETAIL_TIMING::ShowFile (SNEEZE::FILE* pFile)
{
   if (!m_pContainer || !pFile)
      return;

   Rml::Element* pBody = m_pContainer->GetElementById ("timing-body");
   if (!pBody)
      return;

   if (pFile->IsServedFromCache ())
   {
      pBody->SetInnerRML ("<div class=\"detail-kv\"><div class=\"detail-key\">Source:</div><div class=\"detail-value\">Served from cache</div></div>");
   }
   else
   {
      std::string sRml;

      double dQueued   = pFile->FetchQueuedTime ();
      double dStart    = pFile->FetchStartTime ();
      double dEnd      = pFile->FetchEndTime ();
      double dDuration = pFile->FetchDuration ();

      if (dQueued > 0.0 && dStart > 0.0)
         sRml += "<div class=\"detail-kv\"><div class=\"detail-key\">Queued:</div><div class=\"detail-value\">" + FormatMs (dStart - dQueued) + "</div></div>";

      if (dDuration > 0.0)
         sRml += "<div class=\"detail-kv\"><div class=\"detail-key\">Duration:</div><div class=\"detail-value\">" + FormatMs (dDuration) + "</div></div>";
      else
         sRml += "<div class=\"detail-kv\"><div class=\"detail-key\">Duration:</div><div class=\"detail-value\">pending</div></div>";

      if (dStart > 0.0)
      {
         char sBuf[32];
         std::snprintf (sBuf, sizeof (sBuf), "%.3f", dStart);
         sRml += "<div class=\"detail-kv\"><div class=\"detail-key\">Start:</div><div class=\"detail-value\">" + std::string (sBuf) + " s</div></div>";
      }

      if (dEnd > 0.0)
      {
         char sBuf[32];
         std::snprintf (sBuf, sizeof (sBuf), "%.3f", dEnd);
         sRml += "<div class=\"detail-kv\"><div class=\"detail-key\">End:</div><div class=\"detail-value\">" + std::string (sBuf) + " s</div></div>";
      }

      if (sRml.empty ())
         sRml = "No timing data available";

      pBody->SetInnerRML (sRml);
   }
}

void IW_NETWORK_DETAIL_TIMING::ProcessEvent (Rml::Event& Event)
{
   COLLAPSE::Toggle (Event.GetCurrentElement ());
}

IW_NETWORK_DETAIL_TIMING::~IW_NETWORK_DETAIL_TIMING ()
{
   COLLAPSE::Detach (this, m_apSections);
}

} // namespace RUBIDIUM
