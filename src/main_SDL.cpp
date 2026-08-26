// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "shell/App_SDL.h"
#include "cli/MsfCli.h"

#include <SDL3/SDL_main.h>

int main (int argc, char* argv[])
{
   int nResult;

   if (RUBIDIUM::MSF_CLI::IsCommand (argc, argv))
      nResult = RUBIDIUM::MSF_CLI::Run (argc, argv);
   else
   {
      RUBIDIUM::APPSDL* pApp = RUBIDIUM::APPSDL::GetInstance ();
      nResult = pApp->Run ();
   }

   return nResult;
}
