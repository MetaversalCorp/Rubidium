// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "shell/UrlBarRml.h"

#include "rmlui_sdl/RmlUi_SDL_Renderer.h"
#include "rmlui_sdl/RmlUi_SDL_Platform.h"

namespace RUBIDIUM
{

static constexpr int kBarHeight = 44;

static constexpr const char* g_szDocument = R"RML(
<rml>
<head>
   <style>
      body {
         background-color: rgb(238, 238, 238);
         color: rgb(40, 40, 40);
         font-family: "{{FONT_FAMILY}}";
         font-size: 14dp;
         padding: 6dp 12dp;
         box-sizing: border-box;
         width: 100%;
         height: 100%;
      }
      input.url {
         display: block;
         box-sizing: border-box;
         width: 100%;
         height: 28dp;
         padding: 4dp 8dp;
         background-color: rgb(255, 255, 255);
         border: 1dp rgb(180, 180, 180);
         color: rgb(30, 30, 30);
         font-size: 14dp;
      }
      input.url:focus {
         border-color: rgb(80, 140, 220);
      }
   </style>
</head>
<body>
   <input type="text" id="url" class="url" value="{{INITIAL_URL}}"/>
</body>
</rml>
)RML";

#ifdef _WIN32
static const char* const g_apszFonts[] = {
   "C:\\Windows\\Fonts\\segoeui.ttf",
   "C:\\Windows\\Fonts\\arial.ttf",
   nullptr,
};
static const char* const g_szFontFamily = "Segoe UI";
#elif defined(__APPLE__)
static const char* const g_apszFonts[] = {
   "/System/Library/Fonts/SFNS.ttf",
   "/System/Library/Fonts/SFNSText.ttf",
   "/System/Library/Fonts/Supplemental/Arial.ttf",
   nullptr,
};
static const char* const g_szFontFamily = ".SF NS";
#elif defined(__ANDROID__)
static const char* const g_apszFonts[] = {
   "/system/fonts/Roboto-Regular.ttf",
   "/system/fonts/DroidSans.ttf",
   nullptr,
};
static const char* const g_szFontFamily = "Roboto";
#else
static const char* const g_apszFonts[] = {
   "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
   "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
   "/usr/share/fonts/TTF/DejaVuSans.ttf",
   nullptr,
};
static const char* const g_szFontFamily = "DejaVu Sans";
#endif

class SUBMIT_LISTENER : public Rml::EventListener
{
public:
   SUBMIT_LISTENER (Rml::Element* pInput, std::string* psUrl, URL_BAR_RML::OnSubmitFn fn) :
      m_pInput (pInput),
      m_psUrl  (psUrl),
      m_fn     (std::move (fn))
   {
   }

   void ProcessEvent (Rml::Event& event) override
   {
      if (event.GetId () == Rml::EventId::Keydown)
      {
         auto eKey = static_cast<Rml::Input::KeyIdentifier> (
            event.GetParameter<int> ("key_identifier", Rml::Input::KI_UNKNOWN));
         if (eKey != Rml::Input::KI_RETURN && eKey != Rml::Input::KI_NUMPADENTER)
            return;
      }

      Rml::String sValue = m_pInput->GetAttribute<Rml::String> ("value", "");
      *m_psUrl = std::string (sValue.c_str ());
      if (m_fn)
         m_fn (*m_psUrl);
   }

private:
   Rml::Element*            m_pInput;
   std::string*             m_psUrl;
   URL_BAR_RML::OnSubmitFn  m_fn;
};

struct URL_BAR_RML::Impl
{
   SDL_Window*           pWindow         = nullptr;
   SDL_Renderer*         pRenderer       = nullptr;
   RenderInterface_SDL*  pRmlRenderer    = nullptr;
   Rml::Context*         pContext        = nullptr;
   Rml::ElementDocument* pDocument       = nullptr;
   Rml::Element*         pInput          = nullptr;
   SUBMIT_LISTENER*      pListenerChange = nullptr;
   SUBMIT_LISTENER*      pListenerKey    = nullptr;
   std::string           sUrl;
   int                   nWidth          = 0;
   std::mutex            mutex;
};

URL_BAR_RML::URL_BAR_RML () :
   m_pImpl (nullptr)
{
}

URL_BAR_RML::~URL_BAR_RML ()
{
   Shutdown ();
}

bool URL_BAR_RML::Initialize (SDL_Window* pParentWindow, SDL_Renderer* pParentRenderer,
                              int nWidth, const std::string& sUrl, OnSubmitFn fnOnSubmit)
{
   bool bResult = false;

   if (!pParentWindow || !pParentRenderer)
      return false;

   m_pImpl = new Impl ();
   m_pImpl->pWindow   = pParentWindow;
   m_pImpl->pRenderer = pParentRenderer;
   m_pImpl->sUrl      = sUrl;
   m_pImpl->nWidth    = nWidth;

   m_pImpl->pRmlRenderer = new RenderInterface_SDL (pParentRenderer);
   m_pImpl->pRmlRenderer->SetClearColor (0xee, 0xee, 0xee);

   std::string sFamily;
   for (const char* const* p = g_apszFonts; *p; ++p)
   {
      if (Rml::LoadFontFace (*p, true))
      {
         sFamily = g_szFontFamily;
         break;
      }
   }

   m_pImpl->pContext = Rml::CreateContext ("urlbar",
      Rml::Vector2i (nWidth, kBarHeight), m_pImpl->pRmlRenderer);

   if (m_pImpl->pContext && !sFamily.empty ())
   {
      std::string sDocument (g_szDocument);
      auto fnReplace = [&](const std::string& sPlaceholder, const std::string& sValue)
      {
         size_t nPos = 0;
         while ((nPos = sDocument.find (sPlaceholder, nPos)) != std::string::npos)
            sDocument.replace (nPos, sPlaceholder.length (), sValue);
      };
      fnReplace ("{{FONT_FAMILY}}", sFamily);
      fnReplace ("{{INITIAL_URL}}", m_pImpl->sUrl);

      m_pImpl->pDocument = m_pImpl->pContext->LoadDocumentFromMemory (sDocument);
      if (m_pImpl->pDocument)
      {
         m_pImpl->pInput = m_pImpl->pDocument->GetElementById ("url");
         if (m_pImpl->pInput)
         {
            m_pImpl->pListenerChange = new SUBMIT_LISTENER (m_pImpl->pInput, &m_pImpl->sUrl, fnOnSubmit);
            m_pImpl->pListenerKey    = new SUBMIT_LISTENER (m_pImpl->pInput, &m_pImpl->sUrl, fnOnSubmit);
            m_pImpl->pInput->AddEventListener (Rml::EventId::Change,  m_pImpl->pListenerChange);
            m_pImpl->pInput->AddEventListener (Rml::EventId::Keydown, m_pImpl->pListenerKey);
            m_pImpl->pInput->Focus ();
         }

         m_pImpl->pDocument->Show ();
         bResult = true;
      }
   }

   if (!bResult)
      Shutdown ();

   return bResult;
}

void URL_BAR_RML::Shutdown ()
{
   if (!m_pImpl)
      return;

   if (m_pImpl->pInput)
   {
      if (m_pImpl->pListenerChange)
         m_pImpl->pInput->RemoveEventListener (Rml::EventId::Change,  m_pImpl->pListenerChange);
      if (m_pImpl->pListenerKey)
         m_pImpl->pInput->RemoveEventListener (Rml::EventId::Keydown, m_pImpl->pListenerKey);
   }
   delete m_pImpl->pListenerChange;  m_pImpl->pListenerChange = nullptr;
   delete m_pImpl->pListenerKey;     m_pImpl->pListenerKey    = nullptr;

   if (m_pImpl->pContext)
   {
      Rml::RemoveContext ("urlbar");
      m_pImpl->pContext  = nullptr;
      m_pImpl->pDocument = nullptr;
   }

   delete m_pImpl->pRmlRenderer; m_pImpl->pRmlRenderer = nullptr;

   delete m_pImpl;
   m_pImpl = nullptr;
}

void URL_BAR_RML::ProcessEvent (const SDL_Event& ev)
{
   if (!m_pImpl || !m_pImpl->pContext)
      return;

   std::lock_guard<std::mutex> guard (m_pImpl->mutex);
   RmlSDL::InputEventHandler (m_pImpl->pContext, m_pImpl->pWindow, const_cast<SDL_Event&> (ev));
}

void URL_BAR_RML::Render (SDL_Renderer* pRenderer)
{
   if (!m_pImpl || !m_pImpl->pContext || !pRenderer)
      return;

   std::lock_guard<std::mutex> guard (m_pImpl->mutex);

   // Constrain rendering to the top kBarHeight pixels of the framebuffer.
   int nW = 0, nH = 0;
   SDL_GetRenderOutputSize (pRenderer, &nW, &nH);
   SDL_Rect rcBar { 0, 0, nW, kBarHeight };
   SDL_SetRenderViewport (pRenderer, &rcBar);

   if (m_pImpl->nWidth != nW)
   {
      m_pImpl->nWidth = nW;
      m_pImpl->pContext->SetDimensions (Rml::Vector2i (nW, kBarHeight));
   }

   m_pImpl->pContext->Update ();
   m_pImpl->pRmlRenderer->BeginFrame ();
   m_pImpl->pContext->Render ();
   m_pImpl->pRmlRenderer->EndFrame ();

   SDL_SetRenderViewport (pRenderer, nullptr);
}

void URL_BAR_RML::SetUrl (const std::string& sUrl)
{
   if (!m_pImpl)
      return;

   std::lock_guard<std::mutex> guard (m_pImpl->mutex);
   m_pImpl->sUrl = sUrl;
   if (m_pImpl->pInput)
      m_pImpl->pInput->SetAttribute ("value", sUrl);
}

std::string const URL_BAR_RML::GetUrl () const
{
   if (!m_pImpl)
      return std::string ();
   std::lock_guard<std::mutex> guard (m_pImpl->mutex);
   return m_pImpl->sUrl;
}

int URL_BAR_RML::BarHeight () const
{
   return kBarHeight;
}

} // namespace RUBIDIUM
