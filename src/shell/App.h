// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_SHELL_APP_H
#define RUBIDIUM_SHELL_APP_H

#include <SDL3/SDL_events.h>

namespace RUBIDIUM
{
   class APPFRAME;

   class ISDLWINDOW
   {
   public:
      virtual ~ISDLWINDOW () = default;

      virtual SDL_WindowID SDLWindowID () const = 0;
      virtual void         HandleEvent (SDL_Event& Event) = 0;
   };

   class IAPPWINDOW
   {
   public:
      virtual ~IAPPWINDOW () = default;

      virtual void Window_OnCreate  (APPFRAME* pAppFrame, APPFRAME* pAppFrame_From, int& nX, int& nY, int& nWidth, int& nHeight, bool& bMaximized) = 0;
      virtual void Window_OnDestroy (APPFRAME* pAppFrame) = 0;
      virtual void Window_OnFocus   (APPFRAME* pAppFrame) = 0;
      virtual void Window_OnBlur    (APPFRAME* pAppFrame) = 0;
      virtual void Window_OnNew     (APPFRAME* pAppFrame_From, SNEEZE::CONTEXT::eSESSION eSession) = 0;
      virtual void Window_OnExit    () = 0;
   };

   class IUPDATER;
   class UPDATER_NATIVE;
   class RELEASE_NOTES_RML;
   class APP : public SNEEZE::IENGINE
   {
   public:
      explicit APP (std::string sHomePath, ILOGGER* pILogger);
      ~APP ();

      virtual int Run () = 0;

      // === SDL Window Registry
      void SDLWindow_Register   (ISDLWINDOW* pSDLWindow);
      void SDLWindow_Unregister (ISDLWINDOW* pSDLWindow);
      void SDLWindow_Translate  (SDL_Event& Event);
      static bool SDLWindow_Filter (void* pParam, SDL_Event* pEvent);

      // True while SDLWindow_Filter is dispatching an event -- i.e. we are inside
      // SDL's event watch, which fires synchronously from within Cocoa's own
      // resize / fullscreen-transition callbacks. Presenting to a Metal drawable
      // in that window (as an RmlUi/SDL_Renderer dialog would) crashes while the
      // drawable is being recreated, so those windows skip GPU work here and let
      // the main loop repaint them once the callback unwinds.
      static bool InWindowFilter ();

      nlohmann::json& SettingToJSON ();

      // Flush the in-memory settings to settings.json immediately. Called at
      // exit (~APP) and after mutations that must survive an abnormal
      // termination (e.g. URL history), so a crash / hard kill can't lose them.
      void SaveSettings () const;

      // === URL history (persisted under "url_history" in settings.json,
      //     most-recent first). Shared by every chrome's address-bar dropdown.
      std::vector<std::string> UrlHistory ()     const;
      void                     UrlHistory_Add (const std::string& sUrl);

      // === Movement speed (WASD travel distance)
      //
      // Persisted under "movement.speed" as an integer position on a fixed scale
      // (kMovementSpeedMin .. kMovementSpeedMax, default kMovementSpeedDefault).
      // The '+'/'-'/'/' keys and the Settings "Movement" slider both drive this
      // single value; the canvas converts it to MovementScale() and feeds it to
      // the engine each frame. Position -> scale is geometric so the slider feels
      // perceptually linear (position kMovementSpeedDefault == 1.0x).
      static constexpr int   kMovementSpeedMin     = 1;
      static constexpr int   kMovementSpeedMax     = 50;
      static constexpr int   kMovementSpeedDefault = 25;   // startup / '/' reset position
      static constexpr int   kMovementSpeedUnity   = 38;   // position that maps to 1.0x scale
      static constexpr float kMovementSpeedBase    = 1.15f;

      int   MovementSpeedPosition () const;
      void  MovementSpeedPosition (int nPosition);
      float MovementScale ()        const;

      // === Accessors
      std::string const & sHomePath ()   const &;
      std::string const & sFontFamily () const &;
      LOGGER*             Logger ()       const;

      // === Modifiers
      void sFontFamily (const std::string& _sFontFamily);

      bool CreateUpdater (IUPDATER* pNotify);
      void DestroyUpdater ();
      void CheckForUpdate ();
      void ApplyUpdate ();

      // Shown once after an update: if RubidiumSetup staged release notes for the
      // running build, display a modal "Release Notes" popup (owned by pOwner, the
      // main window's native handle) and clear the pending entry.
      void ShowReleaseNotesIfUpdated (void* pOwner);

      // Show the saved release notes for the running build (ellipsis menu).
      void ShowReleaseNotes (void* pOwner);

      // True while the "Release Notes" popup is open. Polled by the SDL chrome each
      // frame (APPFRAME_SDL::ProcessInput) to enforce in-app modality, since
      // windowing-system modality is unreliable for borderless SDL windows.
      bool IsReleaseNotesOpen () const;

      // SDL platforms: true while a text field (URL bar, inspector search, etc.)
      // should capture WASD-style movement keys. Win32 always returns false
      // (Canvas polls GetAsyncKeyState and checks the native URL Edit itself).
      virtual bool MovementKeysSuppressed () const;

   private:
      bool PresentReleaseNotes (const std::string& sVersion, const std::string& sNotes, void* pOwner, bool bMarkSeen);

      // Set only while SDLWindow_Filter (the SDL event watch) is dispatching --
      // see InWindowFilter (). Static: the watch is a single app-wide callback.
      static bool s_bInWindowFilter;

   public: // IENGINE
      std::string const & sAppDataPath ()       const  & override;
      std::string const & sRenderer ()          const  & override;
      void Log (eLOGLEVEL Level, const std::string& sModule, const std::string& sMessage) override;

   protected:
      // The LOGGER depends on the ILOGGER backend, so it must be torn down
      // first. DestroyLogger() deletes m_pLogger (idempotent); the derived
      // class calls it from its own destructor -- which runs before ~APP --
      // and only then deletes m_pILogger (the backend it created and owns).
      void DestroyLogger ();

      std::string          m_sHomePath;
      UPDATER_NATIVE*      m_pUpdater;
      RELEASE_NOTES_RML*   m_pReleaseNotes;
      ILOGGER*             m_pILogger;

   private:
      std::string          m_sFontFamily;
      nlohmann::json       m_jSettings;
      LOGGER*              m_pLogger;
      std::unordered_map<SDL_WindowID, ISDLWINDOW*> m_mapSDLWindow;

      bool                 m_bApplyUpdate;
   };
}

#include "shell/AppFrame.h"
#include "canvas/Canvas.h"

#endif // RUBIDIUM_SHELL_APP_H
