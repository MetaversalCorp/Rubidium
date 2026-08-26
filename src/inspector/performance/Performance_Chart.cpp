// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/performance/Performance.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="placeholder-icon" style="padding-top: 16dp; font-size: 32dp;">&#xE6C1;</div>
<div class="placeholder" style="padding-top: 4dp; font-size: 11dp;">Timeline chart coming soon</div>
)rml";

IW_PERFORMANCE_CHART::IW_PERFORMANCE_CHART ()
{
}

IW_PERFORMANCE_CHART::~IW_PERFORMANCE_CHART ()
{
}

bool IW_PERFORMANCE_CHART::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   return true;
}

} // namespace RUBIDIUM
