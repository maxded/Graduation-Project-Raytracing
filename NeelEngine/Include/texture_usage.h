#pragma once

enum class TextureUsage
{
	kAlbedo,
	kDiffuse = kAlbedo,
	// Treat Diffuse and Albedo textures the same.
	kHeightmap,
	kDepth = kHeightmap,
	// Treat height and depth textures the same.
	kNormalmap,
	kRenderTarget,
	// Texture is used as a render target.
};
