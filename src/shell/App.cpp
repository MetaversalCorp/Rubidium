// Copyright 2026 Metaversal Corporation. All rights reserved.
//

#include "release_notes/ReleaseNotesRml.h"
#include "version.h"
#include "Brand.h"

using namespace RUBIDIUM;

static const char* g_szLogLevels[LOGGER::kLOGLEVEL_COUNT] = { "trace", "info", "warning", "error", "off" };

static void GetRubidiumPath (std::string& sPath)
{
   sPath += "/" PRODUCT_APPDATA_DIR;
   std::filesystem::create_directories (sPath);
}

// ---------------------------------------------------------------------------
// CLASS: APP
// ---------------------------------------------------------------------------

APP::APP (std::string sHomePath, ILOGGER* pILogger) :
   m_sHomePath (std::move (sHomePath)),
   m_pILogger (pILogger),
   m_pLogger (new LOGGER (pILogger))
   , m_bApplyUpdate (false),
   m_pUpdater (nullptr),
   m_pReleaseNotes (nullptr)
{
   GetRubidiumPath (m_sHomePath);

   nlohmann::json jParsed;
   nlohmann::json jDefaults = {
      {
         "home", PRODUCT_HOME_URL
      },
      {
         "window", {
            {"x",         100},
            {"y",         100},
            {"width",     1280},
            {"height",    720},
            {"maximized", false}
         }
      },
      {  
         "updater", {
            {"skipped_version", ""}
         }
      },
      {
         "release_notes", {
            {"last_seen_version", ""},
            {"version",           ""},
            {"notes",             ""}
         }
      },
      {  
         "logger", {
            {"level", "info"}
         }
      },
      {
         "movement", {
            {"speed", kMovementSpeedDefault}
         }
      },
      {
         "inspector_rml", {
            {"width",     1280},
            {"height",    720},
            {"maximized", false}
         }
      },
      {
         "settings_window", {
            {"width",     1280},
            {"height",    720},
            {"maximized", false}
         }
      },
      {
         "developer", {
            { "console",      false },
            { "boundingbox",  false }
         }
      },
      {
         "url_history", nlohmann::json::array ()
      }
   };

   std::string sSettingsPath = m_sHomePath + "/settings.json";
   std::ifstream fIn (sSettingsPath);

   if (fIn.is_open ())
   {
      try
      {
         jParsed = nlohmann::json::parse (fIn);
      }
      catch (const nlohmann::json::exception&)
      {
      }
      fIn.close ();
   }

   // Start from defaults so every key the runtime reads exists, then overlay
   // whatever the user / older versions of Rubidium wrote. merge_patch is a
   // recursive (RFC 7396) merge: parsed values win where present, defaults
   // fill in any gaps. Avoids the historical crash mode where a settings file
   // from an older schema (missing e.g. "logger") caused an unchecked
   // jSettings["logger"]["level"] cast to throw and abort the process.
   m_jSettings = jDefaults;
   if (jParsed.is_object ())
      m_jSettings.merge_patch (jParsed);
}

APP::~APP () 
{ 
   SaveSettings ();

   DestroyLogger ();

   // Fallback: ApplyUpdate() spawns while m_pUpdater is still alive. If the
   // updater was already destroyed, ~APP is the last chance to launch apply.
   if (m_bApplyUpdate)
   {
      UPDATER_NATIVE Updater (nullptr);

      Updater.SpawnApply ();
   }
}

void APP::DestroyLogger ()
{
   delete m_pLogger;
   m_pLogger = nullptr;

   delete m_pILogger;
   m_pILogger = nullptr;
}

nlohmann::json& APP::SettingToJSON ()
{
   return m_jSettings;
}

void APP::SaveSettings () const
{
   std::string   sSettingsPath = m_sHomePath + "/settings.json";
   std::ofstream fOut (sSettingsPath);

   if (fOut.is_open ())
      fOut << m_jSettings.dump (3) << "\n";
}

// Maximum number of address-bar history entries retained in settings.json.
static const size_t kURL_HISTORY_MAX = 25;

std::vector<std::string> APP::UrlHistory () const
{
   std::vector<std::string> asUrl;

   auto it = m_jSettings.find ("url_history");

   if (it != m_jSettings.end ()  &&  it->is_array ())
   {
      for (const nlohmann::json& jEntry : *it)
      {
         if (jEntry.is_string ())
            asUrl.push_back (jEntry.get<std::string> ());
      }
   }

   return asUrl;
}

void APP::UrlHistory_Add (const std::string& sUrl)
{
   // Trim surrounding whitespace; an empty / blank URL is never recorded.
   size_t nFirst = sUrl.find_first_not_of (" \t\r\n");
   size_t nLast  = sUrl.find_last_not_of  (" \t\r\n");

   if (nFirst != std::string::npos)
   {
      std::string sTrimmed = sUrl.substr (nFirst, nLast - nFirst + 1);

      nlohmann::json& jHistory = m_jSettings["url_history"];

      if (!jHistory.is_array ())
         jHistory = nlohmann::json::array ();

      // Drop any existing exact duplicate so the URL re-surfaces at the top.
      for (auto it = jHistory.begin (); it != jHistory.end (); )
      {
         if (it->is_string ()  &&  it->get<std::string> () == sTrimmed)
            it = jHistory.erase (it);
         else
            ++it;
      }

      jHistory.insert (jHistory.begin (), sTrimmed);

      while (jHistory.size () > kURL_HISTORY_MAX)
         jHistory.erase (jHistory.end () - 1);

      // Persist right away so visited URLs survive even if the process is
      // killed or crashes before ~APP runs (the historical reason the macOS
      // dropdown came up empty across sessions).
      SaveSettings ();
   }
}

int APP::MovementSpeedPosition () const
{
   int nPosition = kMovementSpeedDefault;

   auto it = m_jSettings.find ("movement");

   if (it != m_jSettings.end ()  &&  it->is_object ())
      nPosition = it->value ("speed", kMovementSpeedDefault);

   return std::clamp (nPosition, kMovementSpeedMin, kMovementSpeedMax);
}

void APP::MovementSpeedPosition (int nPosition)
{
   m_jSettings["movement"]["speed"] = std::clamp (nPosition, kMovementSpeedMin, kMovementSpeedMax);
}

float APP::MovementScale () const
{
   // Geometric mapping so equal position steps feel like equal speed changes:
   // kMovementSpeedUnity maps to 1.0x, positions below slow down, above speed up.
   // The unity anchor sits above the default position, so the default lands at a
   // moderate pace rather than full speed.
   return std::pow (kMovementSpeedBase, static_cast<float> (MovementSpeedPosition () - kMovementSpeedUnity));
}

std::string const & APP::sFontFamily () const &
{
   return m_sFontFamily;
}

std::string const & APP::sHomePath () const &
{
   return m_sHomePath;
}

LOGGER* APP::Logger () const
{
   return m_pLogger;
}

void APP::sFontFamily (const std::string& _sFontFamily)
{
   m_sFontFamily = _sFontFamily;
}

// ---------------------------------------------------------------------------
// SDL Window Registry
// ---------------------------------------------------------------------------

void APP::SDLWindow_Register (ISDLWINDOW* pSDLWindow)
{
   m_mapSDLWindow[pSDLWindow->SDLWindowID ()] = pSDLWindow;
}

void APP::SDLWindow_Unregister (ISDLWINDOW* pSDLWindow)
{
   m_mapSDLWindow.erase (pSDLWindow->SDLWindowID ());
}

void APP::SDLWindow_Translate (SDL_Event& Event)
{
   SDL_WindowID nWindowID = 0;

   switch (Event.type)
   {
      case SDL_EVENT_MOUSE_MOTION:                 nWindowID = Event.motion.windowID;  break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP:              nWindowID = Event.button.windowID;  break;
      case SDL_EVENT_MOUSE_WHEEL:                  nWindowID = Event.wheel.windowID;   break;
      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_KEY_UP:                       nWindowID = Event.key.windowID;     break;
      case SDL_EVENT_TEXT_INPUT:                   nWindowID = Event.text.windowID;    break;
      case SDL_EVENT_WINDOW_MOVED:
      case SDL_EVENT_WINDOW_RESIZED:
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      case SDL_EVENT_WINDOW_MOUSE_LEAVE:
      case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
      case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
      case SDL_EVENT_WINDOW_FOCUS_GAINED:
      case SDL_EVENT_WINDOW_FOCUS_LOST:
      case SDL_EVENT_WINDOW_SHOWN:
      case SDL_EVENT_WINDOW_EXPOSED:               nWindowID = Event.window.windowID;  break;
   }

   if (nWindowID != 0)
   {
      auto it = m_mapSDLWindow.find (nWindowID);
      if (it != m_mapSDLWindow.end ())
         it->second->HandleEvent (Event);
   }
}

bool APP::s_bInWindowFilter = false;

bool APP::InWindowFilter ()
{
   return s_bInWindowFilter;
}

bool APP::SDLWindow_Filter (void* pParam, SDL_Event* pEvent)
{
   SDL_WindowID nWindowID = 0;

   switch (pEvent->type)
   {
      case SDL_EVENT_WINDOW_EXPOSED:                                                            // 516 - Window has been exposed and should be redrawn, and can be redrawn directly from event watchers for this event. data1 is 1 for live-resize expose events, 0 otherwise.
      case SDL_EVENT_WINDOW_SHOWN:
      case SDL_EVENT_WINDOW_MOVED:
      case SDL_EVENT_WINDOW_RESIZED:               nWindowID = pEvent->window.windowID; break;  // 518 - Window has been resized to data1xdata2
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:                                                 // 519 - The pixel size of the window has changed to data1xdata2
      case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:                                          break;  // 533 - The window safe area has been changed
   }

   if (nWindowID != 0)
   {
      auto pApp = static_cast<APP*>(pParam);

      auto it = pApp->m_mapSDLWindow.find (nWindowID);
      if (it != pApp->m_mapSDLWindow.end ())
      {
         // This runs synchronously inside Cocoa's resize / fullscreen-transition
         // callbacks. Mark it so RmlUi/SDL_Renderer dialogs skip presenting to a
         // Metal drawable mid-transition (a crash); the main loop repaints them.
         s_bInWindowFilter = true;
         it->second->HandleEvent (*pEvent);
         s_bInWindowFilter = false;
      }
   }

   return true;
}

// ---------------------------------------------------------------------------
// IENGINE
// ---------------------------------------------------------------------------

std::string const& APP::sAppDataPath () const&
{
   return m_sHomePath;
}

std::string const& APP::sRenderer () const&
{
   static const std::string sRenderer ("halogen");
   return sRenderer;
}

void APP::Log (SNEEZE::IENGINE::eLOGLEVEL Level, const std::string& sModule, const std::string& sMessage)
{
   m_pLogger->Log (static_cast<LOGGER::eLOGLEVEL> (Level), sModule, sMessage);
}

bool APP::CreateUpdater (IUPDATER* pNotify)
{
   m_pUpdater = new UPDATER_NATIVE (pNotify);

   return m_pUpdater->Initialize ();
}

void APP::DestroyUpdater ()
{
   // Destroy the "Release Notes" popup here (called before SDL_Quit in the shell
   // Run () sequence) so RMLUI_SDL tears its window down while SDL is still up.
   delete m_pReleaseNotes;
   m_pReleaseNotes = nullptr;

   delete m_pUpdater;
   m_pUpdater = nullptr;
}

bool APP::PresentReleaseNotes (const std::string& sVersion, const std::string& sNotes, void* pOwner, bool bMarkSeen)
{
   bool bResult = false;

   if (!sNotes.empty ())
   {
      delete m_pReleaseNotes;
      m_pReleaseNotes = new RELEASE_NOTES_RML ();

      if (m_pReleaseNotes->Initialize (sVersion, sNotes))
      {
         m_pReleaseNotes->SetOwner (pOwner);
         m_pReleaseNotes->Show ();

         if (m_pReleaseNotes->IsOpen ())
         {
            m_pLogger->Log (LOGGER::kLOGLEVEL_Info, "App", "Showing release notes for " + sVersion);
            bResult = true;

            if (bMarkSeen  &&  m_pUpdater)
               m_pUpdater->ClearReleaseNotes ();
         }
         else
         {
            m_pLogger->Log (LOGGER::kLOGLEVEL_Warning, "App", "Release notes window failed to open for " + sVersion);
         }
      }
      else
      {
         m_pLogger->Log (LOGGER::kLOGLEVEL_Warning, "App", "Release notes window failed to initialize for " + sVersion);
      }
   }

   return bResult;
}

void APP::ShowReleaseNotesIfUpdated (void* pOwner)
{
   if (m_pUpdater)
   {
      std::string sVersion;
      std::string sNotes;

      if (m_pUpdater->PendingReleaseNotes (sVersion, sNotes))
      {
         if (!PresentReleaseNotes (sVersion, sNotes, pOwner, true))
         {
            if (sNotes.empty ())
            {
               m_pLogger->Log (LOGGER::kLOGLEVEL_Info, "App", "Pending release notes for " + sVersion + " have empty body; skipping");
               m_pUpdater->ClearReleaseNotes ();
            }
         }
      }
      else
      {
         // Already seen for this version, or offline on first launch -- still
         // cache notes in settings so the ellipsis menu can show them later.
         m_pUpdater->EnsureReleaseNotesCached ();
      }
   }
}

void APP::ShowReleaseNotes (void* pOwner)
{
   if (m_pUpdater)
   {
      std::string sVersion;
      std::string sNotes;

      m_pUpdater->EnsureReleaseNotesCached ();

      if (m_pUpdater->SavedReleaseNotes (sVersion, sNotes))
      {
         if (!PresentReleaseNotes (sVersion, sNotes, pOwner, false))
         {
            if (sNotes.empty ())
               m_pLogger->Log (LOGGER::kLOGLEVEL_Info, "App", "Saved release notes for " + sVersion + " have empty body; skipping");
         }
      }
      else
      {
         m_pLogger->Log (LOGGER::kLOGLEVEL_Info, "App", "No saved release notes available for " + std::string (RUBIDIUM_VERSION));
      }
   }
}

bool APP::IsReleaseNotesOpen () const
{
   return m_pReleaseNotes  &&  m_pReleaseNotes->IsOpen ();
}

bool APP::MovementKeysSuppressed () const
{
   return false;
}

void APP::CheckForUpdate ()
{
   if (m_pUpdater)
   {
      m_pUpdater->SpawnBackgroundCheck (true, true);
   }
}

void APP::ApplyUpdate ()
{
   // Spawn before engine teardown. On Linux, delete m_pSneeze aborts the
   // process before ~APP runs, so a deferred apply in the destructor never fires.
   if (m_pUpdater)
   {
      m_pUpdater->SpawnApply ();
   }
   else
   {
      m_bApplyUpdate = true;
   }
}
