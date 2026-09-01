/*******************************************************************************************************************************
**                                                                                                                            **
**                                                    Rubidium : Logger.cpp                                                    **
**                                                                                                                            **
********************************************************************************************************************************
**                              Copyright 2014-2026 Metaversal Corporation. All rights reserved.                              **
*******************************************************************************************************************************/

#include "Logger.h"

/*******************************************************************************************************************************
**                                                     CLASS (Impl)                                                           **
*******************************************************************************************************************************/

static const char* apcszLevels[] =
{
   "TRACE",
   "INFO",
   "WARNING",
   "ERROR",
   "OFF"
};

typedef struct
{
   time_t                              tNow;
   int                                 nMilliseconds;
   std::string                         sMessage;
   LOGGER::eLOGLEVEL                   Level;
   std::string                         sModule;
}
LOGGER_MESSAGE;

class LOGGER::Impl
{
public:
   Impl (ILOGGER* pLogger) :
      m_pLogger (pLogger),
      m_bShutdown (false)
   {
      m_pThread = new std::thread (&LOGGER::Impl::ThreadLoop, this);
   }

   ~Impl ()
   {
      Shutdown ();
      m_pThread->join ();

      delete m_pThread;
   }

   void Shutdown ()
   {
      std::lock_guard<std::mutex> guard (m_mutex);
      m_bShutdown = true;
      m_condVar.notify_all ();
   }

   void ThreadLoop ()
   {
      std::unique_lock<std::mutex> mlock (m_mutex);
      m_condVar.wait (mlock, std::bind (&LOGGER::Impl::Control, this));
   }

   bool Control ()
   {
      bool bExit = false;
      LOGGER_MESSAGE lm;
      tm tmNow;

      if (m_bShutdown == false)
      {
         do
         {
            std::string sLine;

            m_CS.lock ();
            {
               if (m_aMsg.empty () == false)
               {
                  lm = m_aMsg.front ();
                  m_aMsg.pop ();
               }
               else lm.tNow = 0;
            }
            m_CS.unlock ();

            if (lm.tNow != 0)
            {
#if defined(_MSC_VER)  
               localtime_s (&tmNow, &lm.tNow); // MSVC path
#else
               localtime_r (&lm.tNow, &tmNow); // GCC/Clang path
#endif  

               AppendFormattedNum (sLine, 1900 + tmNow.tm_year); sLine += "-";
               AppendFormattedNum (sLine, tmNow.tm_mon + 1);     sLine += "-";
               AppendFormattedNum (sLine, tmNow.tm_mday);
               sLine += " ";

               AppendFormattedNum (sLine, tmNow.tm_hour);        sLine += ":";
               AppendFormattedNum (sLine, tmNow.tm_min);         sLine += ":";
               AppendFormattedNum (sLine, tmNow.tm_sec);         sLine += ".";
               AppendFormattedNum (sLine, lm.nMilliseconds, 100);
               sLine += " ";

               sLine += apcszLevels[lm.Level];
               sLine += ": \t";

               sLine += lm.sModule + "\t";

               sLine += lm.sMessage;

               m_pLogger->onMessage (sLine);
            }
         }
         while (lm.tNow != 0);
      }

      return m_bShutdown;
   }

   void SetLogLevel (eLOGLEVEL Level)
   {
      m_CS.lock ();
      {
         m_Level = Level;
      }
      m_CS.unlock ();
   }

   LOGGER::eLOGLEVEL GetLogLevel ()
   {
      eLOGLEVEL Level;

      m_CS.lock ();
      {
         Level = m_Level;
      }
      m_CS.unlock ();

      return Level;
   }

   bool IsLogAllowed (eLOGLEVEL Level)
   {
      bool bResult;

      m_CS.lock ();
      {
         bResult = (Level >= m_Level);
      }
      m_CS.unlock ();

      return bResult;
   }

   void Log (eLOGLEVEL Level, const std::string& sModule, const std::string& sMessage)
   {
      std::chrono::milliseconds msNow;
      LOGGER_MESSAGE lm;

      auto Now = std::chrono::system_clock::now ();
      lm.tNow = std::chrono::system_clock::to_time_t (Now);
      msNow = std::chrono::duration_cast<std::chrono::milliseconds>(Now.time_since_epoch ()) % 1000;

      lm.nMilliseconds = (int)msNow.count ();
      lm.sMessage = sMessage;
      lm.Level = Level;
      lm.sModule = sModule;

      m_CS.lock ();
      {
         m_aMsg.push (lm);
      }
      m_CS.unlock ();

      CtlBreak_Thread ();
   }

private:
   void AppendFormattedNum (std::string& sLine, int nNum, int nThreshold = 10)
   {
      if (nNum < nThreshold)
         sLine += "0";
      sLine += std::to_string (nNum);
   }

   void CtlBreak_Thread ()
   {
      std::lock_guard<std::mutex> guard (m_mutex);
      m_condVar.notify_all ();
   }

private:
   std::thread*               m_pThread;
   std::mutex                 m_mutex;
   std::condition_variable    m_condVar;
   bool                       m_bShutdown;

   ILOGGER* m_pLogger;

   LOGGER::eLOGLEVEL          m_Level;
   std::recursive_mutex       m_CS;
   std::queue<LOGGER_MESSAGE> m_aMsg;
};

/*******************************************************************************************************************************
**                                                     CLASS (LOGGER)                                                         **
*******************************************************************************************************************************/

LOGGER::LOGGER (ILOGGER* pLogger)
{
   std::string sModule = "MVMF";
   std::string sMessage = "LOGGER Started";
   m_pImpl = new Impl (pLogger);

   m_pImpl->Log (kLOGLEVEL_Info, sModule, sMessage);
}

LOGGER::~LOGGER ()
{
   delete m_pImpl;
}

void LOGGER::LogLevel (eLOGLEVEL Level)
{
   m_pImpl->SetLogLevel (Level);
}

LOGGER::eLOGLEVEL LOGGER::LogLevel () const
{
   return m_pImpl->GetLogLevel ();
}

void LOGGER::Log (eLOGLEVEL Level, std::string sModule, std::string sMessage)
{
   if (m_pImpl->IsLogAllowed (Level))
      m_pImpl->Log (Level, sModule, sMessage);
}

/******************************************************************************************************************************/
