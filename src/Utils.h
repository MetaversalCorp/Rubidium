// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_UTILS_H
#define RUBIDIUM_UTILS_H

namespace RUBIDIUM
{
   class UTILS
   {
   public:
      static std::string   ToLowerEx (std::string s);
      static void          ToLower   (std::string& s);
      static void          ToLower   (const std::string& sInput, std::string& sOutput);

      static std::wstring  ToLowerEx (std::wstring sText);
      static void          ToLower   (const std::wstring& sInput, std::wstring& sOutput);

      static std::string   Escape (const std::string& sText, bool bQuote = true);
   };
}

#endif // RUBIDIUM_UTILS_H
