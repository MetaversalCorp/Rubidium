// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/performance/Performance.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div id="perf-log-lines">
   <div class="log-line" style="color: #808689;">Waiting for performance data...</div>
</div>
)rml";

IW_PERFORMANCE_LOG::IW_PERFORMANCE_LOG () :
   m_pLines (nullptr)
{
}

IW_PERFORMANCE_LOG::~IW_PERFORMANCE_LOG ()
{
}

bool IW_PERFORMANCE_LOG::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pLines = m_pContainer->GetElementById ("perf-log-lines");

   return (m_pLines != nullptr);
}

void IW_PERFORMANCE_LOG::AppendLine (const char* sLine)
{
   if (!m_pLines)
      return;

   Rml::ElementPtr pOwned = m_pLines->GetOwnerDocument ()->CreateElement ("div");
   Rml::Element* pLine = pOwned.get ();
   pLine->SetClass ("log-line", true);
   pLine->SetInnerRML (sLine);
   m_pLines->AppendChild (std::move (pOwned));
}

void IW_PERFORMANCE_LOG::Clear ()
{
   if (m_pLines)
      m_pLines->SetInnerRML ("");
}

} // namespace RUBIDIUM
