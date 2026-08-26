// Copyright 2026 Metaversal Corporation. All rights reserved.

// Android native canvas implementation.
//
// This translation unit is compiled on Android only. CMake keeps it in the
// source tree on every host and marks it HEADER_FILE_ONLY off-Android.

using namespace RUBIDIUM;

CANVAS_NATIVE::CANVAS_NATIVE (LOGGER* pLogger) :
   CANVAS (pLogger, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER)
{
}

CANVAS_NATIVE::~CANVAS_NATIVE ()
{
}

bool CANVAS_NATIVE::Initialize (void* pParentHandle, int nWidth, int nHeight)
{
   bool bResult = CANVAS::Initialize (pParentHandle, nWidth, nHeight);

   m_pWindow = static_cast<SDL_Window*> (pParentHandle);
   m_bOwnsWindow = false;
   if (m_pWindow)
   {
      APPNATIVE::GetInstance ()->SDLWindow_Register (this);
      bResult = true;
   }
   else m_pLogger->Log (LOGGER::kLOGLEVEL_Error, "CANVAS", "No parent window provided");

   return bResult;
}

void CANVAS_NATIVE::SetVisible (bool bVisible)
{
   if (m_pWindow)
   {
      if (bVisible)
         SDL_ShowWindow (m_pWindow);
      else
         SDL_HideWindow (m_pWindow);
   }
}
