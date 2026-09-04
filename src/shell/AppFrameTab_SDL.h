// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// SDL per-tab state -- the cross-platform analogue of APPFRAMETAB_NATIVE
// (Win32). Owns one tab's CANVAS (Filament surface), Sneeze context +
// viewport, and RmlUi inspector. The Linux APPFRAME_SDL tab manager owns an
// array of these; on macOS / iOS / Android exactly one is created and its
// canvas reuses the host window directly.

#include "inspector/InspectorRml.h"
#include "settings_ui/SettingsRml.h"

#include <SDL3/SDL_events.h>

#ifndef RUBIDIUM_SHELL_APPFRAMETAB_SDL_H
#define RUBIDIUM_SHELL_APPFRAMETAB_SDL_H

namespace RUBIDIUM
{
   class APPFRAMETAB_SDL
   {
   public:
      APPFRAMETAB_SDL (SNEEZE::ENGINE* pSneeze, LOGGER* pLogger);
      ~APPFRAMETAB_SDL ();

      // pParentNative is the reparent target: an X11 Window (cast to void*)
      // on Linux, or the host SDL_Window* on macOS / iOS / Android.
      bool Init (void* pParentNative, int nContentTop, int nWidth, int nHeight, SNEEZE::CONTEXT::eSESSION eSession);

      void ProcessInput ();
      void Show (bool bShow);
      void Resize (int nWidth, int nHeight, int nContentTop);

      // Forward KEY_DOWN/KEY_UP to the canvas movement-key tracker.
      void ApplyMovementKeyEvent (SDL_Event& Event);

      // While blocked, per-frame canvas input is not forwarded to the viewport
      // (the 3D scene stops responding). Driven by APPFRAME_SDL::ProcessInput
      // to enforce modality of the Settings / "Release Notes" popups on backends
      // where SDL windowing modality is a no-op (e.g. Wayland).
      void BlockInput (bool bBlocked);

      void ViewportAttach ();
      void ViewportDetach ();

      void ToggleInspector ();
      void ShowSettings (void* pOwner);
      bool IsSettingsOpen () const;

      // Accessors
      const std::string& Title () const;
      const std::string& Url   () const;
      CANVAS*            Canvas () const;

      // Modifiers
      void Title  (const std::string& sTitle);
      void Url    (const std::string& sUrl);
      void Reload (bool bReset);
      void Passthrough (bool bPassthrough);
      void TrackingRotation (double dQx, double dQy, double dQz, double dQw);
      void TrackingFovY     (double dFovY);

   private:
      LOGGER*                    m_pLogger;
      SNEEZE::ENGINE*            m_pSneeze;
      INSPECTOR_RML*             m_pInspectorRml;
      SETTINGS_RML*              m_pSettingsRml;
      CANVAS*                    m_pCanvas;
      SNEEZE::CONTEXT*           m_pContext;
      SNEEZE::VIEWPORT*          m_pViewport;
      SNEEZE::ICONTEXT*          m_pCtxHost;
      SNEEZE::IVIEWPORT*         m_pVPHost;

      std::string                m_sTitle;
      std::string                m_sUrl;
      SNEEZE::CONTEXT::eSESSION  m_eSession;
      bool                       m_bActive;
      bool                       m_bInputBlocked;
      bool                       m_bPassthrough;
      int                        m_nContentTop;

      bool CreateContext (bool bReset = false);
      void DestroyContext ();
   };
}

#endif // RUBIDIUM_SHELL_APPFRAMETAB_SDL_H
