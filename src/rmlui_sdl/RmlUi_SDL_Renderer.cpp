// Adapted from RmlUi's Backends/RmlUi_Renderer_SDL.cpp
// Original: https://github.com/mikke89/RmlUi (MIT License)
// Modified: image decoding routed through the Sneeze public API (SNEEZE::IMAGE::Decode)
// instead of SDL_image, so <img> elements load via the engine's stb_image build.

#include "RmlUi_SDL_Renderer.h"
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Types.h>

#include <Image.h>

static void SetRenderClipRect(SDL_Renderer* renderer, const SDL_Rect* rect)
{
#if SDL_MAJOR_VERSION >= 3
	SDL_SetRenderClipRect(renderer, rect);
#else
	SDL_RenderSetClipRect(renderer, rect);
#endif
}
static void SetRenderViewport(SDL_Renderer* renderer, const SDL_Rect* rect)
{
#if SDL_MAJOR_VERSION >= 3
	SDL_SetRenderViewport(renderer, rect);
#else
	SDL_RenderSetViewport(renderer, rect);
#endif
}

RenderInterface_SDL::RenderInterface_SDL(SDL_Renderer* renderer) : renderer(renderer)
{
	blend_mode = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ONE,
		SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
}

void RenderInterface_SDL::BeginFrame()
{
	SetRenderViewport(renderer, nullptr);
	SDL_SetRenderDrawColor(renderer, clear_r, clear_g, clear_b, 255);
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawBlendMode(renderer, blend_mode);
}

void RenderInterface_SDL::EndFrame() {}

void RenderInterface_SDL::SetClearColor(Uint8 r, Uint8 g, Uint8 b)
{
	clear_r = r;
	clear_g = g;
	clear_b = b;
}

Rml::CompiledGeometryHandle RenderInterface_SDL::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
{
	GeometryView* data = new GeometryView{vertices, indices};
	return reinterpret_cast<Rml::CompiledGeometryHandle>(data);
}

void RenderInterface_SDL::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
	delete reinterpret_cast<GeometryView*>(geometry);
}

void RenderInterface_SDL::RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture)
{
	const GeometryView* geometry = reinterpret_cast<GeometryView*>(handle);
	const Rml::Vertex* vertices = geometry->vertices.data();
	const size_t num_vertices = geometry->vertices.size();
	const int* indices = geometry->indices.data();
	const size_t num_indices = geometry->indices.size();

	Rml::UniquePtr<SDL_Vertex[]> sdl_vertices{new SDL_Vertex[num_vertices]};

	for (size_t i = 0; i < num_vertices; i++)
	{
		sdl_vertices[i].position = {vertices[i].position.x + translation.x, vertices[i].position.y + translation.y};
		sdl_vertices[i].tex_coord = {vertices[i].tex_coord.x, vertices[i].tex_coord.y};

		const auto& color = vertices[i].colour;
#if SDL_MAJOR_VERSION >= 3
		sdl_vertices[i].color = {color.red / 255.f, color.green / 255.f, color.blue / 255.f, color.alpha / 255.f};
#else
		sdl_vertices[i].color = {color.red, color.green, color.blue, color.alpha};
#endif
	}

	SDL_Texture* sdl_texture = (SDL_Texture*)texture;

	SDL_RenderGeometry(renderer, sdl_texture, sdl_vertices.get(), (int)num_vertices, indices, (int)num_indices);
}

void RenderInterface_SDL::EnableScissorRegion(bool enable)
{
	if (enable)
		SetRenderClipRect(renderer, &rect_scissor);
	else
		SetRenderClipRect(renderer, nullptr);

	scissor_region_enabled = enable;
}

void RenderInterface_SDL::SetScissorRegion(Rml::Rectanglei region)
{
	rect_scissor.x = region.Left();
	rect_scissor.y = region.Top();
	rect_scissor.w = region.Width();
	rect_scissor.h = region.Height();

	if (scissor_region_enabled)
		SetRenderClipRect(renderer, &rect_scissor);
}

Rml::TextureHandle RenderInterface_SDL::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source)
{
	Rml::TextureHandle handle = {};

	Rml::FileInterface* file_interface = Rml::GetFileInterface();
	Rml::FileHandle file_handle = file_interface->Open(source);

	if (file_handle)
	{
		file_interface->Seek(file_handle, 0, SEEK_END);
		size_t buffer_size = file_interface->Tell(file_handle);
		file_interface->Seek(file_handle, 0, SEEK_SET);

		std::vector<uint8_t> encoded(buffer_size);
		file_interface->Read(encoded.data(), buffer_size, file_handle);
		file_interface->Close(file_handle);

		int width = 0, height = 0;
		std::vector<uint8_t> pixels;

		if (SNEEZE::IMAGE::Decode(encoded, width, height, pixels))
		{
			texture_dimensions = Rml::Vector2i(width, height);
			handle = GenerateTexture(Rml::Span<const Rml::byte>(pixels.data(), pixels.size()), texture_dimensions);
		}
	}

	return handle;
}

Rml::TextureHandle RenderInterface_SDL::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions)
{
	RMLUI_ASSERT(source.data() && source.size() == size_t(source_dimensions.x * source_dimensions.y * 4));

#if SDL_MAJOR_VERSION >= 3
	auto CreateSurface = [&]() {
		return SDL_CreateSurfaceFrom(source_dimensions.x, source_dimensions.y, SDL_PIXELFORMAT_RGBA32, (void*)source.data(), source_dimensions.x * 4);
	};
	auto DestroySurface = [](SDL_Surface* surface) { SDL_DestroySurface(surface); };
#else
	auto CreateSurface = [&]() {
		return SDL_CreateRGBSurfaceWithFormatFrom((void*)source.data(), source_dimensions.x, source_dimensions.y, 32, source_dimensions.x * 4,
			SDL_PIXELFORMAT_RGBA32);
	};
	auto DestroySurface = [](SDL_Surface* surface) { SDL_FreeSurface(surface); };
#endif

	SDL_Surface* surface = CreateSurface();

	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_SetTextureBlendMode(texture, blend_mode);

	DestroySurface(surface);
	return (Rml::TextureHandle)texture;
}

void RenderInterface_SDL::ReleaseTexture(Rml::TextureHandle texture_handle)
{
	SDL_DestroyTexture((SDL_Texture*)texture_handle);
}
