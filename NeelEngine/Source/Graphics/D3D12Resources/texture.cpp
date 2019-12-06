#include "neel_engine_pch.h"

#include "texture.h"
#include "neel_engine.h"
#include "resource_state_tracker.h"

Texture::Texture(TextureUsage texture_usage, const std::string& name)
	: Resource(name)
	  , texture_usage_(texture_usage)
{
}

Texture::Texture(const D3D12_RESOURCE_DESC& resource_desc,
                 const D3D12_CLEAR_VALUE* clear_value,
                 TextureUsage texture_usage,
                 const std::string& name)
	: Resource(resource_desc, clear_value, name)
	  , texture_usage_(texture_usage)
{
	Texture::CreateViews();
}

Texture::Texture(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                 TextureUsage texture_usage,
                 const std::string& name)
	: Resource(resource, name)
	  , texture_usage_(texture_usage)
{
	Texture::CreateViews();
}

Texture::Texture(const Texture& copy)
	: Resource(copy)
{
	Texture::CreateViews();
}

Texture::Texture(Texture&& copy)
	: Resource(copy)
{
	Texture::CreateViews();
}

Texture& Texture::operator=(const Texture& other)
{
	Resource::operator=(other);

	CreateViews();

	return *this;
}

Texture& Texture::operator=(Texture&& other)
{
	Resource::operator=(other);

	CreateViews();

	return *this;
}

Texture::~Texture()
{
}

void Texture::Resize(uint32_t width, uint32_t height, uint32_t depth_or_array_size)
{
	// Resource can't be resized if it was never created in the first place.
	if (d3d12_resource_)
	{
		ResourceStateTracker::RemoveGlobalResourceState(d3d12_resource_.Get());

		CD3DX12_RESOURCE_DESC res_desc(d3d12_resource_->GetDesc());

		res_desc.Width = std::max(width, 1u);
		res_desc.Height = std::max(height, 1u);
		res_desc.DepthOrArraySize = depth_or_array_size;

		auto device = NeelEngine::Get().GetDevice();

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&res_desc,
			D3D12_RESOURCE_STATE_COMMON,
			d3d12_clear_value_.get(),
			IID_PPV_ARGS(&d3d12_resource_)
		));

		// Retain the name of the resource if one was already specified.
		d3d12_resource_->SetName(utf8_to_utf16(resource_name_).c_str());

		ResourceStateTracker::AddGlobalResourceState(d3d12_resource_.Get(), D3D12_RESOURCE_STATE_COMMON);

		CreateViews();
	}
}

// Get a UAV description that matches the resource description.
D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(const D3D12_RESOURCE_DESC& res_desc, UINT mip_slice, UINT array_slice = 0,
                                            UINT plane_slice = 0)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
	uav_desc.Format = res_desc.Format;

	switch (res_desc.Dimension)
	{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			if (res_desc.DepthOrArraySize > 1)
			{
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
				uav_desc.Texture1DArray.ArraySize = res_desc.DepthOrArraySize - array_slice;
				uav_desc.Texture1DArray.FirstArraySlice = array_slice;
				uav_desc.Texture1DArray.MipSlice = mip_slice;
			}
			else
			{
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
				uav_desc.Texture1D.MipSlice = mip_slice;
			}
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			if (res_desc.DepthOrArraySize > 1)
			{
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uav_desc.Texture2DArray.ArraySize = res_desc.DepthOrArraySize - array_slice;
				uav_desc.Texture2DArray.FirstArraySlice = array_slice;
				uav_desc.Texture2DArray.PlaneSlice = plane_slice;
				uav_desc.Texture2DArray.MipSlice = mip_slice;
			}
			else
			{
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uav_desc.Texture2D.PlaneSlice = plane_slice;
				uav_desc.Texture2D.MipSlice = mip_slice;
			}
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			uav_desc.Texture3D.WSize = res_desc.DepthOrArraySize - array_slice;
			uav_desc.Texture3D.FirstWSlice = array_slice;
			uav_desc.Texture3D.MipSlice = mip_slice;
			break;
		default:
			throw std::exception("Invalid resource dimension.");
	}

	return uav_desc;
}

void Texture::CreateViews()
{
	if (d3d12_resource_)
	{
		auto& app = NeelEngine::Get();
		auto device = app.GetDevice();

		CD3DX12_RESOURCE_DESC desc(d3d12_resource_->GetDesc());

		if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 &&
			CheckRTVSupport())
		{
			render_target_view_ = app.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			device->CreateRenderTargetView(d3d12_resource_.Get(), nullptr, render_target_view_.GetDescriptorHandle());
		}
		if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0 &&
			CheckDSVSupport())
		{
			depth_stencil_view_ = app.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			device->CreateDepthStencilView(d3d12_resource_.Get(), nullptr, depth_stencil_view_.GetDescriptorHandle());
		}
	}

	std::lock_guard<std::mutex> lock(shader_resource_views_mutex_);
	std::lock_guard<std::mutex> guard(unordered_access_views_mutex_);

	// SRVs and UAVs will be created as needed.
	shader_resource_views_.clear();
	unordered_access_views_.clear();
}

DescriptorAllocation Texture::CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc) const
{
	auto& app = NeelEngine::Get();
	auto device = app.GetDevice();
	auto srv = app.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	device->CreateShaderResourceView(d3d12_resource_.Get(), srv_desc, srv.GetDescriptorHandle());

	return srv;
}

DescriptorAllocation Texture::CreateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc) const
{
	auto& app = NeelEngine::Get();
	auto device = app.GetDevice();
	auto uav = app.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	device->CreateUnorderedAccessView(d3d12_resource_.Get(), nullptr, uav_desc, uav.GetDescriptorHandle());

	return uav;
}


D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc) const
{
	std::size_t hash = 0;
	if (srv_desc)
	{
		hash = std::hash<D3D12_SHADER_RESOURCE_VIEW_DESC>{}(*srv_desc);
	}

	std::lock_guard<std::mutex> lock(shader_resource_views_mutex_);

	auto iter = shader_resource_views_.find(hash);
	if (iter == shader_resource_views_.end())
	{
		auto srv = CreateShaderResourceView(srv_desc);
		iter = shader_resource_views_.insert({hash, std::move(srv)}).first;
	}

	return iter->second.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc) const
{
	std::size_t hash = 0;
	if (uav_desc)
	{
		hash = std::hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>{}(*uav_desc);
	}

	std::lock_guard<std::mutex> guard(unordered_access_views_mutex_);

	auto iter = unordered_access_views_.find(hash);
	if (iter == unordered_access_views_.end())
	{
		auto uav = CreateUnorderedAccessView(uav_desc);
		iter = unordered_access_views_.insert({hash, std::move(uav)}).first;
	}

	return iter->second.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetRenderTargetView() const
{
	return render_target_view_.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetDepthStencilView() const
{
	return depth_stencil_view_.GetDescriptorHandle();
}

bool Texture::IsUAVCompatibleFormat(DXGI_FORMAT format)
{
	switch (format)
	{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SINT:
			return true;
		default:
			return false;
	}
}

bool Texture::IsSRGBFormat(DXGI_FORMAT format)
{
	switch (format)
	{
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return true;
		default:
			return false;
	}
}

bool Texture::IsBGRFormat(DXGI_FORMAT format)
{
	switch (format)
	{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			return true;
		default:
			return false;
	}
}

bool Texture::IsDepthFormat(DXGI_FORMAT format)
{
	switch (format)
	{
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_D16_UNORM:
			return true;
		default:
			return false;
	}
}


DXGI_FORMAT Texture::GetTypelessFormat(DXGI_FORMAT format)
{
	DXGI_FORMAT typeless_format = format;

	switch (format)
	{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
			typeless_format = DXGI_FORMAT_R32G32B32A32_TYPELESS;
			break;
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
			typeless_format = DXGI_FORMAT_R32G32B32_TYPELESS;
			break;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
			typeless_format = DXGI_FORMAT_R16G16B16A16_TYPELESS;
			break;
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
			typeless_format = DXGI_FORMAT_R32G32_TYPELESS;
			break;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			typeless_format = DXGI_FORMAT_R32G8X24_TYPELESS;
			break;
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
			typeless_format = DXGI_FORMAT_R10G10B10A2_TYPELESS;
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
			typeless_format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
			break;
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
			typeless_format = DXGI_FORMAT_R16G16_TYPELESS;
			break;
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
			typeless_format = DXGI_FORMAT_R32_TYPELESS;
			break;
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
			typeless_format = DXGI_FORMAT_R8G8_TYPELESS;
			break;
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
			typeless_format = DXGI_FORMAT_R16_TYPELESS;
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
			typeless_format = DXGI_FORMAT_R8_TYPELESS;
			break;
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			typeless_format = DXGI_FORMAT_BC1_TYPELESS;
			break;
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
			typeless_format = DXGI_FORMAT_BC2_TYPELESS;
			break;
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			typeless_format = DXGI_FORMAT_BC3_TYPELESS;
			break;
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			typeless_format = DXGI_FORMAT_BC4_TYPELESS;
			break;
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
			typeless_format = DXGI_FORMAT_BC5_TYPELESS;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			typeless_format = DXGI_FORMAT_B8G8R8A8_TYPELESS;
			break;
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			typeless_format = DXGI_FORMAT_B8G8R8X8_TYPELESS;
			break;
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
			typeless_format = DXGI_FORMAT_BC6H_TYPELESS;
			break;
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			typeless_format = DXGI_FORMAT_BC7_TYPELESS;
			break;
		default:
			break ;
	}

	return typeless_format;
}

DXGI_FORMAT Texture::GetSRGBFormat(DXGI_FORMAT format)
{
	DXGI_FORMAT srgb_format = format;
	switch (format)
	{
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			srgb_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			break;
		case DXGI_FORMAT_BC1_UNORM:
			srgb_format = DXGI_FORMAT_BC1_UNORM_SRGB;
			break;
		case DXGI_FORMAT_BC2_UNORM:
			srgb_format = DXGI_FORMAT_BC2_UNORM_SRGB;
			break;
		case DXGI_FORMAT_BC3_UNORM:
			srgb_format = DXGI_FORMAT_BC3_UNORM_SRGB;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			srgb_format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			break;
		case DXGI_FORMAT_B8G8R8X8_UNORM:
			srgb_format = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
			break;
		case DXGI_FORMAT_BC7_UNORM:
			srgb_format = DXGI_FORMAT_BC7_UNORM_SRGB;
			break;
		default:
			break;
	}

	return srgb_format;
}

DXGI_FORMAT Texture::GetUAVCompatableFormat(DXGI_FORMAT format)
{
	DXGI_FORMAT uav_format = format;

	switch (format)
	{
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
			uav_format = DXGI_FORMAT_R32_FLOAT;
			break;
		default:
			break;
	}

	return uav_format;
}
