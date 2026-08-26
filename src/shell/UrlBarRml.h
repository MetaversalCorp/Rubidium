// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_SHELL_URLBARRML_H
#define RUBIDIUM_SHELL_URLBARRML_H

#include <functional>
#include <string>

struct SDL_Window;
struct SDL_Renderer;
union  SDL_Event;

namespace RUBIDIUM
{
   class URL_BAR_RML
   {
   public:
      using OnSubmitFn = std::function<void(const std::string&)>;

      URL_BAR_RML ();
      ~URL_BAR_RML ();

      bool Initialize (SDL_Window* pParentWindow, SDL_Renderer* pParentRenderer,
                       int nWidth, const std::string& sUrl, OnSubmitFn fnOnSubmit);
      void Shutdown ();

      // Forwarded from the main SDL event pump.
      void ProcessEvent (const SDL_Event& ev);

      // Drawn as a CANVAS overlay on top of the engine framebuffer.
      void Render (SDL_Renderer* pRenderer);

      void              SetUrl (const std::string& sUrl);
      std::string const GetUrl () const;

      int BarHeight () const;

   private:
      struct Impl;
      Impl* m_pImpl;
   };
}

#endif // RUBIDIUM_SHELL_URLBARRML_H
