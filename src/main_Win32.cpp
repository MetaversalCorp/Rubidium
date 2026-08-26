// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "shell/App.h"
#include "cli/MsfCli.h"

// REMOVE THIS
#define TEMPORARY_CONSOLE_WINDOW

int WINAPI WinMain (_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
   int nResult;

   if (RUBIDIUM::MSF_CLI::IsCommand (__argc, __argv))
   {
      // Rubidium is a GUI-subsystem executable, so it has no console of its own.
      // If the caller already gave it usable stdout (a redirection to a file or
      // a pipe), leave it alone so "Rubidium --verify x.msf > log.txt" and piping
      // work. Otherwise attach to the invoking console (or allocate one) and
      // route stdout/stderr there so the CLI output is visible.
      HANDLE hStdout = GetStdHandle (STD_OUTPUT_HANDLE);
      bool bHaveStdout = (hStdout != nullptr  &&  hStdout != INVALID_HANDLE_VALUE  &&  GetFileType (hStdout) != FILE_TYPE_UNKNOWN);

      if (!bHaveStdout)
      {
         if (AttachConsole (ATTACH_PARENT_PROCESS) == FALSE)
            AllocConsole ();

         FILE* pConsole;
         freopen_s (&pConsole, "CONOUT$", "w", stdout);
         freopen_s (&pConsole, "CONOUT$", "w", stderr);
      }

      nResult = RUBIDIUM::MSF_CLI::Run (__argc, __argv);
   }
   else
   {
      RUBIDIUM::APPNATIVE* pApp = RUBIDIUM::APPNATIVE::GetInstance ();

#ifdef TEMPORARY_CONSOLE_WINDOW
      nlohmann::json& jSettings = pApp->SettingToJSON ();

      bool bConsole = jSettings["developer"].value ("console", false);

      // When launched from cmd, reuse that console so "cmd /k Rubidium.exe" shows output.
      if (AttachConsole (ATTACH_PARENT_PROCESS) != FALSE)
         bConsole = true;
      else if (bConsole)
         AllocConsole ();

      if (bConsole)
      {
         FILE* pConsole;
         freopen_s (&pConsole, "CONOUT$", "w", stdout);
         freopen_s (&pConsole, "CONOUT$", "w", stderr);
      }
#endif

      nResult = pApp->Run ();
   }

   return nResult;
}
