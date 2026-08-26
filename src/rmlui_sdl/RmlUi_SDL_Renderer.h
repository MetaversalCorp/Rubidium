// Adapted from RmlUi's Backends/RmlUi_Renderer_SDL.h
// Original: https://github.com/mikke89/RmlUi (MIT License)
// Modified: removed SDL_image dependency for Rubidium POC.

#pragma once

#include <RmlUi/Core/RenderInterface.h>

#ifndef RMLUI_SDL_VERSION_MAJOR
   #define RMLUI_SDL_VERSION_MAJOR 3
#endif

#if RMLUI_SDL_VERSION_MAJOR == 3
   #include <SDL3/SDL.h>
#elif RMLUI_SDL_VERSION_MAJOR == 2
   #include <SDL.h>
#else
   #error "Unspecified RMLUI_SDL_VERSION_MAJOR."
#endif

class RenderInterface_SDL : public Rml::RenderInterface {
public:
	RenderInterface_SDL(SDL_Renderer* renderer);

	void BeginFrame();
	void EndFrame();

	void SetClearColor(Uint8 r, Uint8 g, Uint8 b);

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
	void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) override;

	Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
	void ReleaseTexture(Rml::TextureHandle texture_handle) override;

	void EnableScissorRegion(bool enable) override;
	void SetScissorRegion(Rml::Rectanglei region) override;

private:
	struct GeometryView {
		Rml::Span<const Rml::Vertex> vertices;
		Rml::Span<const int> indices;
	};

	SDL_Renderer* renderer;
	SDL_BlendMode blend_mode = {};
	SDL_Rect rect_scissor = {};
	bool scissor_region_enabled = false;
	Uint8 clear_r = 0, clear_g = 0, clear_b = 0;
};
