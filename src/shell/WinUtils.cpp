// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "WinUtils.h"

using namespace RUBIDIUM;

//----------------------------------------------

static std::wstring Utf8ToWide (const std::string& sUtf8)
{
   std::wstring sResult;

   if (!sUtf8.empty ())
   {
      int nWideCount = MultiByteToWideChar (CP_UTF8, 0, sUtf8.c_str (), -1, nullptr, 0);

      if (nWideCount > 1)
      {
         sResult.resize (static_cast<size_t> (nWideCount - 1));
         MultiByteToWideChar (CP_UTF8, 0, sUtf8.c_str (), -1, sResult.data (), nWideCount);
      }
   }

   return sResult;
}

static std::string WideToUtf8 (const wchar_t* pWide)
{
   std::string sResult;

   if (pWide && pWide[0] != L'\0')
   {
      int nByteCount = WideCharToMultiByte (CP_UTF8, 0, pWide, -1, nullptr, 0, nullptr, nullptr);

      if (nByteCount > 1)
      {
         sResult.resize (static_cast<size_t> (nByteCount - 1));
         WideCharToMultiByte (CP_UTF8, 0, pWide, -1, sResult.data (), nByteCount, nullptr, nullptr);
      }
   }

   return sResult;
}

static bool QueryMessageLogFont (LOGFONTW* pLogFont, UINT uDpi)
{
   bool bResult = false;

   if (pLogFont)
   {
      NONCLIENTMETRICSW Metrics = {};

      Metrics.cbSize = sizeof (NONCLIENTMETRICSW);
      if (SystemParametersInfoForDpi (SPI_GETNONCLIENTMETRICS, Metrics.cbSize, &Metrics, false, uDpi) == FALSE)
      {
         Metrics.cbSize = sizeof (NONCLIENTMETRICSW) - sizeof (Metrics.iPaddedBorderWidth);
         if (SystemParametersInfoForDpi (SPI_GETNONCLIENTMETRICS, Metrics.cbSize, &Metrics, false, uDpi) != FALSE)
            bResult = true;
      }
      else
         bResult = true;

      if (bResult)
         *pLogFont = Metrics.lfMessageFont;
   }

   return bResult;
}

static bool StartsWithI (const std::wstring& sText, const std::wstring& sPrefix)
{
   std::wstring sLowerText, sLowerPrefix;

   UTILS::ToLower (sText, sLowerText);
   UTILS::ToLower (sPrefix, sLowerPrefix);

   return sLowerText.size () >= sLowerPrefix.size ()
      &&  sLowerText.compare (0, sLowerPrefix.size (), sLowerPrefix) == 0;
}

static bool IsVariantFaceName (const std::wstring& sDisplayName)
{
   bool bResult = false;

   static const wchar_t* aVariantTokens[] =
   {
      L" Bold", L" Italic", L" Semibold", L" Semilight", L" Light",
      L" Black", L" Extrabold", L" Extra Bold", L" Condensed"
   };

   std::wstring sLower;
   
   UTILS::ToLower (sDisplayName, sLower);

   for (const wchar_t* pToken : aVariantTokens)
   {
      if (sLower.find (UTILS::ToLowerEx (std::wstring (pToken))) != std::wstring::npos)
      {
         bResult = true;
         break;
      }
   }

   return bResult;
}

static std::vector<std::pair<std::wstring, std::wstring>> ReadFontRegistryValues (HKEY hRoot, const wchar_t* pSubkey)
{
   std::vector<std::pair<std::wstring, std::wstring>> aValues;
   HKEY hKey = nullptr;

   if (RegOpenKeyExW (hRoot, pSubkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
   {
      DWORD nIndex = 0;

      for (;;)
      {
         wchar_t szValueName[512] = {};
         BYTE    aData[2048]       = {};
         DWORD   nValueNameChars   = static_cast<DWORD> (std::size (szValueName));
         DWORD   nDataBytes        = sizeof (aData);
         DWORD   nType             = 0;
         LSTATUS nStatus           = RegEnumValueW (hKey, nIndex++, szValueName, &nValueNameChars,
            nullptr, &nType, aData, &nDataBytes);

         if (nStatus != ERROR_SUCCESS)
            break;

         if (nType == REG_SZ  &&  nDataBytes >= sizeof (wchar_t))
         {
            size_t nValueChars = nDataBytes / sizeof (wchar_t);
            const wchar_t* pValue = reinterpret_cast<const wchar_t*> (aData);

            if (nValueChars > 0  &&  pValue[nValueChars - 1] == L'\0')
               nValueChars--;

            std::wstring sName  (szValueName, nValueNameChars);
            std::wstring sValue (pValue, nValueChars);

            aValues.emplace_back (sName, sValue);
         }
      }

      RegCloseKey (hKey);
   }

   return aValues;
}

static std::wstring MakeAbsoluteFontPath (const std::wstring& sRegValue, bool bUserFont)
{
   std::wstring sResult;

   if (!sRegValue.empty ())
   {
      if (sRegValue.find (L":\\") != std::wstring::npos  ||  sRegValue.rfind (L"\\\\", 0) == 0)
         sResult = sRegValue;
      else
      {
         wchar_t szBuffer[MAX_PATH] = {};

         if (bUserFont)
         {
            DWORD nLength = GetEnvironmentVariableW (L"LOCALAPPDATA", szBuffer, MAX_PATH);

            if (nLength > 0  &&  nLength < MAX_PATH)
               sResult = std::wstring (szBuffer) + L"\\Microsoft\\Windows\\Fonts\\" + sRegValue;
         }
         else
         {
            UINT nLength = GetWindowsDirectoryW (szBuffer, MAX_PATH);

            if (nLength > 0  &&  nLength < MAX_PATH)
               sResult = std::wstring (szBuffer) + L"\\Fonts\\" + sRegValue;
         }
      }

      if (sResult.empty ())
         sResult = sRegValue;
   }

   return sResult;
}

static std::wstring ResolveFontFilePath (const std::wstring& sFamilyName)
{
   std::wstring sResult;
   const wchar_t* pFontsKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";

   auto fnTryRegistry = [&] (HKEY hRoot, bool bUserFont) -> bool
   {
      bool bFound = false;

      for (const auto& Entry : ReadFontRegistryValues (hRoot, pFontsKey))
      {
         if (StartsWithI (Entry.first, sFamilyName)  &&  !IsVariantFaceName (Entry.first))
         {
            sResult = MakeAbsoluteFontPath (Entry.second, bUserFont);
            bFound  = true;
            break;
         }
      }

      if (!bFound)
      {
         for (const auto& Entry : ReadFontRegistryValues (hRoot, pFontsKey))
         {
            if (StartsWithI (Entry.first, sFamilyName))
            {
               sResult = MakeAbsoluteFontPath (Entry.second, bUserFont);
               bFound  = true;
               break;
            }
         }
      }

      return bFound;
   };

   if (!fnTryRegistry (HKEY_CURRENT_USER, true))
      fnTryRegistry (HKEY_LOCAL_MACHINE, false);

   return sResult;
}

//----------------------------------------------

int WINUTILS::ScaleDpi (HWND hWnd, int nValue)
{
   UINT uDpi = GetDpiForWindow (hWnd);

   return MulDiv (nValue, uDpi, USER_DEFAULT_SCREEN_DPI);
}

int WINUTILS::ScaleDpiEx (int nValue, UINT uDpi)
{
   return MulDiv (nValue, uDpi, USER_DEFAULT_SCREEN_DPI);
}

std::string WINUTILS::DefaultFontFamily (HWND hWnd)
{
   std::string sResult;
   LOGFONTW    LogFont   = {};
   UINT        uDpi      = USER_DEFAULT_SCREEN_DPI;

   if (hWnd)
      uDpi = GetDpiForWindow (hWnd);
   else
      uDpi = GetDpiForSystem ();

   if (QueryMessageLogFont (&LogFont, uDpi))
      sResult = WideToUtf8 (LogFont.lfFaceName);

   if (sResult.empty ())
   {
      if (SystemParametersInfoForDpi (SPI_GETICONTITLELOGFONT, sizeof (LogFont), &LogFont, false, uDpi) != FALSE)
         sResult = WideToUtf8 (LogFont.lfFaceName);
   }

   if (sResult.empty ())
      sResult = "Segoe UI";

   return sResult;
}

std::string WINUTILS::FontFilePath (const std::string& sFontFamily)
{
   std::string sResult;

   if (!sFontFamily.empty ())
      sResult = WideToUtf8 (ResolveFontFilePath (Utf8ToWide (sFontFamily)).c_str ());

   return sResult;
}

std::string WINUTILS::FontFilename (const std::string& sFontFamily)
{
   std::string sResult;

   std::string sPath = FontFilePath (sFontFamily);

   if (!sPath.empty ())
      sResult = std::filesystem::path (sPath).filename ().string ();

   return sResult;
}

void WINUTILS::SetMenuItemState (HMENU hMenu, UINT uItem, bool enabled)
{
   MENUITEMINFO mi = {};

   mi.cbSize = sizeof (MENUITEMINFO);
   mi.fMask = MIIM_STATE;

   mi.fState = enabled ? MF_ENABLED : MF_DISABLED;
   SetMenuItemInfo (hMenu, uItem, false, &mi);
}

void WINUTILS::RectCenterBox (RECT &rcBox, const RECT &rcBound)
{
   int to_width = rcBox.right - rcBox.left;
   int to_height = rcBox.bottom - rcBox.top;
   int outer_width = rcBound.right - rcBound.left;
   int outer_height = rcBound.bottom - rcBound.top;

   int padding_x = (outer_width - to_width) / 2;
   int padding_y = (outer_height - to_height) / 2;

   rcBox.left = rcBound.left + padding_x;
   rcBox.top = rcBound.top + padding_y;
   rcBox.right = rcBox.left + to_width;
   rcBox.bottom = rcBox.top + to_height;
}

