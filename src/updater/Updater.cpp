// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "pch.h"

#include "updater/Updater.h"
#ifdef _WIN32
#include "UpdaterWnd.h"
#else
#include "UpdaterGeneric.h"
#endif

#include "resource.h"

#include "version.h"

#include <curl/curl.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

// Sneeze embeds Mozilla's CA bundle for curl on platforms without an OS trust store.
#if !defined(_WIN32) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
namespace SNEEZE
{
   extern const char* const         g_szCaCertPem;
   extern const unsigned long       g_nCaCertPemLen;
}
#endif

#ifndef RUBIDIUM_CDN_URL
#define RUBIDIUM_CDN_URL "https://cdn.rp1.com/rubidium/"
#endif

#ifndef RUBIDIUM_PLATFORM
#define RUBIDIUM_PLATFORM "windows-x64"
#endif

namespace fs = std::filesystem;

using namespace RUBIDIUM;

static size_t ManifestWriteCallback (char* pData, size_t nSize, size_t nItems, void* pUser)
{
   std::string* psOut = static_cast<std::string*> (pUser);
   size_t       nTotal = nSize * nItems;

   psOut->append (pData, nTotal);

   return nTotal;
}

static void Split (const std::string& s, char delimiter, std::vector<std::string>& aResult)
{
   size_t start = 0;

   aResult.clear ();
   while (true)
   {
      size_t pos = s.find (delimiter, start);
      aResult.push_back (s.substr (start, pos - start));

      if (pos == std::string::npos)
         break;

      start = pos + 1;
   }
}


/*******************************************************************************************************************************
**                                                     CLASS (UpdaterCheck)                                                   **
*******************************************************************************************************************************/

namespace RUBIDIUM
{
   class UPDATERCHECK
   {
   public:
      UPDATERCHECK (UPDATER* pUpdater, bool bForce, IUPDATER* pNotify, bool bNotifyNoUpdate) :
         m_pUpdater (pUpdater),
         m_bForce (bForce),
         m_Result (CHECK_IDLE),
         m_bShutdown (false),
         m_bNotifyNoUpdate (bNotifyNoUpdate),
         m_pNotify (pNotify)
      {
      }

      void Shutdown ()
      {
         std::lock_guard<std::mutex> guard (m_mutex);
         m_bShutdown = true;
         m_condVar.notify_all ();
      }

      void ThreadLoop ()
      {
         {
            std::unique_lock<std::mutex> mlock (m_mutex);
            while (!Control ())
            {
               m_condVar.wait_for (mlock, std::chrono::seconds (2));
            }
         }

         m_pUpdater->OnCheckComplete ();
      }

      bool Control ()
      {
         bool bExit = false;
         int64_t tmNow = 0;

         if (m_bShutdown == false)
         {
            if (m_Result == CHECK_IDLE)
            {
               tmNow = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now ().time_since_epoch ()).count ();

               if (m_bForce)
               {
                  nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON (); // Lock

                  if (jSettings.contains ("updater"))
                     jSettings["updater"]["skipped_version"] = "";

                  // Release
               }

               m_pUpdater->RunCheck (RUBIDIUM_VERSION, m_bForce);

               m_Result = CHECK_PENDING;
            }

            if (m_Result == CHECK_PENDING)
            {
               m_Result = m_pUpdater->PollCheckResult (tmNow);

               if (m_Result == CHECK_UPDATE_AVAILABLE)
               {
                  m_pNotify->onUpdaterAvailable (true);
                  m_bShutdown = true;
               }
               else if (m_Result == CHECK_NO_UPDATE)
               {
                  if (m_bNotifyNoUpdate)
                     m_pNotify->onUpdaterAvailable (false);
                  m_bShutdown = true;
               }
            }
         }

         return m_bShutdown;
      }

   private:
      std::mutex                 m_mutex;
      std::condition_variable    m_condVar;
      bool                       m_bShutdown;

      IUPDATER*                  m_pNotify;
      CHECK_RESULT               m_Result;
      UPDATER*                   m_pUpdater;
      bool                       m_bForce;
      bool                       m_bNotifyNoUpdate;
   };
}

/*******************************************************************************************************************************
**                                                     CLASS (Updater)                                                        **
*******************************************************************************************************************************/

UPDATER::UPDATER (IUPDATER* pNotify) : 
   m_bWaitingForCheck (false),
   m_pNotify (pNotify),
   m_pUpdaterCheck (nullptr)
{
}

UPDATER::~UPDATER ()
{
   if (m_pUpdaterCheck != nullptr)
      m_pUpdaterCheck->Shutdown ();

   if (m_threadCheck.joinable ())
      m_threadCheck.join ();

   delete m_pUpdaterCheck;
   m_pUpdaterCheck = nullptr;
}

bool UPDATER::Initialize ()
{
   bool bResult = true;

   if (CheckForStagedUpdate (RUBIDIUM_VERSION))
   {
      if (!IsVersionSkipped ())
      {
         bResult = Prompt ();
      }
   }
   else
   {
      ClearStaleSkippedVersion (RUBIDIUM_VERSION);
      SpawnBackgroundCheck (false, false);
   }

   return bResult;
}

bool UPDATER::Prompt ()
{
   bool bResult = true;
   IUPDATER::ePROMPT eResult;

   std::string sMsg = "Rubidium " + StagedVersion () + " is available.\n\n";
   std::string sNotes = StagedNotes ();
   std::string sTitle = "Rubidium Updater";

   if (!sNotes.empty ())
      sMsg += sNotes + "\n\n";
   sMsg += "Would you like to install it?";

   eResult = m_pNotify->onUpdaterPrompt (sMsg, sTitle);

   if (eResult == IUPDATER::kPROMPT_YES)
   {
      SpawnApply ();
      bResult = false;
   }
   else if (eResult == IUPDATER::kPROMPT_NO)
   {
      SkipVersion ();
   }

   return bResult;
}

bool UPDATER::ReadJSON ()
{
   bool bResult = false;
   std::string sPath = UpdatesJsonPath ();
   std::ifstream fIn (sPath);

   if (fIn.is_open ())
   {
      try
      {
         m_jUpdates = nlohmann::json::parse (fIn);

         bResult = true;
      }
      catch (const nlohmann::json::exception&)
      {
         bResult = false;
      }

      fIn.close ();
   }

   return bResult;
}

void UPDATER::WriteJSON ()
{
   std::ofstream fOut (UpdatesJsonPath ());
   if (fOut.is_open ())
      fOut << m_jUpdates.dump (3) << "\n";
}

// ---------------------------------------------------------------------------

bool UPDATER::CheckForStagedUpdate (const std::string& sCurrentVersion)
{
   bool bResult = false;

   m_sStagedVersion.clear ();
   m_sStagedNotes.clear ();
   m_sStagedInstaller.clear ();

   if (ReadJSON ())
   {
      if (m_jUpdates.contains ("staged"))
      {
         std::string sVersion = m_jUpdates["staged"].value ("version", "");

         if (!sVersion.empty ()  &&  CompareVersions (sVersion, sCurrentVersion) <= 0)
         {
            std::string sInstaller = m_jUpdates["staged"].value ("installer", "");
            if (!sInstaller.empty ())
            {
               std::error_code ec;
               fs::remove (sInstaller, ec);
            }
            m_jUpdates.erase ("staged");

            WriteJSON ();
         }
         else
         {
            m_sStagedVersion   = sVersion;
            m_sStagedNotes     = m_jUpdates["staged"].value ("notes", "");
            m_sStagedInstaller = m_jUpdates["staged"].value ("installer", "");

            bResult = !m_sStagedVersion.empty () && !m_sStagedInstaller.empty () && fs::exists (m_sStagedInstaller);
         }
      }
   }

   return bResult;
}

bool UPDATER::IsVersionSkipped () const
{
   bool bSkipped = false;
   nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();

   if (jSettings.contains ("updater"))
   {
      std::string sSkipped = jSettings["updater"].value ("skipped_version", "");
      bSkipped = (!sSkipped.empty ()  &&  sSkipped == m_sStagedVersion);
   }

   return bSkipped;
}

std::string UPDATER::StagedVersion () const
{
   return m_sStagedVersion;
}

std::string UPDATER::StagedNotes () const
{
   return m_sStagedNotes;
}

// ---------------------------------------------------------------------------

void UPDATER::MarkReleaseNotesSeen ()
{
   nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();

   if (!jSettings.contains ("release_notes"))
      jSettings["release_notes"] = nlohmann::json::object ();

   jSettings["release_notes"]["last_seen_version"] = RUBIDIUM_VERSION;
}

bool UPDATER::ParseReleaseNotesFromManifest (const std::string& sJson, const std::string& sVersion, std::string& sNotes)
{
   bool bResult = false;

   try
   {
      nlohmann::json jManifest = nlohmann::json::parse (sJson);

      if (jManifest.contains ("releases"))
      {
         const auto& jPlatform = jManifest["releases"];

         if (jPlatform.contains (RUBIDIUM_PLATFORM)  &&
             jPlatform[RUBIDIUM_PLATFORM].contains (sVersion))
         {
            sNotes   = jPlatform[RUBIDIUM_PLATFORM][sVersion].value ("notes", "");
            bResult  = true;
         }
      }
   }
   catch (const nlohmann::json::exception&)
   {
   }

   return bResult;
}

bool UPDATER::FetchManifestJson (std::string& sJson)
{
   bool        bResult = false;
   std::string sUrl    = std::string (RUBIDIUM_CDN_URL) + "manifest.json";
   CURL*       pCurl   = curl_easy_init ();

   if (pCurl)
   {
      curl_easy_setopt (pCurl, CURLOPT_URL, sUrl.c_str ());
      curl_easy_setopt (pCurl, CURLOPT_WRITEFUNCTION, ManifestWriteCallback);
      curl_easy_setopt (pCurl, CURLOPT_WRITEDATA, &sJson);
      curl_easy_setopt (pCurl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt (pCurl, CURLOPT_TIMEOUT, 30L);

      // Win32 Schannel and iOS Secure Transport use the OS trust store. macOS
      // desktop and Linux curl link BoringSSL and need Sneeze's embedded CA
      // bundle (same as NETWORK Fetch).
#if !defined(_WIN32) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
      curl_blob caBlob;

      caBlob.data  = const_cast<void*> (static_cast<const void*> (SNEEZE::g_szCaCertPem));
      caBlob.len   = SNEEZE::g_nCaCertPemLen;
      caBlob.flags = CURL_BLOB_NOCOPY;
      curl_easy_setopt (pCurl, CURLOPT_CAINFO_BLOB, &caBlob);
#endif

      if (curl_easy_perform (pCurl) == CURLE_OK)
      {
         long nHttpCode = 0;

         curl_easy_getinfo (pCurl, CURLINFO_RESPONSE_CODE, &nHttpCode);

         if (nHttpCode == 200)
            bResult = true;
      }

      curl_easy_cleanup (pCurl);
   }

   return bResult;
}

void UPDATER::SaveReleaseNotes (const std::string& sVersion, const std::string& sNotes)
{
   nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();

   if (!jSettings.contains ("release_notes"))
      jSettings["release_notes"] = nlohmann::json::object ();

   jSettings["release_notes"]["version"] = sVersion;
   jSettings["release_notes"]["notes"]   = sNotes;

   APPNATIVE::GetInstance ()->SaveSettings ();
}

bool UPDATER::SavedReleaseNotes (std::string& sVersion, std::string& sNotes) const
{
   bool bResult = false;

   nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();

   if (jSettings.contains ("release_notes"))
   {
      sVersion = jSettings["release_notes"].value ("version", "");
      sNotes   = jSettings["release_notes"].value ("notes", "");

      bResult = !sVersion.empty ()  &&  !sNotes.empty ();
   }

   return bResult;
}

bool UPDATER::EnsureReleaseNotesCached ()
{
   bool        bResult = false;
   std::string sSavedVersion;
   std::string sSavedNotes;

   if (SavedReleaseNotes (sSavedVersion, sSavedNotes)  &&
       sSavedVersion == std::string (RUBIDIUM_VERSION))
   {
      bResult = true;
   }
   else
   {
      std::string sManifestJson;
      std::string sNotes;

      if (FetchManifestJson (sManifestJson)  &&
          ParseReleaseNotesFromManifest (sManifestJson, RUBIDIUM_VERSION, sNotes))
      {
         SaveReleaseNotes (RUBIDIUM_VERSION, sNotes);
         bResult = !sNotes.empty ();
      }
   }

   return bResult;
}

bool UPDATER::PendingReleaseNotes (std::string& sVersion, std::string& sNotes)
{
   bool          bResult = false;
   std::ifstream fIn (UpdatesJsonPath ());

   if (fIn.is_open ())
   {
      try
      {
         nlohmann::json jUpdates = nlohmann::json::parse (fIn);

         if (jUpdates.contains ("release_notes"))
         {
            std::string sPending = jUpdates["release_notes"].value ("version", "");

            if (!sPending.empty ()  &&  sPending == std::string (RUBIDIUM_VERSION))
            {
               sVersion = sPending;
               sNotes   = jUpdates["release_notes"].value ("notes", "");
               bResult  = true;
            }
         }
      }
      catch (const nlohmann::json::exception&)
      {
      }
   }

   if (!bResult)
   {
      nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();
      std::string     sLastSeen;

      if (jSettings.contains ("release_notes"))
         sLastSeen = jSettings["release_notes"].value ("last_seen_version", "");

      if (sLastSeen != std::string (RUBIDIUM_VERSION))
      {
         std::string sManifestJson;

         if (FetchManifestJson (sManifestJson))
         {
            if (ParseReleaseNotesFromManifest (sManifestJson, RUBIDIUM_VERSION, sNotes))
            {
               sVersion = RUBIDIUM_VERSION;
               bResult  = true;
            }
            else
            {
               // Version not listed in the manifest -- nothing to show; mark seen
               // so we do not fetch again on every launch.
               MarkReleaseNotesSeen ();
            }
         }
         else
         {
            LOGGER* pLogger = APPNATIVE::GetInstance ()->Logger ();

            if (pLogger)
            {
               pLogger->Log (LOGGER::kLOGLEVEL_Warning, "Updater",
                  std::string ("Release notes manifest fetch failed for ") + RUBIDIUM_VERSION);
            }
         }
      }
   }

   if (bResult)
      SaveReleaseNotes (sVersion, sNotes);

   return bResult;
}

void UPDATER::ClearReleaseNotes ()
{
   std::ifstream fIn (UpdatesJsonPath ());

   if (fIn.is_open ())
   {
      nlohmann::json jUpdates;

      try
      {
         jUpdates = nlohmann::json::parse (fIn);
      }
      catch (const nlohmann::json::exception&)
      {
         jUpdates = nlohmann::json::object ();
      }

      fIn.close ();

      if (jUpdates.contains ("release_notes"))
      {
         jUpdates.erase ("release_notes");

         std::ofstream fOut (UpdatesJsonPath ());
         if (fOut.is_open ())
            fOut << jUpdates.dump (3) << "\n";
      }
   }

   MarkReleaseNotesSeen ();
}

// ---------------------------------------------------------------------------

CHECK_RESULT UPDATER::PollCheckResult (int64_t tmCheckSpawned)
{
   CHECK_RESULT nResult = CHECK_PENDING;

   std::string sPath = UpdatesJsonPath ();
   std::ifstream fIn (sPath);
   if (fIn.is_open ())
   {
      try
      {
         nlohmann::json jUpdates = nlohmann::json::parse (fIn);
         fIn.close ();

         int64_t tmLastCheck = jUpdates.value ("last_check", (int64_t)0);
         if (tmLastCheck >= tmCheckSpawned)
         {
            if (jUpdates.contains ("staged"))
            {
               m_sStagedVersion   = jUpdates["staged"].value ("version", "");
               m_sStagedNotes     = jUpdates["staged"].value ("notes", "");
               m_sStagedInstaller = jUpdates["staged"].value ("installer", "");
               nResult = CHECK_UPDATE_AVAILABLE;
            }
            else
            {
               nResult = CHECK_NO_UPDATE;
            }
         }
      }
      catch (const nlohmann::json::exception&)
      {
      }
   }

   return nResult;
}

void UPDATER::SpawnApply ()
{
   std::string sExe = SetupExePath ();
   if (sExe.empty ())
      return;

#ifdef _WIN32
   ShellExecuteA (nullptr, "open", sExe.c_str (), "--apply", nullptr, SW_SHOWNORMAL);
#elif !defined(RUBIDIUM_IOS)
   std::string sCmd = "\"" + sExe + "\" --apply &";
   std::system (sCmd.c_str ());
#endif
}

void UPDATER::SkipVersion ()
{
   nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();

   if (!jSettings.contains ("updater"))
      jSettings["updater"] = nlohmann::json::object ();

   jSettings["updater"]["skipped_version"] = m_sStagedVersion;

   CleanupStaged ();
}

void UPDATER::ClearStaleSkippedVersion (const std::string& sCurrentVersion)
{
   nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();

   if (jSettings.contains ("updater"))
   {
      std::string sSkipped = jSettings["updater"].value ("skipped_version", "");
      if (!sSkipped.empty ()  &&  CompareVersions (sSkipped, sCurrentVersion) <= 0)
      {
         jSettings["updater"]["skipped_version"] = "";
      }
   }
}

void UPDATER::CleanupStaged ()
{
   if (!m_sStagedInstaller.empty ())
      fs::remove (m_sStagedInstaller);

   // Remove staged from Updates.json
   std::string sUpdatesPath = UpdatesJsonPath ();
   std::ifstream fIn (sUpdatesPath);
   if (fIn.is_open ())
   {
      nlohmann::json jUpdates;
      try
      {
         jUpdates = nlohmann::json::parse (fIn);
      }
      catch (const nlohmann::json::exception&)
      {
         jUpdates = nlohmann::json::object ();
      }
      fIn.close ();

      if (jUpdates.contains ("staged"))
      {
         jUpdates.erase ("staged");
         std::ofstream fOut (sUpdatesPath);
         if (fOut.is_open ())
            fOut << jUpdates.dump (3) << "\n";
      }
   }

   m_sStagedVersion.clear ();
   m_sStagedNotes.clear ();
   m_sStagedInstaller.clear ();
}

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

std::string UPDATER::UpdatesDir () const
{
   return RUBIDIUM::APPNATIVE::GetInstance ()->sHomePath () + "/Updates";
}

std::string UPDATER::UpdatesJsonPath () const
{
   return UpdatesDir () + "/Updates.json";
}

int UPDATER::CompareVersions (const std::string& sA, const std::string& sB)
{
   int nResult = 0;
   std::vector<std::string> aA, aB;

   Split (sA, '.', aA);
   Split (sB, '.', aB);

   if (aA.size () == 3 && aB.size () == 3)
   {
      nResult = std::stoi (aA[0]) - std::stoi (aB[0]);

      if (nResult == 0)
      {
         nResult = std::stoi (aA[1]) - std::stoi (aB[1]);

         if (nResult == 0)
         {
            nResult = std::stoi (aA[2]) - std::stoi (aB[2]);
         }
      }
   }

   return nResult;
}

bool UPDATER::SpawnBackgroundCheck (bool bForce, bool bNotifyNoUpdate)
{
   bool bResult = false;

   if (m_pUpdaterCheck == nullptr)
   {
      if (m_threadCheck.joinable ())
         m_threadCheck.join ();

      m_pUpdaterCheck = new UPDATERCHECK (this, bForce, m_pNotify, bNotifyNoUpdate);
      m_threadCheck = std::thread (&UPDATERCHECK::ThreadLoop, m_pUpdaterCheck);

      bResult = true;
   }

   return bResult;
}

void UPDATER::OnCheckComplete ()
{
   delete m_pUpdaterCheck;
   m_pUpdaterCheck = nullptr;
   m_threadCheck.detach ();
}
