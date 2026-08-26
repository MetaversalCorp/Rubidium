// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "rmlui_sdl/RmlUi_SDL.h"

#include "settings_ui/SettingsRml.h"
#include "version.h"
#include "Brand.h"

using namespace RUBIDIUM;

/*******************************************************************************************************************************
**                                                        Impl                                                               **
*******************************************************************************************************************************/

class SETTINGS_RML::Impl : public Rml::EventListener
{
public:
   static inline const char* s_sRmlDocument =
R"rml(
<rml>
<head>
<style>
body
{
   font-family: [{FONT-FAMILY}];
   font-size: 13dp;
   background: #ffffff;
   color: #202124;
   margin: 0;
   padding: 0;
   width: 100%;
   height: 100%;
   display: flex;
   flex-direction: column;
}

div#header
{
   display: flex;
   flex-direction: row;
   align-items: center;
   flex-shrink: 0;
   height: 60dp;
   padding-left: 24dp;
   box-sizing: border-box;
}

img#logo
{
   width: 26dp;
   height: 26dp;
   margin-right: 16dp;
}

div#title
{
   font-size: 20dp;
   font-weight: 500;
   color: #202124;
}

div#content
{
   display: flex;
   flex-direction: row;
   flex-grow: 1;
}

div#sidebar
{
   width: 240dp;
   flex-shrink: 0;
   padding-top: 8dp;
   box-sizing: border-box;
}

div.nav-item
{
   display: flex;
   flex-direction: row;
   align-items: center;
   height: 36dp;
   padding-left: 24dp;
   margin-bottom: 4dp;
   color: #444746;
   cursor: pointer;
   border-top-right-radius: 100dp;
   border-bottom-right-radius: 100dp;
}

div.nav-item:hover
{
   background: #F1F3F4;
}

div.nav-item.active
{
   background: #E8F0FE;
   color: #0B57D0;
   font-weight: 500;
}

span.nav-icon
{
   font-family: "Material Symbols Outlined";
   font-size: 18dp;
   margin-right: 16dp;
}

div#main
{
   flex-grow: 1;
   padding: 8dp 24dp 24dp 24dp;
   box-sizing: border-box;
}

div.panel
{
   display: none;
}

div.panel.active
{
   display: block;
}

div.section-title
{
   font-size: 13dp;
   color: #202124;
   padding-top: 16dp;
   padding-bottom: 8dp;
}

div.card
{
   display: flex;
   flex-direction: row;
   align-items: center;
   max-width: 1040dp;
   border: 1px #DADCE0;
   border-radius: 8dp;
   padding: 14dp 24dp;
   box-sizing: border-box;
}

div.card-info
{
   display: flex;
   flex-direction: row;
   align-items: center;
   flex-grow: 1;
}

div.card-label
{
   font-size: 13dp;
   color: #5f6368;
   margin-right: 40dp;
   flex-shrink: 0;
   white-space: nowrap;
}

div.card-value
{
   font-size: 13dp;
   color: #202124;
   flex-grow: 1;
   white-space: nowrap;
}

input.home-input
{
   flex-grow: 1;
   font-size: 13dp;
   color: #202124;
   border: 1px #0B57D0;
   border-radius: 4dp;
   padding: 4dp 6dp;
   box-sizing: border-box;
}

.hidden
{
   display: none;
}

div.card-divider
{
   width: 1px;
   height: 36dp;
   background: #DADCE0;
   margin-left: 16dp;
   margin-right: 16dp;
   flex-shrink: 0;
}

div.card-action
{
   color: #0B57D0;
   font-weight: 500;
   border: 1px #DADCE0;
   border-radius: 100dp;
   padding: 6dp 18dp;
   cursor: pointer;
   flex-shrink: 0;
}

div.card-action:hover
{
   background: #F6FAFE;
   border-color: #C6D7F5;
}

div.about-card
{
   display: block;
   max-width: 1040dp;
   border: 1px #DADCE0;
   border-radius: 8dp;
   box-sizing: border-box;
}

div.about-head
{
   display: flex;
   flex-direction: row;
   align-items: center;
   padding: 18dp 24dp;
}

img.about-logo
{
   width: 36dp;
   height: 36dp;
   margin-right: 18dp;
   flex-shrink: 0;
}

div.about-name
{
   font-size: 17dp;
   color: #202124;
}

div.about-sep
{
   height: 1px;
   background: #E8EAED;
}

div.about-status
{
   display: flex;
   flex-direction: row;
   align-items: center;
   padding: 16dp 24dp;
}

span.about-check
{
   font-family: "Material Symbols Outlined";
   font-size: 20dp;
   color: #0B57D0;
   margin-right: 16dp;
   flex-shrink: 0;
}

div.about-status-title
{
   font-size: 12dp;
   color: #202124;
}

div.about-version
{
   font-size: 11dp;
   color: #5f6368;
   margin-top: 2dp;
}

div.speed-control
{
   display: flex;
   flex-direction: row;
   align-items: center;
   flex-grow: 1;
}

span.speed-end
{
   font-size: 12dp;
   color: #5f6368;
   flex-shrink: 0;
}

div.speed-slider-wrap
{
   display: flex;
   flex-direction: column;
   margin: 0 16dp;
}

input.speed-slider
{
   display: block;
   width: 320dp;
   height: 20dp;
}

input.speed-slider slidertrack
{
   display: block;
   height: 4dp;
   margin-top: 8dp;
   border-radius: 2dp;
   background: #C6D7F5;
}

input.speed-slider sliderbar
{
   display: block;
   width: 16dp;
   height: 16dp;
   margin-top: 2dp;
   border-radius: 8dp;
   background: #0B57D0;
}

input.speed-slider sliderbar:hover
{
   background: #0842A0;
}

input.speed-slider sliderarrowdec, input.speed-slider sliderarrowinc
{
   display: block;
   width: 0;
   height: 0;
}

div.speed-ticks
{
   display: flex;
   flex-direction: row;
   justify-content: space-between;
   width: 320dp;
   margin-top: 4dp;
}

span.tick
{
   display: block;
   width: 1px;
   height: 6dp;
   background: #DADCE0;
}
</style>
</head>
<body>
<div id="header">
   <img id="logo" src="[{LOGO-PATH}]"/>
   <div id="title">Settings</div>
</div>
<div id="content">
   <div id="sidebar">
      <div class="nav-item active" id="nav-startup"><span class="nav-icon">&#xE8AC;</span><span class="nav-label">On startup</span></div>
      <div class="nav-item"        id="nav-movement"><span class="nav-icon">&#xE9E4;</span><span class="nav-label">Movement</span></div>
      <div class="nav-item"        id="nav-about"><span class="nav-icon">&#xE80B;</span><span class="nav-label">About Rubidium</span></div>
   </div>
   <div id="main">
      <div class="panel active" id="panel-startup">
         <div class="section-title">On Startup</div>
         <div class="card">
            <div class="card-info">
               <div class="card-label">Home Page:</div>
               <div class="card-value" id="home-value"></div>
               <input type="text" id="home-input" class="home-input hidden" value=""/>
            </div>
            <div class="card-divider"></div>
            <div class="card-action" id="btn-change">Change</div>
         </div>
      </div>
      <div class="panel" id="panel-movement">
         <div class="section-title">Movement</div>
         <div class="card">
            <div class="card-info">
               <div class="card-label">Speed:</div>
               <div class="speed-control">
                  <span class="speed-end">Slow</span>
                  <div class="speed-slider-wrap">
                     <input type="range" id="speed-input" class="speed-slider" min="1" max="50" step="1" value="25"/>
                     <div class="speed-ticks"><span class="tick"></span><span class="tick"></span><span class="tick"></span><span class="tick"></span><span class="tick"></span><span class="tick"></span><span class="tick"></span><span class="tick"></span><span class="tick"></span><span class="tick"></span><span class="tick"></span></div>
                  </div>
                  <span class="speed-end">Fast</span>
               </div>
            </div>
         </div>
      </div>
      <div class="panel" id="panel-about">
         <div class="section-title">About Rubidium</div>
         <div class="about-card">
            <div class="about-head">
               <img class="about-logo" src="[{LOGO-PATH}]"/>
               <div class="about-name">Rubidium</div>
            </div>
            <div class="about-sep"></div>
            <div class="about-status">
               <div class="about-status-text">
                  <div class="about-version" id="about-version"></div>
               </div>
            </div>
         </div>
      </div>
   </div>
</div>
</body>
</rml>
)rml";

   Impl () :
      m_pNavStartup   (nullptr),
      m_pNavMovement  (nullptr),
      m_pNavAbout     (nullptr),
      m_pPanelStartup (nullptr),
      m_pPanelMovement(nullptr),
      m_pPanelAbout   (nullptr),
      m_pHomeValue    (nullptr),
      m_pHomeInput    (nullptr),
      m_pBtnChange    (nullptr),
      m_pSpeedInput   (nullptr),
      m_bEditing      (false),
      m_pOwner        (nullptr)
   {
   }

   // Window-modal behaviour: while the Settings window is visible the owning
   // application window is disabled so its chrome cannot be interacted with;
   // closing Settings (menu toggle or the window's close button) re-enables and
   // refocuses it. The owner is a native window handle supplied by the platform
   // shell -- modality itself is platform-specific.
   static void OnVisibilityChanged (bool bVisible, void* pUserData)
   {
      Impl* pThis = static_cast<Impl*> (pUserData);

      // The '+'/'-' keys may have moved the stored speed position while Settings
      // was closed; re-sync the slider each time the window is shown.
      if (bVisible)
         pThis->RefreshSpeedSlider ();

#ifdef RUBIDIUM_PLATFORM_WINDOWS
      HWND hOwner = static_cast<HWND> (pThis->m_pOwner);

      if (hOwner)
      {
         EnableWindow (hOwner, !bVisible);

         if (!bVisible)
            SetForegroundWindow (hOwner);
      }
#else
      // SDL platforms (Linux / macOS) rely on the in-app chrome modal guard
      // (CHROME_RML::SetModalBlocked, driven from APPFRAME_SDL::ProcessInput via
      // IsSettingsOpen) rather than windowing-system modality. SDL_SetWindowModal
      // is a no-op on Wayland and, on macOS, hiding a window mid modal-session
      // spawned a phantom un-closeable window. Just bring the owner back to the
      // front when Settings closes.
      if (!bVisible)
      {
         SDL_Window* pOwner = static_cast<SDL_Window*> (pThis->m_pOwner);

         if (pOwner)
            SDL_RaiseWindow (pOwner);
      }
#endif
   }

   ~Impl ()
   {
      if (m_Window.Document ())
      {
         if (m_pNavStartup)  m_pNavStartup ->RemoveEventListener (Rml::EventId::Click,   this);
         if (m_pNavMovement) m_pNavMovement->RemoveEventListener (Rml::EventId::Click,   this);
         if (m_pNavAbout)    m_pNavAbout   ->RemoveEventListener (Rml::EventId::Click,   this);
         if (m_pBtnChange)   m_pBtnChange  ->RemoveEventListener (Rml::EventId::Click,   this);
         if (m_pHomeInput)   m_pHomeInput  ->RemoveEventListener (Rml::EventId::Keydown, this);
         if (m_pSpeedInput)  m_pSpeedInput ->RemoveEventListener (Rml::EventId::Change,  this);
      }
   }

   static void OnGeometryChanged (int nX, int nY, int nWidth, int nHeight, bool bMaximized, void* pUserData)
   {
      auto& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();
      jSettings["settings_window"]["x"]         = nX;
      jSettings["settings_window"]["y"]         = nY;
      jSettings["settings_window"]["width"]     = nWidth;
      jSettings["settings_window"]["height"]    = nHeight;
      jSettings["settings_window"]["maximized"] = bMaximized;
   }

   std::string HomeUrl () const
   {
      return APPNATIVE::GetInstance ()->SettingToJSON ().value ("home", std::string (PRODUCT_HOME_URL));
   }

   static std::string LogoPath ()
   {
      std::string sPath;

      if (const char* pBasePath = SDL_GetBasePath ())
         sPath = std::string (pBasePath) + "images/logo.png";

      std::replace (sPath.begin (), sPath.end (), '\\', '/');

      // RmlUi's SystemInterface::JoinPath strips one leading '/' from absolute
      // POSIX paths (it assumes root-relative web URLs), turning "/mnt/.../logo.png"
      // into "mnt/.../logo.png" and breaking the fopen. Prepend an extra '/' so the
      // strip leaves a valid absolute path. Windows drive paths (C:/...) are passed
      // through JoinPath untouched, so leave those alone.
      if (!sPath.empty ()  &&  sPath[0] == '/')
         sPath = "/" + sPath;

      return sPath;
   }

   bool Initialize ()
   {
      bool   bResult = false;
      size_t nPos;

      auto& jWnd   = APPNATIVE::GetInstance ()->SettingToJSON ()["settings_window"];
      int   nWidth  = jWnd.value ("width",  1280);
      int   nHeight = jWnd.value ("height",  720);

      if (m_Window.Initialize ("Rubidium Settings", nWidth, nHeight))
      {
         int nX = jWnd.value ("x", -1);
         int nY = jWnd.value ("y", -1);
         if (nX >= 0 && nY >= 0)
            m_Window.SetPosition (nX, nY);

         m_Window.SetGeometryCallback (OnGeometryChanged, this);
         m_Window.SetVisibilityCallback (OnVisibilityChanged, this);

         std::string sRmlDocument (s_sRmlDocument);
         nPos = sRmlDocument.find ("[{FONT-FAMILY}]");
         if (nPos != std::string::npos)
            sRmlDocument.replace (nPos, 15, APPNATIVE::GetInstance ()->sFontFamily ());

         // The Rubidium logo ships next to the executable (deployed by the build
         // into <basepath>/images/). RmlUi loads <img> through the default file
         // interface, so the src must be an absolute path with forward slashes.
         std::string sLogoPath = LogoPath ();
         while ((nPos = sRmlDocument.find ("[{LOGO-PATH}]")) != std::string::npos)
            sRmlDocument.replace (nPos, 13, sLogoPath);

         if (m_Window.LoadDocument (sRmlDocument))
         {
            Rml::ElementDocument* pDoc = m_Window.Document ();

            m_pNavStartup    = pDoc->GetElementById ("nav-startup");
            m_pNavMovement   = pDoc->GetElementById ("nav-movement");
            m_pNavAbout      = pDoc->GetElementById ("nav-about");
            m_pPanelStartup  = pDoc->GetElementById ("panel-startup");
            m_pPanelMovement = pDoc->GetElementById ("panel-movement");
            m_pPanelAbout    = pDoc->GetElementById ("panel-about");
            m_pHomeValue     = pDoc->GetElementById ("home-value");
            m_pHomeInput     = pDoc->GetElementById ("home-input");
            m_pBtnChange     = pDoc->GetElementById ("btn-change");
            m_pSpeedInput    = pDoc->GetElementById ("speed-input");

            if (m_pNavStartup && m_pNavMovement && m_pNavAbout && m_pHomeValue && m_pHomeInput && m_pBtnChange && m_pSpeedInput)
            {
               m_pHomeValue->SetInnerRML (UTILS::Escape (HomeUrl ()));

               RefreshSpeedSlider ();

               std::string sVersion ("Version ");

               sVersion += RUBIDIUM_VERSION;

               if (Rml::Element* pVer = pDoc->GetElementById ("about-version"))
                  pVer->SetInnerRML (UTILS::Escape (sVersion));

               m_pNavStartup ->AddEventListener (Rml::EventId::Click,   this);
               m_pNavMovement->AddEventListener (Rml::EventId::Click,   this);
               m_pNavAbout   ->AddEventListener (Rml::EventId::Click,   this);
               m_pBtnChange  ->AddEventListener (Rml::EventId::Click,   this);
               m_pHomeInput  ->AddEventListener (Rml::EventId::Keydown, this);
               m_pSpeedInput ->AddEventListener (Rml::EventId::Change,  this);

               bResult = true;
            }
         }
      }

      return bResult;
   }

   void ActivateNav (Rml::Element* pNav)
   {
      m_pNavStartup   ->SetClass ("active", pNav == m_pNavStartup);
      m_pNavMovement  ->SetClass ("active", pNav == m_pNavMovement);
      m_pNavAbout     ->SetClass ("active", pNav == m_pNavAbout);
      m_pPanelStartup ->SetClass ("active", pNav == m_pNavStartup);
      m_pPanelMovement->SetClass ("active", pNav == m_pNavMovement);
      m_pPanelAbout   ->SetClass ("active", pNav == m_pNavAbout);

      // The speed keys ('+'/'-') can change the stored position while Settings is
      // open, so re-sync the slider whenever the Movement panel is shown.
      if (pNav == m_pNavMovement)
         RefreshSpeedSlider ();
   }

   // Point the slider at the current stored movement-speed position.
   void RefreshSpeedSlider ()
   {
      if (m_pSpeedInput)
         m_pSpeedInput->SetAttribute ("value", APPNATIVE::GetInstance ()->MovementSpeedPosition ());
   }

   void SaveHome ()
   {
      std::string sNew (m_pHomeInput->GetAttribute<Rml::String> ("value", "").c_str ());

      size_t nFirst = sNew.find_first_not_of (" \t\r\n");
      size_t nLast  = sNew.find_last_not_of  (" \t\r\n");

      if (nFirst != std::string::npos)
      {
         std::string sTrimmed = sNew.substr (nFirst, nLast - nFirst + 1);

         APPNATIVE::GetInstance ()->SettingToJSON ()["home"] = sTrimmed;
         m_pHomeValue->SetInnerRML (UTILS::Escape (sTrimmed));
      }

      m_pHomeValue->SetClass ("hidden", false);
      m_pHomeInput->SetClass ("hidden", true);
      m_pBtnChange->SetInnerRML ("Change");
      m_bEditing = false;
   }

   void ProcessEvent (Rml::Event& Event) override
   {
      Rml::Element* pElement = Event.GetCurrentElement ();

      if (Event.GetId () == Rml::EventId::Change  &&  pElement == m_pSpeedInput)
      {
         int nPosition = static_cast<int> (Event.GetParameter<float> ("value", static_cast<float> (APP::kMovementSpeedDefault)) + 0.5f);
         APPNATIVE::GetInstance ()->MovementSpeedPosition (nPosition);
      }
      else if (Event.GetId () == Rml::EventId::Click)
      {
         if (pElement == m_pNavStartup)
         {
            ActivateNav (m_pNavStartup);
         }
         else if (pElement == m_pNavMovement)
         {
            ActivateNav (m_pNavMovement);
         }
         else if (pElement == m_pNavAbout)
         {
            ActivateNav (m_pNavAbout);
         }
         else if (pElement == m_pBtnChange)
         {
            if (m_bEditing)
            {
               SaveHome ();
            }
            else
            {
               m_pHomeInput->SetAttribute ("value", HomeUrl ());
               m_pHomeValue->SetClass ("hidden", true);
               m_pHomeInput->SetClass ("hidden", false);
               m_pBtnChange->SetInnerRML ("Save");
               m_bEditing = true;
               m_pHomeInput->Focus ();
            }
         }
      }
      else if (Event.GetId () == Rml::EventId::Keydown)
      {
         if (m_bEditing  &&  Event.GetParameter<int> ("key_identifier", 0) == Rml::Input::KI_RETURN)
            SaveHome ();
      }
   }

   RMLUI_SDL     m_Window;

   Rml::Element* m_pNavStartup;
   Rml::Element* m_pNavMovement;
   Rml::Element* m_pNavAbout;
   Rml::Element* m_pPanelStartup;
   Rml::Element* m_pPanelMovement;
   Rml::Element* m_pPanelAbout;
   Rml::Element* m_pHomeValue;
   Rml::Element* m_pHomeInput;
   Rml::Element* m_pBtnChange;
   Rml::Element* m_pSpeedInput;
   bool          m_bEditing;
   void*         m_pOwner;
};

/*******************************************************************************************************************************
**                                                     SETTINGS_RML                                                          **
*******************************************************************************************************************************/

SETTINGS_RML::SETTINGS_RML () :
   m_pImpl (new Impl ())
{
}

SETTINGS_RML::~SETTINGS_RML ()
{
   delete m_pImpl;
   m_pImpl = nullptr;
}

bool SETTINGS_RML::Initialize ()  { return m_pImpl->Initialize (); }

void SETTINGS_RML::Toggle ()      {        m_pImpl->m_Window.Toggle (); }
void SETTINGS_RML::Show ()        {        m_pImpl->m_Window.Show (); }
bool SETTINGS_RML::IsVisible () const { return m_pImpl->m_Window.IsVisible (); }
bool SETTINGS_RML::IsOpen () const    { return m_pImpl->m_Window.IsOpen (); }

void SETTINGS_RML::SetOwner (void* hOwner) { m_pImpl->m_pOwner = hOwner; }
