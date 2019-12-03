#pragma once

#include <DirectXMath.h> // For XMFLOAT2
#include <cstdint>
#include <vector>

#include "texture.h"

// Don't use scoped enums to avoid the explicit cast required to use these as 
// array indices.
enum AttachmentPoint
{
	kColor0,
	kColor1,
	kColor2,
	kColor3,
	kColor4,
	kColor5,
	kColor6,
	kColor7,
	kDepthStencil,
	kNumAttachmentPoints
};

class RenderTarget
{
public:
	// Create an empty render target.
	RenderTarget();

	RenderTarget(const RenderTarget& copy) = default;
	RenderTarget(RenderTarget&& copy) = default;

	RenderTarget& operator=(const RenderTarget& other) = default;
	RenderTarget& operator=(RenderTarget&& other) = default;

	// Attach a texture to the render target.
	// The texture will be copied into the texture array.
	void AttachTexture(AttachmentPoint attachment_point, const Texture& texture);
	const Texture& GetTexture(AttachmentPoint attachment_point) const;

	// Resize all of the textures associated with the render target.
	void Resize(DirectX::XMUINT2 size);
	void Resize(uint32_t width, uint32_t height);
	DirectX::XMUINT2 GetSize() const;
	uint32_t GetWidth() const;
	uint32_t GetHeight() const;

	// Get a viewport for this render target.
	// The scale and bias parameters can be used to specify a split-screen
	// viewport (the bias parameter is normalized in the range [0...1]).
	// By default, a fullscreen viewport is returned.
	D3D12_VIEWPORT GetViewport(DirectX::XMFLOAT2 scale = {1.0f, 1.0f}, DirectX::XMFLOAT2 bias = {0.0f, 0.0f},
	                           float min_depth = 0.0f, float max_depth = 1.0f) const;

	// Get a list of the textures attached to the render target.
	// This method is primarily used by the CommandList when binding the
	// render target to the output merger stage of the rendering pipeline.
	const std::vector<Texture>& GetTextures() const;

	// Get the render target formats of the textures currently 
	// attached to this render target object.
	// This is needed to configure the Pipeline State object.
	D3D12_RT_FORMAT_ARRAY GetRenderTargetFormats() const;

	// Get the format of the attached depth/stencil buffer.
	DXGI_FORMAT GetDepthStencilFormat() const;

private:

	std::vector<Texture> textures_;
	DirectX::XMUINT2 size_;
};
