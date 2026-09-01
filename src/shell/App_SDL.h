// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// SDL-based platform entry (macOS, Linux, iOS, Android). Mirrors APPNATIVE on
// Windows: Meyers singleton, IAPPWINDOW callbacks in Impl, APP owns settings.

#ifndef RUBIDIUM_SHELL_APP_SDL_H
#define RUBIDIUM_SHELL_APP_SDL_H

#include "shell/App.h"

namespace RUBIDIUM
{
   class APPSDL : public APP
   {
   private:
      class Impl;
      Impl* m_pImpl;

   public:
      static APPSDL* GetInstance ();

      int Run () override;

      bool MovementKeysSuppressed () const override;

#ifdef __ANDROID__
      void* XrAndroidVm ()       const override;
      void* XrAndroidActivity () const override;
#endif

   private:
      APPSDL ();
      ~APPSDL ();

      static std::string ResolveAppDataPath ();

      APPSDL (const APPSDL&)            = delete;
      void operator= (const APPSDL&)     = delete;

      ILOGGER* m_pILogger;
   };
}

#ifndef RUBIDIUM_PLATFORM_WINDOWS
#define APPNATIVE APPSDL
#endif

#endif // RUBIDIUM_SHELL_APP_SDL_H
