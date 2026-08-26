// Copyright 2026 Metaversal Corporation. All rights reserved.
//

using namespace RUBIDIUM;

// ---------------------------------------------------------------------------
// CLASS: UTILS
// ---------------------------------------------------------------------------

std::string UTILS::ToLowerEx (std::string s) 
{
   std::transform (s.begin (), s.end (), s.begin (), 
      [](unsigned char c) 
      { 
         return static_cast<char>(std::tolower (c));
      });

   return s;
}

void UTILS::ToLower (std::string& s)
{
   std::transform (s.begin (), s.end (), s.begin (),
      [](unsigned char c)
      {
         return static_cast<char>(std::tolower (c));
      });
}

void UTILS::ToLower (const std::string& sInput, std::string& sOutput)
{
   sOutput.resize (sInput.size ());

   std::transform (sInput.begin (), sInput.end (), sOutput.begin (),
      [](unsigned char c)
      {
         return static_cast<char>(std::tolower (c));
      });
}

std::wstring UTILS::ToLowerEx (std::wstring sText)
{
   std::transform (sText.begin (), sText.end (), sText.begin (),
      [](wchar_t cChar) 
      { 
         return static_cast<wchar_t> (towlower (cChar)); 
      });

   return sText;
}

void UTILS::ToLower (const std::wstring& sInput, std::wstring& sOutput)
{
   sOutput.resize (sInput.size ());

   std::transform (sInput.begin (), sInput.end (), sOutput.begin (),
      [](wchar_t cChar)
      {
         return static_cast<wchar_t> (towlower (cChar));
      });
}

std::string UTILS::Escape (const std::string& sText, bool bQuote)
{
   std::string sResult;
   sResult.reserve (sText.size ());

   // RmlUi treats "{{" / "}}" in text and attribute values as data-binding
   // expressions and decodes HTML entities *before* scanning for them, so an
   // entity escape can't hide a literal brace pair. Arbitrary content (file
   // previews -- a .glb starts with the "glTF" magic and embeds JSON full of
   // "}}", raw payloads, URLs) would otherwise trip "Closing double curly
   // brackets encountered outside an expression" and fail to render. Break any
   // run of identical braces with a zero-width space: invisible in the rendered
   // text, but enough that RmlUi never sees a "{{" or "}}" token.
   static const char* s_sZeroWidth = "\xE2\x80\x8B";

   char cPrev = 0;

   for (char c : sText)
   {
      if ((c == '{' || c == '}')  &&  c == cPrev)
         sResult += s_sZeroWidth;

      switch (c)
      {
      case '&':  sResult += "&amp;";  break;
      case '<':  sResult += "&lt;";   break;
      case '>':  sResult += "&gt;";   break;
      case '"':  
         if (bQuote)
         {
            sResult += "&quot;";
            break;
         }
         [[fallthrough]];

      default:   
         sResult += c;        
         break;
      }

      cPrev = c;
   }

   return sResult;
}
