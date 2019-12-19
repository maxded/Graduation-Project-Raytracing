#pragma once

enum class TextureUsage
{
	Albedo,
	Diffuse		= Albedo, // Treat Diffuse and Albedo textures the same.
	Heightmap,
	Depth		= Heightmap, // Treat height and depth textures the same.	
	Normalmap,
	MetalRoughnessmap,
	Emissivemap,
	AmbientOcclusionmap,
	RenderTarget,	// Texture is used as a render target.	
};
