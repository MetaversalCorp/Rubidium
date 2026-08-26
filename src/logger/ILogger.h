// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_LOGGER_ILOGGER_H
#define RUBIDIUM_LOGGER_ILOGGER_H

class ILOGGER_NATIVE : public ILOGGER
{
public:
   ILOGGER_NATIVE (bool bUseStdOut);

   void onMessage (std::string& sLine) override;

private:
   bool        m_bUseStdOut;

   void onMessage_StdOut (std::string& sLine);
   void onMessage_File   (std::string& sLine);
};

#endif // RUBIDIUM_LOGGER_ILOGGER_H
