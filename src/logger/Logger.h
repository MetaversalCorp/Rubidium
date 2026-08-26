#ifndef RUBIDIUM_LOGGER_H
#define RUBIDIUM_LOGGER_H

class ILOGGER
{
public:
   virtual ~ILOGGER () = default;

   virtual void onMessage (std::string& sLine) = 0;
};

class LOGGER
{
public:
   enum eLOGLEVEL
   {
      kLOGLEVEL_Trace,
      kLOGLEVEL_Info,
      kLOGLEVEL_Warning,
      kLOGLEVEL_Error,
      kLOGLEVEL_Off,

      kLOGLEVEL_COUNT
   };

public:
   LOGGER (ILOGGER* pLogger);
   ~LOGGER ();

   void LogLevel (eLOGLEVEL Level);
   eLOGLEVEL LogLevel () const;

   void Log (eLOGLEVEL Level, std::string sModule, std::string sMessage);

private:
   class Impl;
   Impl* m_pImpl;
};

#endif // RUBIDIUM_LOGGER_H
