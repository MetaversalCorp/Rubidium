// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/elements/Elements.h"

namespace RUBIDIUM
{

IW_ELEMENTS_DETAIL_EVENTS::IW_ELEMENTS_DETAIL_EVENTS ()
{
}

IW_ELEMENTS_DETAIL_EVENTS::~IW_ELEMENTS_DETAIL_EVENTS ()
{
}

bool IW_ELEMENTS_DETAIL_EVENTS::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML ("<div class=\"placeholder\">Event listeners will appear here</div>");

   return true;
}

} // namespace RUBIDIUM
