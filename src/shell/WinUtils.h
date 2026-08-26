// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_SHELL_WINUTILS_H
#define RUBIDIUM_SHELL_WINUTILS_H

#include <string>

namespace RUBIDIUM
{
   class WINUTILS
   {
   public:
      static int ScaleDpi   (HWND hWnd, int nValue);
      static int ScaleDpiEx (int nValue, UINT uDpi);

      // UTF-8 face name of the current Windows UI font (lfMessageFont), DPI-aware.
      static std::string DefaultFontFamily (HWND hWnd = nullptr);

      // Installed font file name for a family (e.g. "segoeui.ttf"). Empty if not found.
      static std::string FontFilename (const std::string& sFontFamily);

      // Absolute UTF-8 path to the regular face of an installed family. Empty if not found.
      static std::string FontFilePath (const std::string& sFontFamily);

      static void SetMenuItemState (HMENU hMenu, UINT uItem, bool enabled);
      static void RectCenterBox (RECT& rcBox, const RECT& rcBound);
   };
}

#endif
