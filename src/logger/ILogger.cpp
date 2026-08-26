// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "ILogger.h"

ILOGGER_NATIVE::ILOGGER_NATIVE (bool bUseStdOut) :
   m_bUseStdOut (bUseStdOut)
{

}

void ILOGGER_NATIVE::onMessage (std::string& sLine)
{
   if (m_bUseStdOut)
      onMessage_StdOut (sLine);
   else onMessage_File (sLine);
}

void ILOGGER_NATIVE::onMessage_File (std::string& sLine)
{
   std::ofstream file;
   file.open ("log.txt", std::ios::app); // std::ios::app ensures append mode

   if (file)
   {
      // Append text with newline
      file << sLine << std::endl;

      file.close ();

   }
}

void ILOGGER_NATIVE::onMessage_StdOut (std::string& sLine)
{
   std::printf ("%s\n", sLine.c_str ());

#ifndef RUBIDIUM_PLATFORM_WINDOWS
   fflush (stdout);
#endif
}

