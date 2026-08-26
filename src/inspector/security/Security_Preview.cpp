// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/security/Security.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="placeholder-icon">&#xE897;</div>
<div class="placeholder">Security Under Construction</div>
)rml";

IW_SECURITY_PREVIEW::IW_SECURITY_PREVIEW ()
{
}

IW_SECURITY_PREVIEW::~IW_SECURITY_PREVIEW ()
{
}

bool IW_SECURITY_PREVIEW::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   return true;
}

} // namespace RUBIDIUM
