// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifdef __ANDROID__

#include "shell/App.h"
#include "shell/ChromeXr.h"
#include "rmlui_sdl/RmlUi_SDL.h"

#include <Sneeze.h>
#include "xr/XrRuntime.h"

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_system.h>
#include <jni.h>

#include <cstring>
#include <vector>

namespace RUBIDIUM
{

namespace
{
   static const char* s_szStyle = R"css(
body {
   margin: 0;
   padding: 0;
   width: 512dp;
   height: 64dp;
   background: #202124;
   font-family: Inter;
   font-size: 18dp;
   color: #E8EAED;
}
#toolbar {
   width: 512dp;
   height: 64dp;
   padding: 10dp 16dp;
   box-sizing: border-box;
}
#urlbar {
   display: block;
   width: 100%;
   height: 44dp;
   padding: 8dp 16dp;
   box-sizing: border-box;
   background: #303134;
   color: #E8EAED;
   border-width: 1px;
   border-color: #5F6368;
   font-family: Inter;
   font-size: 16dp;
}
#urlbar:focus {
   border-color: #8AB4F8;
}
)css";

   static const char* s_szDocument = R"rml(<rml>
<head>
<style>
[{STYLE}]
</style>
</head>
<body>
   <div id="toolbar">
      <input type="text" id="urlbar"/>
   </div>
</body>
</rml>)rml";

   void AndroidSetUrlText (const std::string& sUrl)
   {
      JNIEnv* env = static_cast<JNIEnv*> (SDL_GetAndroidJNIEnv ());
      jobject activity = static_cast<jobject> (SDL_GetAndroidActivity ());

      if (env  &&  activity)
      {
         jclass cls = env->GetObjectClass (activity);
         jmethodID mid = env->GetMethodID (cls, "setUrlText", "(Ljava/lang/String;)V");

         if (mid)
         {
            jstring jUrl = env->NewStringUTF (sUrl.c_str ());
            env->CallVoidMethod (activity, mid, jUrl);
            env->DeleteLocalRef (jUrl);
         }

         env->DeleteLocalRef (cls);
         env->DeleteLocalRef (activity);
      }
   }
}

class CHROME_XR::Impl
{
public:
   RMLUI_SDL*   m_pWindow = nullptr;
   std::string  m_sUrl;
   bool         m_bReady = false;

   ~Impl ()
   {
      Shutdown ();
   }

   bool Initialize ()
   {
      bool bOk = true;

      // Offscreen RmlUi needs an SDL window, which blocks on Quest waiting
      // for an ANativeWindow. Tick () already paints a solid chrome quad
      // when CaptureRgba is unavailable; IME submit still uses JNI.
      m_bReady = true;
      SetUrl (m_sUrl);

      return bOk;
   }

   void Shutdown ()
   {
      m_bReady = false;
      delete m_pWindow;
      m_pWindow = nullptr;
   }

   void SetUrl (const std::string& sUrl, bool bSyncIme = true)
   {
      m_sUrl = sUrl;

      if (m_bReady  &&  m_pWindow)
      {
         Rml::ElementDocument* pDoc = m_pWindow->Document ();
         if (pDoc)
         {
            Rml::Element* pEl = pDoc->GetElementById ("urlbar");
            if (pEl)
               pEl->SetAttribute ("value", sUrl);
         }
      }

      if (bSyncIme)
         AndroidSetUrlText (sUrl);
   }

   void Focus ()
   {
      if (m_bReady  &&  m_pWindow)
      {
         Rml::ElementDocument* pDoc = m_pWindow->Document ();
         if (pDoc)
         {
            Rml::Element* pEl = pDoc->GetElementById ("urlbar");
            if (pEl)
               pEl->Focus ();
         }
      }
   }

   void Tick (SNEEZE::ENGINE* pEngine)
   {
      if (pEngine  &&  pEngine->XrRuntime ())
      {
         std::vector<uint8_t> aRgba;
         int nWidth = 0, nHeight = 0;
         bool bHave = false;

         if (m_bReady  &&  m_pWindow)
            bHave = m_pWindow->CaptureRgba (aRgba, nWidth, nHeight)  &&  !aRgba.empty ();

         if (!bHave)
         {
            nWidth  = 512;
            nHeight = 64;
            aRgba.assign (static_cast<size_t> (nWidth) * static_cast<size_t> (nHeight) * 4u, 0);
            for (size_t nPx = 0; nPx < aRgba.size (); nPx += 4)
            {
               aRgba[nPx + 0] = 0x20;
               aRgba[nPx + 1] = 0x21;
               aRgba[nPx + 2] = 0x24;
               aRgba[nPx + 3] = 0xFF;
            }
            bHave = true;
         }

         if (bHave)
            pEngine->XrRuntime ()->SetChromePixels (aRgba.data (), nWidth, nHeight);
      }
   }
};

CHROME_XR::CHROME_XR () : m_pImpl (new Impl ()) {}
CHROME_XR::~CHROME_XR () { delete m_pImpl; }

CHROME_XR& CHROME_XR::GetInstance ()
{
   static CHROME_XR sInstance;
   return sInstance;
}

bool CHROME_XR::Initialize ()                 { return m_pImpl->Initialize (); }
void CHROME_XR::Shutdown ()                   { m_pImpl->Shutdown (); }
void CHROME_XR::SetUrl (const std::string& s, bool bSyncIme) { m_pImpl->SetUrl (s, bSyncIme); }
void CHROME_XR::Focus ()                      { m_pImpl->Focus (); }
void CHROME_XR::Tick (SNEEZE::ENGINE* pEngine) { m_pImpl->Tick (pEngine); }

} // namespace RUBIDIUM

#endif // __ANDROID__
