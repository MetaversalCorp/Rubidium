// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/network/Network.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="waterfall-header">0 ms</div>
<div class="waterfall-bars"></div>
)rml";

IW_NETWORK_WATERFALL::IW_NETWORK_WATERFALL () :
   m_pHeader (nullptr),
   m_pBars   (nullptr)
{
}

bool IW_NETWORK_WATERFALL::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   m_pHeader = m_pContainer->GetChild (0);
   m_pBars   = m_pContainer->GetChild (1);

   return (m_pHeader  &&  m_pBars);
}

IW_NETWORK_WATERFALL::~IW_NETWORK_WATERFALL ()
{
}

} // namespace RUBIDIUM
