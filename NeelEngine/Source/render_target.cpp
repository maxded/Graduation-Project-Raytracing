#include "neel_engine_pch.h"

#include "render_target.h"

RenderTarget::RenderTarget()
	: textures_(AttachmentPoint::kNumAttachmentPoints)
	  , size_(0, 0)
{
}

// Attach a texture to the render target.
// The texture will be copied into the texture array.
void RenderTarget::AttachTexture(AttachmentPoint attachment_point, const Texture& texture)
{
	textures_[attachment_point] = texture;

	if (texture.GetD3D12Resource())
	{
		auto desc = texture.GetD3D12Resource()->GetDesc();
		size_.x = static_cast<uint32_t>(desc.Width);
		size_.y = static_cast<uint32_t>(desc.Height);
	}
}

const Texture& RenderTarget::GetTexture(AttachmentPoint attachment_point) const
{
	return textures_[attachment_point];
}

// Resize all of the textures associated with the render target.
void RenderTarget::Resize(DirectX::XMUINT2 size)
{
	size_ = size;
	for (auto& texture : textures_)
	{
		texture.Resize(size_.x, size_.y);
	}
}

void RenderTarget::Resize(uint32_t width, uint32_t height)
{
	Resize(XMUINT2(width, height));
}

DirectX::XMUINT2 RenderTarget::GetSize() const
{
	return size_;
}

uint32_t RenderTarget::GetWidth() const
{
	return size_.x;
}

uint32_t RenderTarget::GetHeight() const
{
	return size_.y;
}

D3D12_VIEWPORT RenderTarget::GetViewport(DirectX::XMFLOAT2 scale, DirectX::XMFLOAT2 bias, float min_depth,
                                         float max_depth) const
{
	UINT64 width = 0;
	UINT height = 0;

	for (int i = AttachmentPoint::kColor0; i <= AttachmentPoint::kColor7; ++i)
	{
		const Texture& texture = textures_[i];
		if (texture.IsValid())
		{
			auto desc = texture.GetD3D12ResourceDesc();
			width = std::max(width, desc.Width);
			height = std::max(height, desc.Height);
		}
	}

	D3D12_VIEWPORT viewport = {
		(width * bias.x), // TopLeftX
		(height * bias.y), // TopLeftY
		(width * scale.x), // Width
		(height * scale.y), // Height
		min_depth, // MinDepth
		max_depth // MaxDepth
	};

	return viewport;
}

// Get a list of the textures attached to the render target.
// This method is primarily used by the CommandList when binding the
// render target to the output merger stage of the rendering pipeline.
const std::vector<Texture>& RenderTarget::GetTextures() const
{
	return textures_;
}

D3D12_RT_FORMAT_ARRAY RenderTarget::GetRenderTargetFormats() const
{
	D3D12_RT_FORMAT_ARRAY rtv_formats = {};

	for (int i = AttachmentPoint::kColor0; i <= AttachmentPoint::kColor7; ++i)
	{
		const Texture& texture = textures_[i];
		if (texture.IsValid())
		{
			rtv_formats.RTFormats[rtv_formats.NumRenderTargets++] = texture.GetD3D12ResourceDesc().Format;
		}
	}

	return rtv_formats;
}

DXGI_FORMAT RenderTarget::GetDepthStencilFormat() const
{
	DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN;
	const Texture& depth_stencil_texture = textures_[AttachmentPoint::kDepthStencil];
	if (depth_stencil_texture.IsValid())
	{
		dsv_format = depth_stencil_texture.GetD3D12ResourceDesc().Format;
	}

	return dsv_format;
}
