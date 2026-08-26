// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/console/Console.h"

namespace RUBIDIUM
{

const char* IW_CONSOLE::s_sRml =
R"rml(
<div id="console-toolbar" class="panel-toolbar"></div>
<div id="console-body"    class="panel-body"></div>
)rml";

const FN_CREATEWIDGET IW_CONSOLE::s_afnCreateWidget[kWIDGET_COUNT] =
{
   IW_CONSOLE_TOOLBAR::Create,
   IW_CONSOLE_BODY::Create,
};

IW_CONSOLE::IW_CONSOLE ()
{
}

IW_CONSOLE::~IW_CONSOLE ()
{
}

bool IW_CONSOLE::Initialize (Rml::Element* pContainer)
{
   bool bResult = false;

   m_pContainer = pContainer;

   if (m_pContainer)
   {
      m_pContainer->SetInnerRML (s_sRml);

      bResult = CreateWidgets (s_afnCreateWidget, kWIDGET_COUNT);

      if (bResult)
      {
         auto* pToolbar = static_cast<IW_CONSOLE_TOOLBAR*> (Widget (kWIDGET_TOOLBAR));
         auto* pBody    = static_cast<IW_CONSOLE_BODY*>    (Widget (kWIDGET_BODY));

         if (pToolbar  &&  pBody)
            pToolbar->SetEntries (pBody->Entries ());
      }
   }

   return bResult;
}

} // namespace RUBIDIUM
