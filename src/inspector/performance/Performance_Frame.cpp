// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/performance/Performance.h"

namespace RUBIDIUM
{

const char* IW_PERFORMANCE::s_sRml =
R"rml(
<div id="perf-toolbar" class="panel-toolbar"></div>
<div id="perf-metrics" class="perf-metrics"></div>
<div id="perf-chart"   class="perf-chart"></div>
<div id="perf-log"     class="perf-log"></div>
)rml";

const FN_CREATEWIDGET IW_PERFORMANCE::s_afnCreateWidget[kWIDGET_COUNT] =
{
   IW_PERFORMANCE_TOOLBAR::Create,
   IW_PERFORMANCE_METRICS::Create,
   IW_PERFORMANCE_CHART::Create,
   IW_PERFORMANCE_LOG::Create,
};

IW_PERFORMANCE::IW_PERFORMANCE ()
{
}

IW_PERFORMANCE::~IW_PERFORMANCE ()
{
}

bool IW_PERFORMANCE::Initialize (Rml::Element* pContainer)
{
   bool bResult = false;

   m_pContainer = pContainer;

   if (m_pContainer)
   {
      m_pContainer->SetInnerRML (s_sRml);

      bResult = CreateWidgets (s_afnCreateWidget, kWIDGET_COUNT);
   }

   return bResult;
}

IW_PERFORMANCE_TOOLBAR* IW_PERFORMANCE::Toolbar () const { return static_cast<IW_PERFORMANCE_TOOLBAR*> (Widget (kWIDGET_TOOLBAR)); }
IW_PERFORMANCE_METRICS* IW_PERFORMANCE::Metrics () const { return static_cast<IW_PERFORMANCE_METRICS*> (Widget (kWIDGET_METRICS)); }
IW_PERFORMANCE_CHART*   IW_PERFORMANCE::Chart ()   const { return static_cast<IW_PERFORMANCE_CHART*>   (Widget (kWIDGET_CHART)); }
IW_PERFORMANCE_LOG*     IW_PERFORMANCE::Log ()     const { return static_cast<IW_PERFORMANCE_LOG*>     (Widget (kWIDGET_LOG)); }

} // namespace RUBIDIUM
