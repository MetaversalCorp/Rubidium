// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_UPDATER_UPDATER_H
#define RUBIDIUM_UPDATER_UPDATER_H

#include <string>
#include <thread>

namespace RUBIDIUM
{
   enum CHECK_RESULT
   {
      CHECK_IDLE,
      CHECK_PENDING,
      CHECK_NO_UPDATE,
      CHECK_UPDATE_AVAILABLE
   };

   class IUPDATER
   {
   public:
      enum ePROMPT
      {
         kPROMPT_YES,
         kPROMPT_NO,
         kPROMPT_CANCEL
      };

   public:
      virtual void    onUpdaterAvailable (bool bAvailable)                          = 0;
      virtual ePROMPT onUpdaterPrompt    (std::string& sMsg, std::string& sTitle)   = 0;
   };

   class APPFRAME;
   class UPDATERCHECK;
   class UPDATER
   {
   public:
      UPDATER (IUPDATER* pNotify);
      virtual ~UPDATER ();

      bool CheckForStagedUpdate (const std::string& sCurrentVersion);
      bool IsVersionSkipped () const;

      std::string StagedVersion () const;
      std::string StagedNotes () const;

      // Release notes: (1) Updates.json handoff from RubidiumSetup (Windows
      // install/apply, Linux apply), or (2) manifest fetch when the running
      // version has not yet been marked seen in settings (Mac/Linux tarball/DMG
      // first launch, manual installs). ClearReleaseNotes () clears the handoff
      // and marks the version seen so notes show once per version.
      bool PendingReleaseNotes (std::string& sVersion, std::string& sNotes);
      void ClearReleaseNotes ();

      // Persisted copy in settings.json for the ellipsis menu. Saved whenever
      // notes are obtained; EnsureReleaseNotesCached () fetches from the CDN
      // manifest when the saved copy is missing or stale.
      void SaveReleaseNotes (const std::string& sVersion, const std::string& sNotes);
      bool SavedReleaseNotes (std::string& sVersion, std::string& sNotes) const;
      bool EnsureReleaseNotesCached ();

      CHECK_RESULT PollCheckResult (int64_t tmCheckSpawned);
      void SpawnApply ();
      void SkipVersion ();
      void ClearStaleSkippedVersion (const std::string& sCurrentVersion);
      void CleanupStaged ();

      bool SpawnBackgroundCheck (bool bForce, bool bNotifyNoUpdate);
      void OnCheckComplete ();

      bool Prompt ();

      //===============================

      virtual bool Initialize ();

      virtual void RunCheck (const std::string& sCurrentVersion, bool bForce) = 0;

   protected:
      virtual std::string SetupExePath () const = 0;

   private:
      UPDATERCHECK*  m_pUpdaterCheck;
      IUPDATER*      m_pNotify;
      std::thread    m_threadCheck;

      std::string    m_sStagedVersion;
      std::string    m_sStagedNotes;
      std::string    m_sStagedInstaller;
      bool           m_bWaitingForCheck;

      nlohmann::json m_jUpdates;

      bool ReadJSON ();
      void WriteJSON ();

      int CompareVersions (const std::string& sA, const std::string& sB);

      std::string UpdatesJsonPath () const;
      std::string UpdatesDir () const;

      void MarkReleaseNotesSeen ();
      bool FetchManifestJson (std::string& sJson);
      bool ParseReleaseNotesFromManifest (const std::string& sJson, const std::string& sVersion, std::string& sNotes);
   };
}

#endif // RUBIDIUM_UPDATER_UPDATER_H
