// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/elements/Elements.h"

namespace RUBIDIUM
{

const char* IW_ELEMENTS::s_sRml =
R"rml(
<div id="elements-containers" class="panel-sidebar"></div>
<div id="elements-tree"       class="split-left"></div>
<div id="elements-details"    class="split-right"></div>
)rml";

const FN_CREATEWIDGET IW_ELEMENTS::s_afnCreateWidget[kWIDGET_COUNT] =
{
   IW_ELEMENTS_CONTAINERS::Create,
   IW_ELEMENTS_TREE::Create,
   IW_ELEMENTS_DETAILS::Create,
};

IW_ELEMENTS::IW_ELEMENTS ()
{
}

IW_ELEMENTS::~IW_ELEMENTS ()
{
}

bool IW_ELEMENTS::Initialize (Rml::Element* pContainer)
{
   bool bResult = false;

   m_pContainer = pContainer;

   if (m_pContainer)
   {
      m_pContainer->SetInnerRML (s_sRml);

      bResult = CreateWidgets (s_afnCreateWidget, kWIDGET_COUNT);

      if (bResult)
      {
         Containers ()->SetTree (Tree ());
         Tree ()->SetDetails (Details ());
      }
   }

   return bResult;
}

IW_ELEMENTS_CONTAINERS* IW_ELEMENTS::Containers () const { return static_cast<IW_ELEMENTS_CONTAINERS*> (Widget (kWIDGET_CONTAINERS)); }
IW_ELEMENTS_TREE*       IW_ELEMENTS::Tree ()       const { return static_cast<IW_ELEMENTS_TREE*>       (Widget (kWIDGET_TREE)); }
IW_ELEMENTS_DETAILS*    IW_ELEMENTS::Details ()    const { return static_cast<IW_ELEMENTS_DETAILS*>    (Widget (kWIDGET_DETAILS)); }

} // namespace RUBIDIUM
