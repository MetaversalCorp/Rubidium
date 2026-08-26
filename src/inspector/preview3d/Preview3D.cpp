// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/preview3d/Preview3D.h"

using namespace RUBIDIUM;

/*******************************************************************************************************************************
**                                                Class: PREVIEW_VIEWPORT_HOST                                                **
*******************************************************************************************************************************/

// Minimal IVIEWPORT host for the preview canvas, mirroring VIEWPORT_HOST_SDL in
// AppFrameTab_SDL.cpp: hands Sneeze the canvas's native window handle (so
// Halogen takes its native-surface path) and its current framebuffer size.
// OnFrameReady is only reached on the (non-functional) CPU-readback path and
// simply forwards to the canvas, matching the main tab.

namespace
{
   class PREVIEW_VIEWPORT_HOST : public SNEEZE::IVIEWPORT
   {
   public:
      explicit PREVIEW_VIEWPORT_HOST (RUBIDIUM::CANVAS* pCanvas) :
         m_pCanvas (pCanvas)
      {
      }

      void* FrameWindow () override { return m_pCanvas->NativeWindowHandle (); }

      bool FrameSize (int& nWidth, int& nHeight) override
      {
         return m_pCanvas->FrameSize (nWidth, nHeight);
      }

      void OnFrameReady (const uint32_t* pFB, int nFbW, int nFbH) override
      {
         m_pCanvas->Present (pFB, nFbW, nFbH);
      }

   private:
      RUBIDIUM::CANVAS* m_pCanvas;
   };
}

/*******************************************************************************************************************************
**                                                        PREVIEW3D                                                          **
*******************************************************************************************************************************/

PREVIEW3D::PREVIEW3D (LOGGER* pLogger) :
   m_pLogger  (pLogger),
   m_pEngine  (nullptr),
   m_pCanvas  (new CANVAS_NATIVE (pLogger)),
   m_pContext (nullptr),
   m_pViewport(nullptr),
   m_pCtxHost (nullptr),
   m_pVPHost  (nullptr),
   m_bShown   (false),
   m_nGeomX   (0),
   m_nGeomY   (0),
   m_nGeomW   (0),
   m_nGeomH   (0)
{
}

PREVIEW3D::~PREVIEW3D ()
{
   // Same teardown order the main tab uses (DestroyContext then delete canvas):
   // detach the viewport, close the context (tears down the renderer on the
   // compositor thread), then release the canvas window and the hosts.
   if (m_pViewport && m_bShown)
      m_pViewport->Deactivate ();

   if (m_pContext && m_pEngine)
      m_pEngine->Context_Close (m_pContext);

   delete m_pCanvas;

   delete m_pVPHost;
   delete m_pCtxHost;
}

bool PREVIEW3D::Initialize (SNEEZE::ENGINE* pEngine, void* pParentNative)
{
   bool bResult = (m_pContext != nullptr);   // already initialized -> no-op

   if (!bResult && pEngine && pParentNative)
   {
      m_pEngine = pEngine;

      // Placeholder size; the real docking rect arrives through SetGeometry
      // before the window is first shown.
      if (m_pCanvas->Initialize (pParentNative, 320, 240))
      {
         // CANVAS_NATIVE comes up shown (the main tab relies on that); the
         // preview must stay hidden until a glb is actually selected (Show).
         m_pCanvas->SetVisible (false);
         m_pCanvas->SetChildGeometry (0, 0, 320, 240);

         // A preview context needs no inspector callbacks -- the concrete
         // ICONTEXT base (all no-op virtuals) is enough.
         m_pCtxHost = new SNEEZE::ICONTEXT ();
         m_pVPHost  = new PREVIEW_VIEWPORT_HOST (m_pCanvas);

         // Empty URL + transitory: an empty-but-renderable scene, no fabric
         // fetch, its own Halogen device/viewport independent of the tab.
         m_pContext = m_pEngine->Context_Open (m_pCtxHost, "", SNEEZE::CONTEXT::kSESSION_TRANSITORY);

         if (m_pContext)
         {
            m_pViewport = m_pContext->Viewport ();
            bResult = true;
         }
      }

      if (!bResult && m_pLogger)
         m_pLogger->Log (LOGGER::kLOGLEVEL_Warning, "Preview3D", "Failed to initialize preview viewport");
   }

   return bResult;
}

bool PREVIEW3D::LoadGlb (const uint8_t* pData, size_t nLen)
{
   bool bResult = false;

   if (m_pContext && m_pContext->Scene () && pData && nLen > 0)
      bResult = m_pContext->Scene ()->Gltf_Preview (pData, nLen);

   return bResult;
}

void PREVIEW3D::Show ()
{
   if (m_pContext && !m_bShown)
   {
      if (m_nGeomW > 0 && m_nGeomH > 0)
      {
         m_pCanvas->SetChildGeometry (m_nGeomX, m_nGeomY, m_nGeomW, m_nGeomH);

         // Execute_Create reads Viewport::Size before FrameSize; seed the
         // viewport so Halogen initializes at the dock size, not 0x0.
         if (m_pViewport)
            m_pViewport->Resize (m_nGeomW, m_nGeomH);
      }

      m_pCanvas->SetVisible (true);

      if (m_pViewport)
         m_pViewport->Activate (m_pVPHost);

      // Re-assert docking after Activate -- macOS positions the child in screen
      // space and the compositor thread may have raced the first geometry pass.
      if (m_nGeomW > 0 && m_nGeomH > 0)
         m_pCanvas->SetChildGeometry (m_nGeomX, m_nGeomY, m_nGeomW, m_nGeomH);

      // Activate rebuilds the renderer from scratch, which comes up with the
      // default (black) backdrop. The scene pushes its backdrop to the renderer
      // through a consume-once changed flag that was already cleared on the
      // first show (and the glb -- hence Gltf_Preview -- is not reloaded across
      // reopens), so re-assert the stored colour here to re-trip that flag and
      // force the compositor to re-apply it to the new renderer.
      if (m_pContext && m_pContext->Scene ())
         m_pContext->Scene ()->Background (m_pContext->Scene ()->Background ());

      m_bShown = true;
   }
}

void PREVIEW3D::Hide ()
{
   // Drop the native child from the screen immediately. Deactivate() blocks
   // while the compositor tears down Halogen and must not run first -- on macOS
   // the Metal window would stay visible showing the last frame until deactivate
   // returns.
   if (m_pCanvas)
      m_pCanvas->SetVisible (false);

   if (m_pViewport  &&  m_bShown)
      m_pViewport->Deactivate ();

   m_bShown = false;
}

void PREVIEW3D::Tick ()
{
   // Let the user orbit the model by dragging inside the preview window; the
   // canvas has been accumulating input deltas from its own SDL events.
   if (m_bShown && m_pViewport)
   {
      m_pCanvas->ProcessInput (m_pViewport);
      // RmlUi repaints the inspector each input event and can bury the native
      // child -- re-raise after every frame while the preview is active.
      m_pCanvas->RaiseChild ();
   }
}

void PREVIEW3D::SetGeometry (int nX, int nY, int nWidth, int nHeight)
{
   if (nWidth  < 1) nWidth  = 1;
   if (nHeight < 1) nHeight = 1;

   // Only re-dock on an actual change -- SetChildGeometry moves an OS window
   // (XMoveResizeWindow on Linux), so calling it every frame would flicker.
   if (nX != m_nGeomX || nY != m_nGeomY || nWidth != m_nGeomW || nHeight != m_nGeomH)
   {
      m_nGeomX = nX;
      m_nGeomY = nY;
      m_nGeomW = nWidth;
      m_nGeomH = nHeight;

      if (m_bShown)
         m_pCanvas->SetChildGeometry (m_nGeomX, m_nGeomY, m_nGeomW, m_nGeomH);
   }
}
