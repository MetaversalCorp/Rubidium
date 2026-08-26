// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// MSF_CLI -- command-line signing / verification mode for Rubidium. Mirrors
// Sneeze's standalone SignMsf tool: when the executable is launched with
// --sign or --verify, Rubidium performs the JWS operation on an .msf file and
// exits instead of opening the browser window. Built entirely on SNEEZE::MSF,
// which requires no ENGINE for signing or verification.

#ifndef RUBIDIUM_CLI_MSFCLI_H
#define RUBIDIUM_CLI_MSFCLI_H

namespace RUBIDIUM
{
   class MSF_CLI
   {
   public:
      // True when the command line requests sign / verify mode (--sign or --verify present) rather than launching the browser GUI.
      static bool IsCommand (int nArgc, char** aArgv);

      // Perform the requested sign / verify operation and return a process exit code (0 = success). Only meaningful when IsCommand() is true.
      static int  Run (int nArgc, char** aArgv);
   };
}

#endif // RUBIDIUM_CLI_MSFCLI_H
