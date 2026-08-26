// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "pch.h"

#include "updater/UpdaterGeneric.h"
#include "shell/AppFrame.h"

#if defined(__APPLE__) && !defined(RUBIDIUM_IOS)
#include <mach-o/dyld.h>
#elif !defined(_WIN32) && !defined(__ANDROID__) && !defined(RUBIDIUM_IOS)
#include <unistd.h>
#endif

namespace fs = std::filesystem;

using namespace RUBIDIUM;

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
   if (sExe.empty ())
      return;

   std::string sCmd = "\"" + sExe + "\" --check " + sCurrentVersion;
   if (bForce)
      sCmd += " --force";

   // Run synchronously (no trailing '&'). RunCheck executes on the updater
   // worker thread, and the poller inspects Updates.json immediately after
   // this returns -- so we must wait for RubidiumSetup to finish writing the
   // staged entry, mirroring the Win32 WaitForSingleObject behavior.
   std::system (sCmd.c_str ());
}

std::string UPDATER_NATIVE::SetupExePath () const
{
   std::string sResult;

#if defined(__APPLE__) && !defined(RUBIDIUM_IOS)
   char szPath[4096] = {};
   uint32_t nSize = sizeof (szPath);
   if (_NSGetExecutablePath (szPath, &nSize) == 0)
   {
      std::string sExeDir = fs::path (szPath).parent_path ().string ();
      std::string sCandidate = sExeDir + "/RubidiumSetup";
      if (fs::exists (sCandidate))
         sResult = sCandidate;
   }
#elif !defined(_WIN32) && !defined(__ANDROID__) && !defined(RUBIDIUM_IOS)
   char szPath[4096] = {};
   ssize_t nLen = readlink ("/proc/self/exe", szPath, sizeof (szPath) - 1);

   if (nLen > 0)
   {
      szPath[nLen] = '\0';
      std::string sCandidate = fs::path (szPath).parent_path ().string () + "/RubidiumSetup";
      if (fs::exists (sCandidate))
         sResult = sCandidate;
   }
#else
   sResult = "./RubidiumSetup";
#endif

   return sResult;
}
