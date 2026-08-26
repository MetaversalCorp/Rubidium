// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "pch.h"

#include "release_notes/Markdown.h"

using namespace RUBIDIUM;

namespace
{
   std::string EscapeChar (char c)
   {
      std::string sOut;

      switch (c)
      {
      case '&':  sOut = "&amp;";              break;
      case '<':  sOut = "&lt;";               break;
      case '>':  sOut = "&gt;";               break;
      case '"':  sOut = "&quot;";             break;
      default:   sOut = std::string (1, c);   break;
      }

      return sOut;
   }

   std::string EscapeText (const std::string& sText)
   {
      std::string sOut;

      sOut.reserve (sText.size ());

      for (char c : sText)
         sOut += EscapeChar (c);

      return sOut;
   }

   bool StartsWith (const std::string& sText, const std::string& sPrefix)
   {
      return sText.rfind (sPrefix, 0) == 0;
   }

   std::string TrimRight (const std::string& sText)
   {
      size_t      nLast = sText.find_last_not_of (" \t\r\n");
      std::string sOut;

      if (nLast != std::string::npos)
         sOut = sText.substr (0, nLast + 1);

      return sOut;
   }

   std::string TrimLeft (const std::string& sText)
   {
      size_t      nFirst = sText.find_first_not_of (" \t");
      std::string sOut;

      if (nFirst != std::string::npos)
         sOut = sText.substr (nFirst);

      return sOut;
   }

   // A horizontal rule is a line consisting solely of three or more of the same
   // marker character (-, * or _), optionally spaced.
   bool IsHorizontalRule (const std::string& sLine)
   {
      bool   bRule  = false;
      char   cFirst = 0;
      size_t nCount = 0;
      bool   bValid = !sLine.empty ();

      for (char c : sLine)
      {
         if (c == ' ' || c == '\t')
            continue;

         if (c == '-' || c == '*' || c == '_')
         {
            if (cFirst == 0)
               cFirst = c;

            if (c == cFirst)
               nCount++;
            else
               bValid = false;
         }
         else
         {
            bValid = false;
         }
      }

      if (bValid  &&  nCount >= 3)
         bRule = true;

      return bRule;
   }

   // Convert one line of inline Markdown to RML. Recognizes `code`, **bold**,
   // *italic* / _italic_ and [text](url) links; all other characters are
   // RML-escaped literals.
   std::string InlineToRml (const std::string& sLine)
   {
      std::string sOut;
      size_t      i = 0;
      size_t      n = sLine.size ();

      while (i < n)
      {
         char c = sLine[i];

         if (c == '`')
         {
            size_t nEnd = sLine.find ('`', i + 1);

            if (nEnd != std::string::npos)
            {
               sOut += "<span class=\"code\">";
               sOut += EscapeText (sLine.substr (i + 1, nEnd - i - 1));
               sOut += "</span>";
               i = nEnd + 1;
            }
            else
            {
               sOut += EscapeChar (c);
               i++;
            }
         }
         else if (c == '*'  &&  i + 1 < n  &&  sLine[i + 1] == '*')
         {
            size_t nEnd = sLine.find ("**", i + 2);

            if (nEnd != std::string::npos  &&  nEnd > i + 2)
            {
               sOut += "<strong>";
               sOut += InlineToRml (sLine.substr (i + 2, nEnd - i - 2));
               sOut += "</strong>";
               i = nEnd + 2;
            }
            else
            {
               sOut += EscapeChar (c);
               i++;
            }
         }
         else if (c == '*'  ||  c == '_')
         {
            size_t nEnd = sLine.find (c, i + 1);

            if (nEnd != std::string::npos  &&  nEnd > i + 1)
            {
               sOut += "<em>";
               sOut += InlineToRml (sLine.substr (i + 1, nEnd - i - 1));
               sOut += "</em>";
               i = nEnd + 1;
            }
            else
            {
               sOut += EscapeChar (c);
               i++;
            }
         }
         else if (c == '[')
         {
            size_t nTextEnd = sLine.find (']', i + 1);
            bool   bLink    = false;

            if (nTextEnd != std::string::npos  &&  nTextEnd + 1 < n  &&  sLine[nTextEnd + 1] == '(')
            {
               size_t nUrlEnd = sLine.find (')', nTextEnd + 2);

               if (nUrlEnd != std::string::npos)
               {
                  std::string sText = sLine.substr (i + 1, nTextEnd - i - 1);
                  std::string sUrl  = sLine.substr (nTextEnd + 2, nUrlEnd - nTextEnd - 2);

                  sOut += "<a class=\"link\" href=\"";
                  sOut += EscapeText (sUrl);
                  sOut += "\">";
                  sOut += InlineToRml (sText);
                  sOut += "</a>";
                  i = nUrlEnd + 1;
                  bLink = true;
               }
            }

            if (!bLink)
            {
               sOut += EscapeChar (c);
               i++;
            }
         }
         else
         {
            sOut += EscapeChar (c);
            i++;
         }
      }

      return sOut;
   }
}

std::string RUBIDIUM::Markdown_ToRml (const std::string& sMarkdown)
{
   std::string sOut;
   std::string sPara;
   size_t      nStart = 0;

   // Flush the accumulated paragraph buffer as a <p> block.
   auto FlushParagraph = [&] ()
   {
      if (!sPara.empty ())
      {
         sOut += "<p>";
         sOut += InlineToRml (sPara);
         sOut += "</p>";
         sPara.clear ();
      }
   };

   while (nStart <= sMarkdown.size ())
   {
      size_t      nNewline = sMarkdown.find ('\n', nStart);
      size_t      nLength  = (nNewline == std::string::npos) ? std::string::npos : nNewline - nStart;
      std::string sRaw     = TrimRight (sMarkdown.substr (nStart, nLength));
      std::string sLine    = TrimLeft (sRaw);

      if (sLine.empty ())
      {
         FlushParagraph ();
      }
      else if (StartsWith (sLine, "### "))
      {
         FlushParagraph ();
         sOut += "<div class=\"h3\">" + InlineToRml (sLine.substr (4)) + "</div>";
      }
      else if (StartsWith (sLine, "## "))
      {
         FlushParagraph ();
         sOut += "<div class=\"h2\">" + InlineToRml (sLine.substr (3)) + "</div>";
      }
      else if (StartsWith (sLine, "# "))
      {
         FlushParagraph ();
         sOut += "<div class=\"h1\">" + InlineToRml (sLine.substr (2)) + "</div>";
      }
      else if (IsHorizontalRule (sLine))
      {
         FlushParagraph ();
         sOut += "<div class=\"hr\"></div>";
      }
      else if (StartsWith (sLine, "- ")  ||  StartsWith (sLine, "* ")  ||  StartsWith (sLine, "+ "))
      {
         FlushParagraph ();
         sOut += "<div class=\"li\"><span class=\"bullet\">&#8226;</span><span class=\"litext\">"
              +  InlineToRml (sLine.substr (2))
              +  "</span></div>";
      }
      else
      {
         if (!sPara.empty ())
            sPara += " ";
         sPara += sLine;
      }

      if (nNewline == std::string::npos)
         break;

      nStart = nNewline + 1;
   }

   FlushParagraph ();

   return sOut;
}
