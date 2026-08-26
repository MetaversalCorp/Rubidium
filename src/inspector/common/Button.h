// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_COMMON_BUTTON_H
#define RUBIDIUM_INSPECTOR_COMMON_BUTTON_H

#include "inspector/InspectorRml.h"

namespace RUBIDIUM
{
   class IW_BUTTON : public INSPECTOR_WIDGET, public Rml::EventListener
   {
   public:
      enum eBEHAVIOR
      {
         kTOGGLE,
         kCLICK
      };

      class IW_BUTTON_DATA : public INSPECTOR_WIDGET_DATA
      {
      public:
         IW_BUTTON_DATA (const char* sId, const char* sIcon, eBEHAVIOR eBehavior);

         INSPECTOR_WIDGET* CreateWidget () override;

      public:
         const char*       sId;
         const char*       sIcon;
         eBEHAVIOR         eBehavior;
      };

   public:
      IW_BUTTON (const IW_BUTTON_DATA* pBD);
      ~IW_BUTTON () override;

      const char* Id         ()                         override;
      bool        Initialize (Rml::Element* pContainer) override;

      bool IsActive () const;
      void SetActive (bool bActive);

      void ProcessEvent (Rml::Event& Event) override;

   private:
      const char* m_sId;
      const char* m_sIcon;
      eBEHAVIOR   m_eBehavior;
      bool        m_bActive;
   };
} // namespace RUBIDIUM

#endif // RUBIDIUM_INSPECTOR_COMMON_BUTTON_H
