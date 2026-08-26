// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/storage/Storage.h"

namespace RUBIDIUM
{

const char* IW_STORAGE::s_sRml =
R"rml(
<div id="storage-containers" class="panel-sidebar"></div>
<div id="storage-preview" class="panel-main" style="display: flex; flex-direction: column;"></div>
)rml";

const FN_CREATEWIDGET IW_STORAGE::s_afnCreateWidget[kWIDGET_COUNT] =
{
   IW_STORAGE_CONTAINERS::Create,
   IW_STORAGE_PREVIEW::Create,
};

IW_STORAGE::IW_STORAGE ()
{
}

IW_STORAGE::~IW_STORAGE ()
{
}

bool IW_STORAGE::Initialize (Rml::Element* pContainer)
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

IW_STORAGE_CONTAINERS* IW_STORAGE::Containers () const { return static_cast<IW_STORAGE_CONTAINERS*> (Widget (kWIDGET_CONTAINERS)); }
IW_STORAGE_PREVIEW*    IW_STORAGE::Preview ()    const { return static_cast<IW_STORAGE_PREVIEW*>    (Widget (kWIDGET_PREVIEW)); }

} // namespace RUBIDIUM
