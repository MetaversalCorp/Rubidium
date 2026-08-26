// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_SHELL_APPFRAME_H
#define RUBIDIUM_SHELL_APPFRAME_H

#define WM_LAUNCH                                         (WM_USER + 150)

namespace RUBIDIUM 
{
   class APPFRAME
   {
   public:
      APPFRAME (IAPPWINDOW* pController, LOGGER* pLogger);
      virtual ~APPFRAME ();

      void Log (LOGGER::eLOGLEVEL Level, const std::string& sModule, const std::string& sMessage);

      virtual void  ProcessInput ()                                                                                             = 0;
      virtual void* NativeWindow () const                                                                                      = 0;
      virtual void* Init (APPFRAME* pAppFrame_From, SNEEZE::CONTEXT::eSESSION eSession = SNEEZE::CONTEXT::kSESSION_PERSISTENT) = 0;

      virtual void  ToggleInspector ()                                                                                          {}

   protected:
      IAPPWINDOW*                m_pController;
      LOGGER*                    m_pLogger;
   };
}
#endif // RUBIDIUM_SHELL_APPFRAME_H
