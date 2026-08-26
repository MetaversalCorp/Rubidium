// Copyright 2026 Metaversal Corporation. All rights reserved.

using namespace RUBIDIUM;

APPFRAME::APPFRAME (IAPPWINDOW* pController, LOGGER* pLogger) :
   m_pController (pController),
   m_pLogger (pLogger)
{
}

APPFRAME::~APPFRAME ()
{
}

void APPFRAME::Log (LOGGER::eLOGLEVEL Level, const std::string& sModule, const std::string& sMessage)
{
   m_pLogger->Log (Level, sModule, sMessage);
}
