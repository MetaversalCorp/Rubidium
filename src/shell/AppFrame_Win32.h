// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// Windows APPFRAME — native Win32 shell with custom dark-mode chrome.
// Does NOT use SDL for the top-level window (only the Canvas child
// surface does). Keeps the implementation isolated to Windows so
// every other platform's APPFRAME_NATIVE inherits from APPFRAME_SDL instead.

#ifndef RUBIDIUM_SHELL_APPFRAME_NATIVE_H
#define RUBIDIUM_SHELL_APPFRAME_NATIVE_H

namespace RUBIDIUM 
{ 
   class APPFRAME_NATIVE : public APPFRAME
   {
   public:
      APPFRAME_NATIVE (IAPPWINDOW* pController, SNEEZE::ENGINE* pSneeze, LOGGER* pLogger);
      ~APPFRAME_NATIVE () override;

      void  ProcessInput ()                                         override;
      void* NativeWindow () const                                    override;
      void* Init (APPFRAME* pAppFrame_From, SNEEZE::CONTEXT::eSESSION eSession = SNEEZE::CONTEXT::kSESSION_PERSISTENT) override;

      static LRESULT CALLBACK WndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

   private:
      void NotifyChildReady ();
      int  CanvasWidth ()  const;
      int  CanvasHeight () const;

   private:
      class Impl;
      Impl* m_pImpl;
   };
}

#endif // RUBIDIUM_SHELL_APPFRAME_NATIVE_H
