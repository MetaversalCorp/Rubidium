// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_SECURITY_H
#define RUBIDIUM_INSPECTOR_SECURITY_H

#include "inspector/InspectorRml.h"

namespace RUBIDIUM
{

/*******************************************************************************************************************************
**                                                   Container Sidebar                                                        **
*******************************************************************************************************************************/

class IW_SECURITY_CONTAINERS : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_SECURITY_CONTAINERS ();
   ~IW_SECURITY_CONTAINERS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_SECURITY_CONTAINERS (); }
   const char* Id () override { return "security-containers"; }
   bool Initialize (Rml::Element* pContainer) override;

   void AddContainer (const std::string& sName);
   void Clear ();

   const std::string& SelectedContainer () const;

   void ProcessEvent (Rml::Event& Event) override;

private:
   void UpdateSelection (Rml::Element* pItem);

   Rml::Element*            m_pItems;
   Rml::Element*            m_pItemAll;
   std::vector<std::string> m_asContainers;
   std::string              m_sSelected;
};

/*******************************************************************************************************************************
**                                                     Preview Pane                                                           **
*******************************************************************************************************************************/

class IW_SECURITY_PREVIEW : public INSPECTOR_WIDGET
{
public:
   IW_SECURITY_PREVIEW ();
   ~IW_SECURITY_PREVIEW () override;

   static INSPECTOR_WIDGET* Create () { return new IW_SECURITY_PREVIEW (); }
   const char* Id () override { return "security-preview"; }
   bool Initialize (Rml::Element* pContainer) override;
};

/*******************************************************************************************************************************
**                                                    Security Frame                                                          **
*******************************************************************************************************************************/

class IW_SECURITY : public INSPECTOR_WIDGET
{
public:
   IW_SECURITY ();
   ~IW_SECURITY () override;

   static INSPECTOR_WIDGET* Create () { return new IW_SECURITY (); }
   const char* Id () override { return "security"; }
   bool Initialize (Rml::Element* pContainer) override;

   IW_SECURITY_CONTAINERS* Containers () const;
   IW_SECURITY_PREVIEW*    Preview ()    const;

   enum eWIDGET
   {
      kWIDGET_CONTAINERS = 0,
      kWIDGET_PREVIEW    = 1,
      kWIDGET_COUNT      = 2
   };

private:
   static const char*           s_sRml;
   static const FN_CREATEWIDGET s_afnCreateWidget[kWIDGET_COUNT];
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_INSPECTOR_SECURITY_H
