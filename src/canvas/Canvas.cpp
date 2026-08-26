// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "Canvas.h"

using namespace RUBIDIUM;

CANVAS::CANVAS (LOGGER* pLogger, std::string sName) :
   nMouseX (0), nMouseY (0),
   nMouseDX (0), nMouseDY (0),
   m_sName (std::move (sName)),
   bMouseLeft (false), bMouseRight (false),
   dScrollY (0.0f),
   bKeySpace (false), bKeyPlus (false), bKeyMinus (false), bKeySlash (false),
   bKeyA (false), bKeyS (false), bKeyD (false), bKeyW (false),
   bKeyCtrl (false),
   m_pWindow (nullptr),
   m_pSDLRenderer (nullptr),
   m_pTexture (nullptr),
   m_nWidth (0), m_nHeight (0),
   m_pLogger (pLogger),
   m_bOwnsWindow (false),
   m_nWidth_Pending (0), m_nHeight_Pending (0),
   m_nChildX (0), m_nChildY (0), m_nChildW (-1), m_nChildH (-1),
   m_bPrevPlus (false), m_bPrevMinus (false), m_bPrevSlash (false)
{
}

bool CANVAS::Initialize (void* pParentHandle, int nWidth, int nHeight)
{
   bool bResult = false;

   m_nWidth  = m_nWidth_Pending  = nWidth;
   m_nHeight = m_nHeight_Pending = nHeight;

   return bResult;
}

CANVAS::~CANVAS ()
{
   APPNATIVE::GetInstance ()->SDLWindow_Unregister (this);

   if (m_pTexture)
   {
      SDL_DestroyTexture (m_pTexture);
      m_pTexture = nullptr;
   }
   if (m_pSDLRenderer)
   {
      SDL_DestroyRenderer (m_pSDLRenderer);
      m_pSDLRenderer = nullptr;
   }
   if (m_pWindow  &&  m_bOwnsWindow)
   {
      SDL_DestroyWindow (m_pWindow);
   }
   m_pWindow = nullptr;
}

bool CANVAS::CreatePresentation ()
{
   bool bResult = false;

   if (!m_pWindow)
   {
   }
   else if (m_pSDLRenderer)
   {
      bResult = true;
   }
   else
   {
      m_pSDLRenderer = SDL_CreateRenderer (m_pWindow, nullptr);
      if (m_pSDLRenderer)
      {
         SDL_SetRenderVSync (m_pSDLRenderer, 0);

         m_pTexture = SDL_CreateTexture (m_pSDLRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, m_nWidth, m_nHeight);
         if (m_pTexture)
         {
            bResult = true;
         }
         else
         {
            m_pLogger->Log (LOGGER::kLOGLEVEL_Error, "CANVAS", std::string ("SDL_CreateTexture failed: ") + SDL_GetError ());
            SDL_DestroyRenderer (m_pSDLRenderer);
            m_pSDLRenderer = nullptr;
         }
      }
      else m_pLogger->Log (LOGGER::kLOGLEVEL_Error, "CANVAS", std::string ("SDL_CreateRenderer failed: ") + SDL_GetError ());
   }

   return bResult;
}

/*
#elif defined(__ANDROID__)       SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER
#elif defined(RUBIDIUM_IOS)       SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER
#elif defined(__APPLE__)         SDL_PROP_WINDOW_COCOA_WINDOW_POINTER
*/
void* CANVAS::NativeWindowHandle () const
{
   void* pResult = nullptr;

   if (m_pWindow)
   {
      SDL_PropertiesID nProps = SDL_GetWindowProperties (m_pWindow);

      pResult = SDL_GetPointerProperty (nProps, m_sName.c_str (), nullptr);

      if (pResult == nullptr && m_sName.compare (SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER) == 0)
      {
         Uint64 nX11 = SDL_GetNumberProperty (nProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
         pResult = reinterpret_cast<void*> (static_cast<uintptr_t> (nX11));
      }
   }

   return pResult;
}

void CANVAS::HandleEvent (SDL_Event& Event)
{
   switch (Event.type)
   {
      case SDL_EVENT_MOUSE_MOTION:
         nMouseX  = static_cast<int> (Event.motion.x);
         nMouseY  = static_cast<int> (Event.motion.y);
         nMouseDX = static_cast<int> (Event.motion.xrel);
         nMouseDY = static_cast<int> (Event.motion.yrel);
         break;

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
         if (Event.button.button == SDL_BUTTON_LEFT)  bMouseLeft = true;
         if (Event.button.button == SDL_BUTTON_RIGHT) bMouseRight = true;
         break;

      case SDL_EVENT_MOUSE_BUTTON_UP:
         if (Event.button.button == SDL_BUTTON_LEFT)  bMouseLeft = false;
         if (Event.button.button == SDL_BUTTON_RIGHT) bMouseRight = false;
         break;

      case SDL_EVENT_MOUSE_WHEEL:
         dScrollY = Event.wheel.y;
         break;

      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_KEY_UP:
         ApplyMovementKeyEvent (Event);
         break;

      case SDL_EVENT_WINDOW_EXPOSED:
         // When the embedded child is re-exposed (restore from minimize, leave
         // fullscreen, uncover), SDL may have re-applied its stale position. A
         // non-live-resize expose (data1 == 0) re-asserts the docked geometry;
         // this fires correlated with SDL's own move, so it wins the race that a
         // host-window-driven relayout can lose. Live-resize exposes (data1 == 1)
         // are already handled by the manager's RESIZED relayout.
         if (Event.window.data1 == 0)
            ApplyChildGeometry ();
         break;

      default:
         break;
   }
}

void CANVAS::ApplyMovementKeyEvent (SDL_Event& Event)
{
   bool bDown = (Event.type == SDL_EVENT_KEY_DOWN);

   if (Event.key.key == SDLK_SPACE)  bKeySpace = bDown;
   if (Event.key.key == SDLK_EQUALS  ||  Event.key.key == SDLK_KP_PLUS)     bKeyPlus  = bDown;
   if (Event.key.key == SDLK_MINUS   ||  Event.key.key == SDLK_KP_MINUS)    bKeyMinus = bDown;
   if (Event.key.key == SDLK_SLASH   ||  Event.key.key == SDLK_KP_DIVIDE)   bKeySlash = bDown;
   if (Event.key.key == SDLK_A)      bKeyA     = bDown;
   if (Event.key.key == SDLK_S)      bKeyS     = bDown;
   if (Event.key.key == SDLK_D)      bKeyD     = bDown;
   if (Event.key.key == SDLK_W)      bKeyW     = bDown;
   if (Event.key.key == SDLK_LCTRL  ||  Event.key.key == SDLK_RCTRL)       bKeyCtrl  = bDown;
}

void CANVAS::ProcessInput (SNEEZE::VIEWPORT* pViewport)
{
   // Continuous movement keys. On Win32 the canvas HWND is WS_EX_NOACTIVATE and
   // the frame is a native (non-SDL) window, so no SDL window holds keyboard
   // focus -- SDL_GetKeyboardState never sees A/S/D/W. Poll Win32 async key
   // state instead. On SDL platforms (Linux/macOS) poll SDL_GetKeyboardState
   // the same way -- the chrome window holds focus but SDL still tracks physical
   // key state application-wide. Suppress while a text field is capturing input
   // so the URL bar can type.
   bool bCaptureKeys = true;

#ifdef _WIN32
   {
      HWND hFocus = GetFocus ();
      if (hFocus)
      {
         wchar_t wszClass[64] = {};
         if (GetClassNameW (hFocus, wszClass, 64) > 0
             &&  _wcsicmp (wszClass, L"Edit") == 0)
            bCaptureKeys = false;
      }
   }

   if (bCaptureKeys)
   {
      bKeyA     = (GetAsyncKeyState ('A') & 0x8000) != 0;
      bKeyS     = (GetAsyncKeyState ('S') & 0x8000) != 0;
      bKeyD     = (GetAsyncKeyState ('D') & 0x8000) != 0;
      bKeyW     = (GetAsyncKeyState ('W') & 0x8000) != 0;
      bKeySpace = (GetAsyncKeyState (VK_SPACE) & 0x8000) != 0;
      bKeyCtrl  = (GetAsyncKeyState (VK_CONTROL) & 0x8000) != 0;
      bKeyPlus  = ((GetAsyncKeyState (VK_OEM_PLUS)  & 0x8000) != 0)  ||  ((GetAsyncKeyState (VK_ADD)      & 0x8000) != 0);
      bKeyMinus = ((GetAsyncKeyState (VK_OEM_MINUS) & 0x8000) != 0)  ||  ((GetAsyncKeyState (VK_SUBTRACT) & 0x8000) != 0);
      bKeySlash = ((GetAsyncKeyState (VK_OEM_2)     & 0x8000) != 0)  ||  ((GetAsyncKeyState (VK_DIVIDE)   & 0x8000) != 0);
   }
   else
   {
      bKeyA     = false;
      bKeyS     = false;
      bKeyD     = false;
      bKeyW     = false;
      bKeySpace = false;
      bKeyCtrl  = false;
      bKeyPlus  = false;
      bKeyMinus = false;
      bKeySlash = false;
   }
#else
   if (APPNATIVE::GetInstance ()->MovementKeysSuppressed ())
   {
      bKeyA     = false;
      bKeyS     = false;
      bKeyD     = false;
      bKeyW     = false;
      bKeySpace = false;
      bKeyCtrl  = false;
      bKeyPlus  = false;
      bKeyMinus = false;
      bKeySlash = false;
   }
   else
   {
      const bool* aKeys = SDL_GetKeyboardState (nullptr);

      bKeyA     = aKeys[SDL_SCANCODE_A];
      bKeyS     = aKeys[SDL_SCANCODE_S];
      bKeyD     = aKeys[SDL_SCANCODE_D];
      bKeyW     = aKeys[SDL_SCANCODE_W];
      bKeySpace = aKeys[SDL_SCANCODE_SPACE];
      bKeyCtrl  = aKeys[SDL_SCANCODE_LCTRL]  ||  aKeys[SDL_SCANCODE_RCTRL];
      bKeyPlus  = aKeys[SDL_SCANCODE_EQUALS]  ||  aKeys[SDL_SCANCODE_KP_PLUS];
      bKeyMinus = aKeys[SDL_SCANCODE_MINUS]   ||  aKeys[SDL_SCANCODE_KP_MINUS];
      bKeySlash = aKeys[SDL_SCANCODE_SLASH]   ||  aKeys[SDL_SCANCODE_KP_DIVIDE];
   }
#endif

   // Movement speed: '+' steps the WASD travel distance up one notch, '-' down,
   // '/' resets to the default. Rising-edge latched so a held key advances one
   // notch per press. The position lives in settings (shared with the Movement
   // settings slider); the resulting scale is pushed to the engine every frame.
   APP* pApp    = APPNATIVE::GetInstance ();
   int  nSpeed  = pApp->MovementSpeedPosition ();

   if (bKeyPlus   &&  !m_bPrevPlus)   pApp->MovementSpeedPosition (nSpeed + 1);
   if (bKeyMinus  &&  !m_bPrevMinus)  pApp->MovementSpeedPosition (nSpeed - 1);
   if (bKeySlash  &&  !m_bPrevSlash)  pApp->MovementSpeedPosition (APP::kMovementSpeedDefault);

   m_bPrevPlus  = bKeyPlus;
   m_bPrevMinus = bKeyMinus;
   m_bPrevSlash = bKeySlash;

   pViewport->Input_Mouse     (nMouseDX, nMouseDY, dScrollY, bMouseLeft, bMouseRight);
   pViewport->Input_Key       (bKeySpace, bKeyPlus, bKeyMinus, bKeyA, bKeyS, bKeyD, bKeyW, bKeyCtrl);
   pViewport->Input_MoveScale (pApp->MovementScale ());

   nMouseDX = 0;
   nMouseDY = 0;
   dScrollY = 0.0f;
}

void CANVAS::Present (const uint32_t* pPixels, int nWidth, int nHeight)
{
   std::lock_guard<std::mutex> guard (m_textureMutex);

   if (m_pTexture  &&  pPixels  &&  nWidth == m_nWidth  &&  nHeight == m_nHeight)
   {
      SDL_UpdateTexture (m_pTexture, nullptr, pPixels, nWidth * sizeof (uint32_t));
      SDL_RenderClear (m_pSDLRenderer);
      SDL_RenderTexture (m_pSDLRenderer, m_pTexture, nullptr, nullptr);

      if (m_fnOverlay)
         m_fnOverlay (m_pSDLRenderer);

      SDL_RenderPresent (m_pSDLRenderer);
   }
}

void CANVAS::SetOverlay (std::function<void(SDL_Renderer*)> fn)
{
   std::lock_guard<std::mutex> guard (m_textureMutex);
   m_fnOverlay = std::move (fn);
}

void CANVAS::Resize (int nWidth, int nHeight)
{
   std::lock_guard<std::mutex> guard(m_textureMutex);

   if (nWidth > 0  &&  nHeight > 0)
   {
      m_nWidth_Pending  = nWidth;
      m_nHeight_Pending = nHeight;
   }
}

bool CANVAS::FrameSize (int& nWidth, int& nHeight)
{
   bool bResult = false;

   {
      std::lock_guard<std::mutex> guard (m_textureMutex);

      if (nWidth != m_nWidth_Pending  ||  nHeight != m_nHeight_Pending)
      {
         nWidth  = m_nWidth  = m_nWidth_Pending;
         nHeight = m_nHeight = m_nHeight_Pending;

         if (m_pTexture)
         {
            SDL_DestroyTexture (m_pTexture);
            m_pTexture = nullptr;
         }
   
         if (m_pSDLRenderer)
            m_pTexture = SDL_CreateTexture (m_pSDLRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, m_nWidth, m_nHeight);

         bResult = true;
      }
   }

   if (bResult  &&  m_pWindow)
      SDL_SetWindowSize (m_pWindow, m_nWidth, m_nHeight);

   return bResult;
}

void CANVAS::SetChildGeometry (int nX, int nY, int nWidth, int nHeight)
{
   if (nWidth > 0  &&  nHeight > 0)
   {
      m_nChildX = nX;
      m_nChildY = nY;
      m_nChildW = nWidth;
      m_nChildH = nHeight;

      ApplyChildGeometry ();
   }

   Resize (nWidth, nHeight);
}

void CANVAS::ApplyChildGeometry ()
{
}

void CANVAS::RaiseChild ()
{
}

SDL_WindowID CANVAS::SDLWindowID () const
{
   return m_pWindow ? SDL_GetWindowID (m_pWindow) : 0;
}

SDL_Renderer* CANVAS::Renderer () const
{
   return m_pSDLRenderer;
}

/*
Initialize ()
{
   m_pWindow = static_cast<SDL_Window*> (pParentHandle);
   m_bOwnsWindow = false;
   if (m_pWindow)
   {
      APPNATIVE::GetInstance ()->SDLWindow_Register (this);
      bResult = true;
   }
   else m_pLogger->Log (LOGGER::kLOGLEVEL_Error, "CANVAS", "No parent window provided");
}
*/