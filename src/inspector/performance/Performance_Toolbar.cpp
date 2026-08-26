// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/performance/Performance.h"
#include "inspector/common/Button.h"

using namespace RUBIDIUM;

static IW_BUTTON::IW_BUTTON_DATA aButtonData[IW_PERFORMANCE_TOOLBAR::kBUTTON_COUNT] =
{
   IW_BUTTON::IW_BUTTON_DATA ("btn-record",   "&#xE061;", IW_BUTTON::kTOGGLE),
   IW_BUTTON::IW_BUTTON_DATA ("btn-clear",    "&#xF08C;", IW_BUTTON::kCLICK),
};

static const char* s_sRml =
R"rml(
<div class="toolbar-row">
   <div id="btn-record" class="toolbar-btn"></div>
   <div id="btn-clear" class="toolbar-btn"></div>
</div>
)rml";

IW_PERFORMANCE_TOOLBAR::IW_PERFORMANCE_TOOLBAR ()
{
}

IW_PERFORMANCE_TOOLBAR::~IW_PERFORMANCE_TOOLBAR ()
{
}

bool IW_PERFORMANCE_TOOLBAR::Initialize (Rml::Element* pContainer)
{
   bool bResult = false;

   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   if (CreateWidgetsEx (aButtonData, kBUTTON_COUNT))
   {
      bResult = true;

      Button (kBUTTON_RECORD)->SetActive (true);
   }

   return bResult;
}

IW_BUTTON* IW_PERFORMANCE_TOOLBAR::Button (int nIndex) const
{
   return static_cast<IW_BUTTON*> (Widget (nIndex));
}
