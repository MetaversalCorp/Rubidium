// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/security/Security.h"

namespace RUBIDIUM
{

const char* IW_SECURITY::s_sRml =
R"rml(
<div id="security-containers" class="panel-sidebar"></div>
<div id="security-preview" class="panel-main"></div>
)rml";

const FN_CREATEWIDGET IW_SECURITY::s_afnCreateWidget[kWIDGET_COUNT] =
{
   IW_SECURITY_CONTAINERS::Create,
   IW_SECURITY_PREVIEW::Create,
};

IW_SECURITY::IW_SECURITY ()
{
}

IW_SECURITY::~IW_SECURITY ()
{
}

bool IW_SECURITY::Initialize (Rml::Element* pContainer)
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

IW_SECURITY_CONTAINERS* IW_SECURITY::Containers () const { return static_cast<IW_SECURITY_CONTAINERS*> (Widget (kWIDGET_CONTAINERS)); }
IW_SECURITY_PREVIEW*    IW_SECURITY::Preview ()    const { return static_cast<IW_SECURITY_PREVIEW*>    (Widget (kWIDGET_PREVIEW)); }

} // namespace RUBIDIUM
