// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// SETTINGS_RML — cross-platform Chrome-style Settings window. Owns an
// RMLUI_SDL window via composition (same pattern as INSPECTOR_RML) so it
// registers with the central SDL event loop and inherits DPI/font handling.

#ifndef RUBIDIUM_SETTINGS_UI_SETTINGSRML_H
#define RUBIDIUM_SETTINGS_UI_SETTINGSRML_H

namespace RUBIDIUM
{
   class SETTINGS_RML
   {
   public:
      SETTINGS_RML ();
      ~SETTINGS_RML ();

      bool Initialize ();

      void Toggle ();
      void Show ();
      bool IsVisible () const;

      // True while the Settings window is open (shown or minimized) -- only a
      // real close hides it. Used to keep the owner window modally blocked even
      // when Settings is minimized, matching the Win32 build.
      bool IsOpen () const;

      // Owner window (native handle) disabled while the modal Settings window is
      // shown, re-enabled when it closes. Set before the first Toggle ().
      void SetOwner (void* hOwner);

   private:
      class Impl;
      Impl* m_pImpl;
   };
} // namespace RUBIDIUM

#endif // RUBIDIUM_SETTINGS_UI_SETTINGSRML_H
