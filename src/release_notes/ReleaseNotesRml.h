// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// RELEASE_NOTES_RML — a modeless "Release Notes" popup shown once after the client
// updates to a new version. Owns an RMLUI_SDL window via composition (same
// pattern as SETTINGS_RML / INSPECTOR_RML) so it registers with the central SDL
// event loop and inherits DPI/font handling. The release notes are supplied as
// Markdown (downloaded from the release manifest by RubidiumSetup) and converted
// to RML for display.

#ifndef RUBIDIUM_RELEASE_NOTES_RELEASENOTESRML_H
#define RUBIDIUM_RELEASE_NOTES_RELEASENOTESRML_H

#include <string>

namespace RUBIDIUM
{
   class RELEASE_NOTES_RML
   {
   public:
      RELEASE_NOTES_RML ();
      ~RELEASE_NOTES_RML ();

      // sVersion  -- e.g. "0.0.8"; shown in the header ("Release Notes v 0.0.8").
      // sMarkdown -- release notes in Markdown; converted to RML internally.
      bool Initialize (const std::string& sVersion, const std::string& sMarkdown);

      void Show ();
      bool IsVisible () const;
      bool IsOpen () const;

      // Owner window (native handle) disabled while the modal popup is shown,
      // re-enabled when it closes. Set before Show ().
      void SetOwner (void* hOwner);

   private:
      class Impl;
      Impl* m_pImpl;
   };
} // namespace RUBIDIUM

#endif // RUBIDIUM_RELEASE_NOTES_RELEASENOTESRML_H
