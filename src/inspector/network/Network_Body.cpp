// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/network/Network.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div id="network-containers" class="panel-sidebar"></div>
<div id="network-files" class="panel-main"></div>
<div id="network-details" class="network-details"></div>
)rml";

const FN_CREATEWIDGET IW_NETWORK_BODY::s_afnCreateWidget[kWIDGET_COUNT] =
{
   IW_NETWORK_CONTAINERS::Create,
   IW_NETWORK_FILES::Create,
   IW_NETWORK_DETAILS::Create,
};

IW_NETWORK_BODY::IW_NETWORK_BODY () :
   m_bDetailsVisible (false),
   m_pDetailFile     (nullptr)
{
}

IW_NETWORK_BODY::~IW_NETWORK_BODY ()
{
}

bool IW_NETWORK_BODY::Initialize (Rml::Element* pContainer)
{
   bool bResult = false;

   m_pContainer = pContainer;

   if (m_pContainer)
   {
      m_pContainer->SetInnerRML (s_sRml);

      bResult = CreateWidgets (s_afnCreateWidget, kWIDGET_COUNT);

      if (bResult)
      {
         Files ()->SetBody (this);
         Details ()->SetBody (this);
         Containers ()->SetFiles (Files ());
      }
   }

   return bResult;
}

void IW_NETWORK_BODY::ShowDetails ()
{
   SNEEZE::FILE* pFile = Files ()->SelectedFile ();

   if (pFile == m_pDetailFile && m_bDetailsVisible)
      return;

   if (m_pDetailFile)
      m_pDetailFile->Detach ();

   m_pDetailFile = pFile;

   if (m_pDetailFile)
      m_pDetailFile->Attach ();

   m_bDetailsVisible = true;
   Container (kWIDGET_DETAILS)->SetClass ("visible", true);

   if (m_pDetailFile)
   {
      Details ()->ShowFile (m_pDetailFile);

      // Jump straight to Preview for glTF/GLB -- the live viewport lives there.
      if (Details ()->TabPreview () && Details ()->TabPreview ()->IsGlb ())
         Details ()->SetActiveTab (IW_NETWORK_DETAILS::kWIDGET_PREVIEW);
   }
}

void IW_NETWORK_BODY::HideDetails ()
{
   if (m_bDetailsVisible)
   {
      m_bDetailsVisible = false;

      if (m_pDetailFile)
      {
         m_pDetailFile->Detach ();
         m_pDetailFile = nullptr;
      }

      Container (kWIDGET_DETAILS)->SetClass ("visible", false);
      Files ()->Deselect ();
   }
}

void IW_NETWORK_BODY::ResetDetails ()
{
   // Forget the detail file WITHOUT calling Detach (): on a page reset the old
   // network and its FILE objects may already be destroyed, so m_pDetailFile
   // can be dangling. Just drop the reference and hide the pane.
   m_pDetailFile     = nullptr;
   m_bDetailsVisible = false;
   Container (kWIDGET_DETAILS)->SetClass ("visible", false);
   Files ()->Deselect ();
}

void IW_NETWORK_BODY::OnFileDeleted (SNEEZE::FILE* pFile)
{
   if (m_pDetailFile == pFile)
   {
      m_pDetailFile->Detach ();
      m_pDetailFile = nullptr;

      if (m_bDetailsVisible)
      {
         m_bDetailsVisible = false;
         Container (kWIDGET_DETAILS)->SetClass ("visible", false);
         Files ()->Deselect ();
      }
   }
}

bool IW_NETWORK_BODY::IsDetailsVisible () const { return m_bDetailsVisible; }

IW_NETWORK_CONTAINERS* IW_NETWORK_BODY::Containers () const { return static_cast<IW_NETWORK_CONTAINERS*> (Widget (kWIDGET_CONTAINERS)); }
IW_NETWORK_FILES*      IW_NETWORK_BODY::Files ()      const { return static_cast<IW_NETWORK_FILES*>      (Widget (kWIDGET_FILES)); }
IW_NETWORK_DETAILS*    IW_NETWORK_BODY::Details ()    const { return static_cast<IW_NETWORK_DETAILS*>    (Widget (kWIDGET_DETAILS)); }

} // namespace RUBIDIUM
