// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_UPDATER_UPDATERWND_H
#define RUBIDIUM_UPDATER_UPDATERWND_H

#include "Updater.h"

namespace RUBIDIUM
{
   class UPDATER_NATIVE : public UPDATER
   {
   public:
      UPDATER_NATIVE (IUPDATER* pNotify);
      ~UPDATER_NATIVE ();

      void RunCheck (const std::string& sCurrentVersion, bool bForce) override;

   protected:
      std::string SetupExePath () const override;
   };
}

#endif // RUBIDIUM_UPDATER_UPDATERWND_H
