// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "pch.h"

#include "updater/UpdaterWnd.h"
#include "Brand.h"

using namespace RUBIDIUM;

namespace fs = std::filesystem;

UPDATER_NATIVE::UPDATER_NATIVE (IUPDATER* pNotify) :
   UPDATER (pNotify)
{
}

UPDATER_NATIVE::~UPDATER_NATIVE ()
{
}

void UPDATER_NATIVE::RunCheck (const std::string& sCurrentVersion, bool bForce)
{
   std::string sExe = SetupExePath ();
   std::string sCmd = "\"" + sExe + "\" --check " + sCurrentVersion;
   STARTUPINFOA pSi = {};
   PROCESS_INFORMATION pPi = {};

   if (bForce)
      sCmd += " --force";

   pSi.cb = sizeof (pSi);

   if (CreateProcessA (nullptr, const_cast<char*> (sCmd.c_str ()), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS,nullptr, nullptr, &pSi, &pPi) != FALSE)
   {
      WaitForSingleObject (pPi.hProcess, INFINITE);

      CloseHandle (pPi.hThread);
      CloseHandle (pPi.hProcess);
   }
}

std::string UPDATER_NATIVE::SetupExePath () const
{
   std::string sResult;
   char szPath[MAX_PATH] = {};

   if (GetModuleFileNameA (nullptr, szPath, MAX_PATH))
   {
      std::string sExeDir = fs::path (szPath).parent_path ().string ();
      std::string sCandidate = sExeDir + "\\" PRODUCT_SETUP_EXE;
      if (fs::exists (sCandidate))
         sResult = sCandidate;
   }

   return sResult;
}

