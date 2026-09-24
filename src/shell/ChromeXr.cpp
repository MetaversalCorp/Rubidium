// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifdef __ANDROID__

#include "shell/App.h"
#include "shell/ChromeXr.h"

#include <Sneeze.h>
#include "xr/XrRuntime.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_system.h>
#include <jni.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace RUBIDIUM
{

namespace
{
   // 5x7 glyphs, column-major, bit 0 = top. Index = ch - 32.
   static const uint8_t s_aGlyph[95][5] =
   {
      { 0x00, 0x00, 0x00, 0x00, 0x00 },
      { 0x00, 0x00, 0x2F, 0x00, 0x00 },
      { 0x00, 0x03, 0x00, 0x03, 0x00 },
      { 0x0A, 0x1F, 0x0A, 0x1F, 0x0A },
      { 0x24, 0x2A, 0x7F, 0x2A, 0x12 },
      { 0x13, 0x0B, 0x04, 0x1A, 0x19 },
      { 0x1A, 0x25, 0x25, 0x2A, 0x10 },
      { 0x00, 0x00, 0x03, 0x00, 0x00 },
      { 0x00, 0x00, 0x1E, 0x21, 0x00 },
      { 0x00, 0x21, 0x1E, 0x00, 0x00 },
      { 0x0A, 0x04, 0x1F, 0x04, 0x0A },
      { 0x08, 0x08, 0x3E, 0x08, 0x08 },
      { 0x00, 0x40, 0x30, 0x00, 0x00 },
      { 0x08, 0x08, 0x08, 0x08, 0x08 },
      { 0x00, 0x00, 0x20, 0x00, 0x00 },
      { 0x10, 0x08, 0x04, 0x02, 0x01 },
      { 0x3E, 0x51, 0x49, 0x45, 0x3E },
      { 0x00, 0x42, 0x7F, 0x40, 0x00 },
      { 0x42, 0x61, 0x51, 0x49, 0x46 },
      { 0x22, 0x41, 0x49, 0x49, 0x36 },
      { 0x18, 0x14, 0x12, 0x7F, 0x10 },
      { 0x27, 0x45, 0x45, 0x45, 0x39 },
      { 0x3C, 0x4A, 0x49, 0x49, 0x30 },
      { 0x01, 0x71, 0x09, 0x05, 0x03 },
      { 0x36, 0x49, 0x49, 0x49, 0x36 },
      { 0x06, 0x49, 0x49, 0x29, 0x1E },
      { 0x00, 0x00, 0x12, 0x00, 0x00 },
      { 0x00, 0x40, 0x32, 0x00, 0x00 },
      { 0x08, 0x14, 0x22, 0x41, 0x00 },
      { 0x14, 0x14, 0x14, 0x14, 0x14 },
      { 0x00, 0x41, 0x22, 0x14, 0x08 },
      { 0x02, 0x01, 0x51, 0x09, 0x06 },
      { 0x3E, 0x41, 0x5D, 0x55, 0x1E },
      { 0x7E, 0x09, 0x09, 0x09, 0x7E },
      { 0x7F, 0x49, 0x49, 0x49, 0x36 },
      { 0x3E, 0x41, 0x41, 0x41, 0x22 },
      { 0x7F, 0x41, 0x41, 0x22, 0x1C },
      { 0x7F, 0x49, 0x49, 0x49, 0x41 },
      { 0x7F, 0x09, 0x09, 0x09, 0x01 },
      { 0x3E, 0x41, 0x49, 0x49, 0x3A },
      { 0x7F, 0x08, 0x08, 0x08, 0x7F },
      { 0x00, 0x41, 0x7F, 0x41, 0x00 },
      { 0x20, 0x40, 0x41, 0x3F, 0x01 },
      { 0x7F, 0x08, 0x14, 0x22, 0x41 },
      { 0x7F, 0x40, 0x40, 0x40, 0x40 },
      { 0x7F, 0x02, 0x0C, 0x02, 0x7F },
      { 0x7F, 0x02, 0x04, 0x08, 0x7F },
      { 0x3E, 0x41, 0x41, 0x41, 0x3E },
      { 0x7F, 0x09, 0x09, 0x09, 0x06 },
      { 0x3E, 0x41, 0x51, 0x21, 0x5E },
      { 0x7F, 0x09, 0x19, 0x29, 0x46 },
      { 0x26, 0x49, 0x49, 0x49, 0x32 },
      { 0x01, 0x01, 0x7F, 0x01, 0x01 },
      { 0x3F, 0x40, 0x40, 0x40, 0x3F },
      { 0x1F, 0x20, 0x40, 0x20, 0x1F },
      { 0x3F, 0x40, 0x38, 0x40, 0x3F },
      { 0x63, 0x14, 0x08, 0x14, 0x63 },
      { 0x03, 0x04, 0x78, 0x04, 0x03 },
      { 0x61, 0x51, 0x49, 0x45, 0x43 },
      { 0x00, 0x7F, 0x41, 0x41, 0x00 },
      { 0x01, 0x02, 0x04, 0x08, 0x10 },
      { 0x00, 0x41, 0x41, 0x7F, 0x00 },
      { 0x04, 0x02, 0x01, 0x02, 0x04 },
      { 0x40, 0x40, 0x40, 0x40, 0x40 },
      { 0x00, 0x01, 0x02, 0x00, 0x00 },
      { 0x20, 0x54, 0x54, 0x54, 0x78 },
      { 0x7F, 0x44, 0x44, 0x44, 0x38 },
      { 0x38, 0x44, 0x44, 0x44, 0x28 },
      { 0x38, 0x44, 0x44, 0x44, 0x7F },
      { 0x38, 0x54, 0x54, 0x54, 0x18 },
      { 0x08, 0x7E, 0x09, 0x01, 0x02 },
      { 0x08, 0x54, 0x54, 0x54, 0x3C },
      { 0x7F, 0x04, 0x04, 0x04, 0x78 },
      { 0x00, 0x44, 0x7D, 0x40, 0x00 },
      { 0x20, 0x40, 0x44, 0x3D, 0x00 },
      { 0x7F, 0x10, 0x28, 0x44, 0x00 },
      { 0x00, 0x41, 0x7F, 0x40, 0x00 },
      { 0x7C, 0x04, 0x18, 0x04, 0x78 },
      { 0x7C, 0x04, 0x04, 0x04, 0x78 },
      { 0x38, 0x44, 0x44, 0x44, 0x38 },
      { 0x7C, 0x14, 0x14, 0x14, 0x08 },
      { 0x08, 0x14, 0x14, 0x14, 0x7C },
      { 0x7C, 0x08, 0x04, 0x04, 0x08 },
      { 0x48, 0x54, 0x54, 0x54, 0x24 },
      { 0x04, 0x3F, 0x44, 0x40, 0x20 },
      { 0x3C, 0x40, 0x40, 0x40, 0x7C },
      { 0x1C, 0x20, 0x40, 0x20, 0x1C },
      { 0x3C, 0x40, 0x30, 0x40, 0x3C },
      { 0x44, 0x28, 0x10, 0x28, 0x44 },
      { 0x0C, 0x50, 0x50, 0x50, 0x3C },
      { 0x44, 0x64, 0x54, 0x4C, 0x44 },
      { 0x00, 0x08, 0x77, 0x41, 0x00 },
      { 0x00, 0x00, 0x7F, 0x00, 0x00 },
      { 0x00, 0x41, 0x77, 0x08, 0x00 },
      { 0x08, 0x04, 0x08, 0x10, 0x08 },
   };

   constexpr int kChromeW     = 512;
   constexpr int kChromeH     = 64;
   constexpr int kScale       = 2;
   constexpr int kGlyphW      = 5;
   constexpr int kGlyphH      = 7;
   constexpr int kAdvance     = kGlyphW * kScale + 2;

   void FillRect (uint8_t* pRgba, int nX, int nY, int nW, int nH,
                  uint8_t nR, uint8_t nG, uint8_t nB)
   {
      int nX0 = nX < 0 ? 0 : nX;
      int nY0 = nY < 0 ? 0 : nY;
      int nX1 = nX + nW;
      int nY1 = nY + nH;
      if (nX1 > kChromeW)
         nX1 = kChromeW;
      if (nY1 > kChromeH)
         nY1 = kChromeH;

      for (int nPy = nY0; nPy < nY1; nPy++)
      {
         uint8_t* pRow = pRgba + static_cast<size_t> (nPy) * kChromeW * 4u
            + static_cast<size_t> (nX0) * 4u;
         for (int nPx = nX0; nPx < nX1; nPx++)
         {
            pRow[0] = nR;
            pRow[1] = nG;
            pRow[2] = nB;
            pRow[3] = 0xFF;
            pRow += 4;
         }
      }
   }

   void DrawGlyph (uint8_t* pRgba, int nX, int nY, char c,
                   uint8_t nR, uint8_t nG, uint8_t nB)
   {
      unsigned char nCh = static_cast<unsigned char> (c);
      if (nCh < 32  ||  nCh > 126)
         nCh = '?';

      const uint8_t* pCol = s_aGlyph[nCh - 32];
      for (int nCol = 0; nCol < kGlyphW; nCol++)
      {
         uint8_t nBits = pCol[nCol];
         for (int nRow = 0; nRow < kGlyphH; nRow++)
         {
            if (nBits & (1u << nRow))
               FillRect (pRgba, nX + nCol * kScale, nY + nRow * kScale,
                         kScale, kScale, nR, nG, nB);
         }
      }
   }

   void DrawText (uint8_t* pRgba, int nX, int nY, int nMaxX, const std::string& sText,
                  uint8_t nR, uint8_t nG, uint8_t nB, int& nEndX)
   {
      int nCursor = nX;
      size_t nStart = 0;
      const int nFit = (nMaxX - nX) / kAdvance;
      if (nFit > 0  &&  static_cast<int> (sText.size ()) > nFit)
         nStart = sText.size () - static_cast<size_t> (nFit);

      for (size_t nIx = nStart; nIx < sText.size (); nIx++)
      {
         if (nCursor + kGlyphW * kScale > nMaxX)
            break;
         DrawGlyph (pRgba, nCursor, nY, sText[nIx], nR, nG, nB);
         nCursor += kAdvance;
      }
      nEndX = nCursor;
   }

   bool InRect (float dU, float dV, int nX, int nY, int nW, int nH)
   {
      float dPx = dU * static_cast<float> (kChromeW);
      float dPy = dV * static_cast<float> (kChromeH);
      return dPx >= static_cast<float> (nX)  &&  dPx < static_cast<float> (nX + nW)
          && dPy >= static_cast<float> (nY)  &&  dPy < static_cast<float> (nY + nH);
   }

   void DrawButton (uint8_t* pRgba, int nX, int nY, int nW, int nH, const char* szLabel, bool bHot)
   {
      uint8_t nR = bHot ? 0x3C : 0x2A;
      uint8_t nG = bHot ? 0x6A : 0x2E;
      uint8_t nB = bHot ? 0x4A : 0x32;
      FillRect (pRgba, nX, nY, nW, nH, nR, nG, nB);
      FillRect (pRgba, nX, nY, nW, 2, 0x8A, 0xB4, 0xF8);
      FillRect (pRgba, nX, nY + nH - 2, nW, 2, 0x8A, 0xB4, 0xF8);
      FillRect (pRgba, nX, nY, 2, nH, 0x8A, 0xB4, 0xF8);
      FillRect (pRgba, nX + nW - 2, nY, 2, nH, 0x8A, 0xB4, 0xF8);

      int nLabel = 0;
      while (szLabel[nLabel])
         nLabel++;
      int nTextW = nLabel * kAdvance;
      int nTextX = nX + (nW - nTextW) / 2;
      int nTextY = nY + (nH - kGlyphH * kScale) / 2;
      int nEndX = nTextX;
      DrawText (pRgba, nTextX, nTextY, nX + nW - 4, szLabel, 0xE8, 0xEA, 0xED, nEndX);
   }

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
   std::mutex   m_mx;
   std::string  m_sUrl;
   bool         m_bReady        = false;
   bool         m_bFocused      = false;
   bool         m_bToolbar      = true;
   bool         m_bPassthrough  = false;
   bool         m_bWantKeyboard = false;
   bool         m_bWantPass     = false;

   bool Initialize ()
   {
      bool bOk = true;
      m_bReady = true;
      SetUrl (m_sUrl);
      return bOk;
   }

   void Shutdown ()
   {
      m_bReady = false;
      m_bFocused = false;
   }

   void SetUrl (const std::string& sUrl, bool bSyncIme = true)
   {
      {
         std::lock_guard<std::mutex> lock (m_mx);
         m_sUrl = sUrl;
         if (bSyncIme)
            m_bFocused = false;
      }

      if (bSyncIme)
         AndroidSetUrlText (sUrl);
   }

   void Focus ()
   {
      std::lock_guard<std::mutex> lock (m_mx);
      m_bFocused = true;
   }

   void ShowToolbar ()
   {
      std::lock_guard<std::mutex> lock (m_mx);
      m_bToolbar = true;
   }

   void Passthrough (bool bOn)
   {
      std::lock_guard<std::mutex> lock (m_mx);
      m_bPassthrough = bOn;
   }

   bool ConsumeKeyboard ()
   {
      std::lock_guard<std::mutex> lock (m_mx);
      bool bWant = m_bWantKeyboard;
      m_bWantKeyboard = false;
      return bWant;
   }

   bool ConsumePassthrough ()
   {
      std::lock_guard<std::mutex> lock (m_mx);
      bool bWant = m_bWantPass;
      m_bWantPass = false;
      return bWant;
   }

   void Tick (SNEEZE::ENGINE* pEngine)
   {
      if (pEngine  &&  pEngine->XrRuntime ())
      {
         const int nUiX = 8;
         const int nUiY = 8;
         const int nUiW = 72;
         const int nUiH = 48;
         const int nUrlX = 88;
         const int nUrlY = 10;
         const int nUrlW = 308;
         const int nUrlH = 44;
         const int nArX = 404;
         const int nArY = 10;
         const int nArW = 100;
         const int nArH = 44;

         const float kAnchorLeft = -0.52f;
         const float kViewY = 0.32f;
         const float kViewZ = -1.15f;
         const float kFullW = 1.10f;
         const float kBtnW = 0.18f;
         const float kBarH = 0.13f;

         std::string sUrl;
         bool bFocused = false;
         bool bToolbar = true;
         bool bPass = false;
         {
            std::lock_guard<std::mutex> lock (m_mx);
            sUrl = m_sUrl;
            bFocused = m_bFocused;
            bToolbar = m_bToolbar;
            bPass = m_bPassthrough;
         }

         float dU = 0.0f;
         float dV = 0.0f;
         bool bHover = pEngine->XrRuntime ()->ChromePointer (dU, dV);
         float dClickU = 0.0f;
         float dClickV = 0.0f;
         if (pEngine->XrRuntime ()->ConsumeChromeClick (dClickU, dClickV))
         {
            std::lock_guard<std::mutex> lock (m_mx);
            if (!m_bToolbar)
               m_bToolbar = true;
            else if (InRect (dClickU, dClickV, nUiX, nUiY, nUiW, nUiH))
               m_bToolbar = false;
            else if (InRect (dClickU, dClickV, nArX, nArY, nArW, nArH))
               m_bWantPass = true;
            else if (InRect (dClickU, dClickV, nUrlX, nUrlY, nUrlW, nUrlH))
            {
               m_bFocused = true;
               m_bWantKeyboard = true;
            }
            bToolbar = m_bToolbar;
            bFocused = m_bFocused;
         }

         std::vector<uint8_t> aRgba (static_cast<size_t> (kChromeW) * kChromeH * 4u, 0);
         FillRect (aRgba.data (), 0, 0, kChromeW, kChromeH, 0x20, 0x21, 0x24);

         if (!bToolbar)
         {
            bool bHot = bHover;
            DrawButton (aRgba.data (), 16, 8, kChromeW - 32, 48, "UI", bHot);
         }
         else
         {
            bool bUiHot = bHover  &&  InRect (dU, dV, nUiX, nUiY, nUiW, nUiH);
            bool bArHot = bPass  ||  (bHover  &&  InRect (dU, dV, nArX, nArY, nArW, nArH));
            bool bUrlHot = bFocused  ||  (bHover  &&  InRect (dU, dV, nUrlX, nUrlY, nUrlW, nUrlH));
            DrawButton (aRgba.data (), nUiX, nUiY, nUiW, nUiH, "UI", bUiHot  ||  bToolbar);
            DrawButton (aRgba.data (), nArX, nArY, nArW, nArH, bPass ? "VR" : "AR", bArHot);

            if (bUrlHot)
               FillRect (aRgba.data (), nUrlX, nUrlY, nUrlW, nUrlH, 0x3C, 0x40, 0x43);
            else
               FillRect (aRgba.data (), nUrlX, nUrlY, nUrlW, nUrlH, 0x30, 0x31, 0x34);

            uint8_t nBr = bUrlHot ? 0x8A : 0x5F;
            uint8_t nBg = bUrlHot ? 0xB4 : 0x63;
            uint8_t nBb = bUrlHot ? 0xF8 : 0x68;
            FillRect (aRgba.data (), nUrlX, nUrlY, nUrlW, 2, nBr, nBg, nBb);
            FillRect (aRgba.data (), nUrlX, nUrlY + nUrlH - 2, nUrlW, 2, nBr, nBg, nBb);
            FillRect (aRgba.data (), nUrlX, nUrlY, 2, nUrlH, nBr, nBg, nBb);
            FillRect (aRgba.data (), nUrlX + nUrlW - 2, nUrlY, 2, nUrlH, nBr, nBg, nBb);

            const int nTextX = nUrlX + 12;
            const int nTextY = nUrlY + (nUrlH - kGlyphH * kScale) / 2;
            const int nTextMax = nUrlX + nUrlW - 8;
            const bool bHint = sUrl.empty ();
            std::string sDraw = bHint ? std::string ("Enter a URL") : sUrl;
            int nEndX = nTextX;
            if (bHint)
               DrawText (aRgba.data (), nTextX, nTextY, nTextMax, sDraw, 0x9A, 0xA0, 0xA6, nEndX);
            else
               DrawText (aRgba.data (), nTextX, nTextY, nTextMax, sDraw, 0xE8, 0xEA, 0xED, nEndX);

            if (bFocused)
            {
               auto nMs = std::chrono::duration_cast<std::chrono::milliseconds> (
                  std::chrono::steady_clock::now ().time_since_epoch ()).count ();
               if ((nMs / 400) % 2 == 0)
                  FillRect (aRgba.data (), nEndX + 1, nTextY, 2, kGlyphH * kScale, 0xE8, 0xEA, 0xED);
            }
         }

         float dW = bToolbar ? kFullW : kBtnW;
         float dCx = kAnchorLeft + dW * 0.5f;
         pEngine->XrRuntime ()->SetChromeLayout (dCx, kViewY, kViewZ, dW, kBarH);
         pEngine->XrRuntime ()->SetChromePixels (aRgba.data (), kChromeW, kChromeH);
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
void CHROME_XR::ShowToolbar ()                { m_pImpl->ShowToolbar (); }
void CHROME_XR::Passthrough (bool bOn)        { m_pImpl->Passthrough (bOn); }
bool CHROME_XR::ConsumeKeyboard ()            { return m_pImpl->ConsumeKeyboard (); }
bool CHROME_XR::ConsumePassthrough ()         { return m_pImpl->ConsumePassthrough (); }
void CHROME_XR::Tick (SNEEZE::ENGINE* pEngine) { m_pImpl->Tick (pEngine); }

} // namespace RUBIDIUM

#endif // __ANDROID__
