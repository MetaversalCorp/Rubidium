// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/network/Network.h"

namespace RUBIDIUM
{

const char* IW_NETWORK::s_sRml =
R"rml(
<div id="network-toolbar"   class="panel-toolbar"></div>
<div id="network-waterfall" class="network-waterfall"></div>
<div id="network-body"      class="panel-body"></div>
<div id="network-statusbar" class="panel-statusbar"></div>
)rml";

const FN_CREATEWIDGET IW_NETWORK::s_afnCreateWidget[kWIDGET_COUNT] =
{
   IW_NETWORK_TOOLBAR::Create,
   IW_NETWORK_WATERFALL::Create,
   IW_NETWORK_BODY::Create,
   IW_NETWORK_STATUSBAR::Create,
};

IW_NETWORK::IW_NETWORK ()
{
}

IW_NETWORK::~IW_NETWORK ()
{
}

bool IW_NETWORK::Initialize (Rml::Element* pContainer)
{
   bool bResult = false;

   m_pContainer = pContainer;

   if (m_pContainer)
   {
      m_pContainer->SetInnerRML (s_sRml);

      bResult = CreateWidgets (s_afnCreateWidget, kWIDGET_COUNT);

      if (bResult)
      {
         auto* pToolbar = static_cast<IW_NETWORK_TOOLBAR*> (Widget (kWIDGET_TOOLBAR));
         auto* pBody    = static_cast<IW_NETWORK_BODY*>    (Widget (kWIDGET_BODY));

         if (pToolbar  &&  pBody)
            pToolbar->SetFiles (pBody->Files ());
      }
   }

   return bResult;
}

} // namespace RUBIDIUM
