// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_COMMON_COLLAPSE_H
#define RUBIDIUM_INSPECTOR_COMMON_COLLAPSE_H

namespace RUBIDIUM
{

// -----------------------------------------------------------------------------
// COLLAPSE -- shared collapsible "detail-section" behavior.
//
// A collapsible section is a "detail-section" header div immediately followed by
// a "detail-section-body" wrapper div. Clicking the header toggles a "collapsed"
// class on the header (which rotates its arrow via CSS) and hides/shows the body.
// -----------------------------------------------------------------------------

namespace COLLAPSE
{
   // Registers pListener for clicks on every "detail-section" header directly
   // under pContainer and records those headers in apSections for later detach.
   inline void Attach (Rml::Element* pContainer, Rml::EventListener* pListener, std::vector<Rml::Element*>& apSections)
   {
      if (pContainer)
      {
         int nCount = pContainer->GetNumChildren ();

         for (int n = 0; n < nCount; n++)
         {
            Rml::Element* pChild = pContainer->GetChild (n);

            if (pChild && pChild->IsClassSet ("detail-section"))
            {
               pChild->AddEventListener (Rml::EventId::Click, pListener);
               apSections.push_back (pChild);
            }
         }
      }
   }

   // Removes pListener from every header recorded by Attach and clears apSections.
   inline void Detach (Rml::EventListener* pListener, std::vector<Rml::Element*>& apSections)
   {
      for (Rml::Element* pSection : apSections)
      {
         if (pSection)
            pSection->RemoveEventListener (Rml::EventId::Click, pListener);
      }

      apSections.clear ();
   }

   // Toggles the section whose header is pHeader. Returns true when pHeader is a
   // section header followed by a section body (i.e. a toggle actually occurred).
   inline bool Toggle (Rml::Element* pHeader)
   {
      bool bToggled = false;

      if (pHeader && pHeader->IsClassSet ("detail-section"))
      {
         Rml::Element* pBody = pHeader->GetNextSibling ();

         if (pBody && pBody->IsClassSet ("detail-section-body"))
         {
            bool bCollapsed = pHeader->IsClassSet ("collapsed");

            pHeader->SetClass ("collapsed", !bCollapsed);
            pBody->SetProperty ("display", bCollapsed ? "block" : "none");

            bToggled = true;
         }
      }

      return bToggled;
   }
}

} // namespace RUBIDIUM

#endif // RUBIDIUM_INSPECTOR_COMMON_COLLAPSE_H
