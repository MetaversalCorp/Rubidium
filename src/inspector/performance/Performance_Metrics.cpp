// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/performance/Performance.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="metric-card">
   <div class="metric-label">FPS</div>
   <div class="metric-value" id="perf-val-fps">--</div>
</div>
<div class="metric-card">
   <div class="metric-label">Frame</div>
   <div class="metric-value" id="perf-val-frame">--</div>
</div>
<div class="metric-card">
   <div class="metric-label">Submit</div>
   <div class="metric-value" id="perf-val-submit">--</div>
</div>
<div class="metric-card">
   <div class="metric-label">Render</div>
   <div class="metric-value" id="perf-val-render">--</div>
</div>
)rml";

IW_PERFORMANCE_METRICS::IW_PERFORMANCE_METRICS () :
   m_pFps    (nullptr),
   m_pFrame  (nullptr),
   m_pSubmit (nullptr),
   m_pRender (nullptr)
{
}

IW_PERFORMANCE_METRICS::~IW_PERFORMANCE_METRICS ()
{
}

bool IW_PERFORMANCE_METRICS::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pFps    = m_pContainer->GetElementById ("perf-val-fps");
   m_pFrame  = m_pContainer->GetElementById ("perf-val-frame");
   m_pSubmit = m_pContainer->GetElementById ("perf-val-submit");
   m_pRender = m_pContainer->GetElementById ("perf-val-render");

   return (m_pFps != nullptr);
}

void IW_PERFORMANCE_METRICS::Update (double dFps, double dFrame, double dSubmit, double dRender)
{
   char szBuf[32];

   if (m_pFps)
   {
      std::snprintf (szBuf, sizeof (szBuf), "%.0f", dFps);
      m_pFps->SetInnerRML (szBuf);
   }

   if (m_pFrame)
   {
      std::snprintf (szBuf, sizeof (szBuf), "%.1fms", dFrame);
      m_pFrame->SetInnerRML (szBuf);
   }

   if (m_pSubmit)
   {
      std::snprintf (szBuf, sizeof (szBuf), "%.1fms", dSubmit);
      m_pSubmit->SetInnerRML (szBuf);
   }

   if (m_pRender)
   {
      std::snprintf (szBuf, sizeof (szBuf), "%.1fms", dRender);
      m_pRender->SetInnerRML (szBuf);
   }
}

} // namespace RUBIDIUM
