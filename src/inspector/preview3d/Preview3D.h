// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_PREVIEW3D_H
#define RUBIDIUM_INSPECTOR_PREVIEW3D_H

// LOGGER lives in the global namespace (see logger/Logger.h, force-included via
// the precompiled header); CANVAS is RUBIDIUM::CANVAS from canvas/Canvas.h.
class LOGGER;

namespace RUBIDIUM
{
   class CANVAS;

   // A live 3D preview of a glTF/GLB asset. It drives a dedicated Sneeze
   // context/viewport that renders the model into a borderless native-surface
   // child window docked over a region of the inspector window. Halogen only
   // produces pixels through a native window surface (its CPU-readback path is
   // non-functional), so this reuses CANVAS_NATIVE -- the very same
   // native-surface child window the main browser tab uses -- rather than any
   // offscreen readback. One instance is owned by INSPECTOR_RML and reused
   // across selections; the model is replaced on each LoadGlb.
   class PREVIEW3D
   {
   public:
      explicit PREVIEW3D (LOGGER* pLogger);
      ~PREVIEW3D ();

      // Opens the (empty-URL, transitory) preview context and creates the child
      // render window reparented under the inspector window. pParentNative is
      // the handle a CANVAS_NATIVE reparents under (RMLUI_SDL::ParentNativeHandle).
      // Idempotent -- a second call is a no-op once initialized.
      bool Initialize (SNEEZE::ENGINE* pEngine, void* pParentNative);

      // Feed accumulated child-window input to the viewport (orbit the model)
      // and re-assert docking. Call once per frame while shown.
      void Tick ();

      // Loads glb/gltf bytes as the preview's model (replacing any prior one).
      // Does NOT change visibility -- call Show ()/Hide () for that. Returns true
      // when at least one drawable primitive was produced.
      bool LoadGlb (const uint8_t* pData, size_t nLen);

      // Show / hide the docked preview window and start / stop its rendering.
      // Show activates the viewport (Halogen renders on the compositor thread);
      // Hide deactivates it and hides the window. Both are idempotent.
      void Show ();
      void Hide ();

      bool IsInitialized () const { return m_pContext != nullptr; }
      bool IsShown       () const { return m_bShown; }

      // Dock the child window over a rectangle expressed in the inspector's
      // RmlUi coordinate space (an element's AbsoluteOffset + border-box size),
      // which is the same coordinate space SDL_SetWindowPosition uses for this
      // window, so the rect is forwarded straight through (clamped to sane
      // minimums). Call every frame while shown to track scroll / resize.
      void SetGeometry (int nX, int nY, int nWidth, int nHeight);

   private:
      LOGGER*            m_pLogger;
      SNEEZE::ENGINE*    m_pEngine;
      CANVAS*            m_pCanvas;
      SNEEZE::CONTEXT*   m_pContext;
      SNEEZE::VIEWPORT*  m_pViewport;
      SNEEZE::ICONTEXT*  m_pCtxHost;
      SNEEZE::IVIEWPORT* m_pVPHost;

      bool               m_bShown;
      int                m_nGeomX;
      int                m_nGeomY;
      int                m_nGeomW;
      int                m_nGeomH;
   };
}

#endif // RUBIDIUM_INSPECTOR_PREVIEW3D_H
