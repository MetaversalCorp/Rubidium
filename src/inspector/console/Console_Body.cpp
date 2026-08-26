// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/console/Console.h"

using namespace RUBIDIUM;

static const char* s_sRml =
R"rml(
<div id="console-containers" class="panel-sidebar"></div>
<div id="console-entries" class="panel-main"></div>
)rml";

const FN_CREATEWIDGET IW_CONSOLE_BODY::s_afnCreateWidget[kWIDGET_COUNT] =
{
   IW_CONSOLE_CONTAINERS::Create,
   IW_CONSOLE_ENTRIES::Create,
};

IW_CONSOLE_BODY::IW_CONSOLE_BODY ()
{
}

IW_CONSOLE_BODY::~IW_CONSOLE_BODY ()
{
}

bool IW_CONSOLE_BODY::Initialize (Rml::Element* pContainer)
{
   bool bResult = false;

   m_pContainer = pContainer;

   if (m_pContainer)
   {
      m_pContainer->SetInnerRML (s_sRml);

      bResult = CreateWidgets (s_afnCreateWidget, kWIDGET_COUNT);

      if (bResult)
         Containers ()->SetEntries (Entries ());
   }

   return bResult;
}

IW_CONSOLE_CONTAINERS* IW_CONSOLE_BODY::Containers () const { return static_cast<IW_CONSOLE_CONTAINERS*> (Widget (kWIDGET_CONTAINERS)); }
IW_CONSOLE_ENTRIES*    IW_CONSOLE_BODY::Entries ()    const { return static_cast<IW_CONSOLE_ENTRIES*>    (Widget (kWIDGET_ENTRIES)); }

void IW_CONSOLE_BODY::onEntryDeleted (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr)
{
/*
   if (m_pDetailEntry == pEntryPtr)
   {
      m_pDetailEntry->Detach ();
      m_pDetailEntry = nullptr;

      if (m_bDetailsVisible)
      {
         m_bDetailsVisible = false;
         Container (kWIDGET_DETAILS)->SetClass ("visible", false);
         Files ()->Deselect ();
      }
   }
*/
}

