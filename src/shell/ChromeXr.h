// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// Head-locked Quest URL bar: CPU-drawn into the OpenXR quad swapchain.
// Horizon's system keyboard is summoned via a 1x1 MainActivity EditText.

#ifndef RUBIDIUM_SHELL_CHROMEXR_H
#define RUBIDIUM_SHELL_CHROMEXR_H

#include <string>

namespace SNEEZE { class ENGINE; }

namespace RUBIDIUM
{

class CHROME_XR
{
public:
   static CHROME_XR& GetInstance ();

   bool Initialize ();
   void Shutdown ();
   void SetUrl (const std::string& sUrl, bool bSyncIme = true);
   void Focus ();
   void Tick (SNEEZE::ENGINE* pEngine);

private:
   CHROME_XR ();
   ~CHROME_XR ();

   class Impl;
   Impl* m_pImpl;
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_SHELL_CHROMEXR_H
