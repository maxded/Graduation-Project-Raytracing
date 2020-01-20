#include "neel_engine_pch.h"

#include "commandlist.h"
#include "commandqueue.h"
#include "generate_mips_pso.h"

#include "neel_engine.h"
#include "dynamic_descriptor_heap.h"
#include "index_buffer.h"
#include "render_target.h"
#include "resource_state_tracker.h"
#include "structured_buffer.h"
#include "vertex_buffer.h"
#include "upload_buffer.h"
#include "root_signature.h"

#include "DirectXTex.h"

CommandList::CommandList(D3D12_COMMAND_LIST_TYPE type)
	: d3d12_command_list_type_(type)
{
	auto device = NeelEngine::Get().GetDevice();

	ThrowIfFailed(device->CreateCommandAllocator(d3d12_command_list_type_, IID_PPV_ARGS(&d3d12_command_allocator_)));

	ThrowIfFailed(device->CreateCommandList(0, d3d12_command_list_type_, d3d12_command_allocator_.Get(),
	                                        nullptr, IID_PPV_ARGS(&d3d12_command_list_)));

	upload_buffer_ = std::make_unique<UploadBuffer>();

	resource_state_tracker_ = std::make_unique<ResourceStateTracker>();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamic_descriptor_heap_[i] = std::make_unique<DynamicDescriptorHeap>(
			static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
		descriptor_heaps_[i] = nullptr;
	}
}

CommandList::~CommandList()
{
}

void CommandList::TransitionBarrier(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES state_after,
                                    UINT subresource, bool flush_barriers)
{
	if (resource)
	{
		// The "before" State is not important. It will be resolved by the resource State tracker.
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COMMON, state_after,
		                                                    subresource);
		resource_state_tracker_->ResourceBarrier(barrier);
	}

	if (flush_barriers)
	{
		FlushResourceBarriers();
	}
}

void CommandList::TransitionBarrier(const Resource& resource, D3D12_RESOURCE_STATES state_after, UINT subresource,
                                    bool flush_barriers)
{
	TransitionBarrier(resource.GetD3D12Resource(), state_after, subresource, flush_barriers);
}

void CommandList::UAVBarrier(Microsoft::WRL::ComPtr<ID3D12Resource> resource, bool flush_barriers)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());

	resource_state_tracker_->ResourceBarrier(barrier);

	if (flush_barriers)
	{
		FlushResourceBarriers();
	}
}

void CommandList::UAVBarrier(const Resource& resource, bool flush_barriers)
{
	UAVBarrier(resource.GetD3D12Resource(), flush_barriers);
}

void CommandList::AliasingBarrier(Microsoft::WRL::ComPtr<ID3D12Resource> before_resource,
                                  Microsoft::WRL::ComPtr<ID3D12Resource> after_resource, bool flush_barriers)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(before_resource.Get(), after_resource.Get());

	resource_state_tracker_->ResourceBarrier(barrier);

	if (flush_barriers)
	{
		FlushResourceBarriers();
	}
}

void CommandList::AllocateUAVBuffer(UINT64 buffer_size, Resource& resource, D3D12_RESOURCE_STATES initial_state)
{
	auto device = NeelEngine::Get().GetDevice();

	ComPtr<ID3D12Resource> d3d12_resource;
	
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		initial_state,
		nullptr,
		IID_PPV_ARGS(&d3d12_resource)));

	// Add the resource to the global resource State tracker.
	ResourceStateTracker::AddGlobalResourceState(d3d12_resource.Get(), initial_state);

	TrackResource(d3d12_resource);

	resource.SetD3D12Resource(d3d12_resource);
}

void CommandList::AllocateUAVBuffer(UINT64 buffer_size, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initial_state)
{
	auto device = NeelEngine::Get().GetDevice();
	
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		initial_state,
		nullptr,
		IID_PPV_ARGS(ppResource)));

	ResourceStateTracker::AddGlobalResourceState(*ppResource, initial_state);
}

D3D12_GPU_VIRTUAL_ADDRESS CommandList::AllocateUploadBuffer(size_t size_in_bytes, const void* buffer_data)
{
	auto heap_allocation = upload_buffer_->Allocate(size_in_bytes, size_in_bytes);
	memcpy(heap_allocation.CPU, buffer_data, size_in_bytes);

	return heap_allocation.GPU;
}


void CommandList::AliasingBarrier(const Resource& before_resource, const Resource& after_resource, bool flush_barriers)
{
	AliasingBarrier(before_resource.GetD3D12Resource(), after_resource.GetD3D12Resource());
}

void CommandList::FlushResourceBarriers()
{
	resource_state_tracker_->FlushResourceBarriers(*this);
}

void CommandList::CopyResource(Microsoft::WRL::ComPtr<ID3D12Resource> dst_res,
                               Microsoft::WRL::ComPtr<ID3D12Resource> src_res)
{
	TransitionBarrier(dst_res, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionBarrier(src_res, D3D12_RESOURCE_STATE_COPY_SOURCE);

	FlushResourceBarriers();

	d3d12_command_list_->CopyResource(dst_res.Get(), src_res.Get());

	TrackResource(dst_res);
	TrackResource(src_res);
}

void CommandList::CopyResource(Resource& dst_res, const Resource& src_res)
{
	CopyResource(dst_res.GetD3D12Resource(), src_res.GetD3D12Resource());
}

void CommandList::ResolveSubresource(Resource& dst_res, const Resource& src_res, uint32_t dst_subresource,
                                     uint32_t src_subresource)
{
	TransitionBarrier(dst_res, D3D12_RESOURCE_STATE_RESOLVE_DEST, dst_subresource);
	TransitionBarrier(src_res, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, src_subresource);

	FlushResourceBarriers();

	d3d12_command_list_->ResolveSubresource(dst_res.GetD3D12Resource().Get(), dst_subresource,
	                                        src_res.GetD3D12Resource().Get(), src_subresource,
	                                        dst_res.GetD3D12ResourceDesc().Format);

	TrackResource(src_res);
	TrackResource(dst_res);
}


void CommandList::CopyBuffer(Buffer& buffer, size_t num_elements, size_t element_size, const void* buffer_data,
                             D3D12_RESOURCE_FLAGS flags)
{
	auto device = NeelEngine::Get().GetDevice();

	size_t buffer_size = num_elements * element_size;

	ComPtr<ID3D12Resource> d3d12_resource;
	if (buffer_size == 0)
	{
		// This will result in a NULL resource (which may be desired to define a default null resource).
	}
	else
	{
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(buffer_size, flags),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&d3d12_resource)));

		// Add the resource to the global resource State tracker.
		ResourceStateTracker::AddGlobalResourceState(d3d12_resource.Get(), D3D12_RESOURCE_STATE_COMMON);

		if (buffer_data != nullptr)
		{
			// Create an upload resource to use as an intermediate buffer to copy the buffer resource 
			ComPtr<ID3D12Resource> upload_resource;
			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(buffer_size),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&upload_resource)));

			D3D12_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pData = buffer_data;
			subresource_data.RowPitch = buffer_size;
			subresource_data.SlicePitch = subresource_data.RowPitch;

			resource_state_tracker_->TransitionResource(d3d12_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
			FlushResourceBarriers();

			UpdateSubresources(d3d12_command_list_.Get(), d3d12_resource.Get(),
			                   upload_resource.Get(), 0, 0, 1, &subresource_data);

			// Add references to resources so they stay in scope until the command list is reset.
			TrackResource(upload_resource);
		}
		TrackResource(d3d12_resource);
	}

	buffer.SetD3D12Resource(d3d12_resource);
	buffer.CreateViews(num_elements, element_size);
}

void CommandList::CopyBuffer(Buffer& buffer, size_t buffer_size, const void* buffer_data, D3D12_RESOURCE_FLAGS flags)
{
	auto device = NeelEngine::Get().GetDevice();

	ComPtr<ID3D12Resource> d3d12_resource;
	if (buffer_size == 0)
	{
		// This will result in a NULL resource (which may be desired to define a default null resource).
	}
	else
	{
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(buffer_size, flags),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&d3d12_resource)));

		// Add the resource to the global resource State tracker.
		ResourceStateTracker::AddGlobalResourceState(d3d12_resource.Get(), D3D12_RESOURCE_STATE_COMMON);

		if (buffer_data != nullptr)
		{
			// Create an upload resource to use as an intermediate buffer to copy the buffer resource 
			ComPtr<ID3D12Resource> upload_resource;
			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(buffer_size),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&upload_resource)));

			D3D12_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pData = buffer_data;
			subresource_data.RowPitch = buffer_size;
			subresource_data.SlicePitch = subresource_data.RowPitch;

			resource_state_tracker_->TransitionResource(d3d12_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
			FlushResourceBarriers();

			UpdateSubresources(d3d12_command_list_.Get(), d3d12_resource.Get(),
			                   upload_resource.Get(), 0, 0, 1, &subresource_data);

			// Add references to resources so they stay in scope until the command list is reset.
			TrackResource(upload_resource);
		}
		TrackResource(d3d12_resource);
	}
	buffer.SetD3D12Resource(d3d12_resource);
}

void CommandList::CopyVertexBuffer(VertexBuffer& vertex_buffer, size_t num_vertices, size_t vertex_stride,
                                   const void* vertex_buffer_data)
{
	CopyBuffer(vertex_buffer, num_vertices, vertex_stride, vertex_buffer_data);
}

void CommandList::CopyIndexBuffer(IndexBuffer& index_buffer, size_t num_indicies, DXGI_FORMAT index_format,
                                  const void* index_buffer_data)
{
	size_t index_size_in_bytes = index_format == DXGI_FORMAT_R16_UINT ? 2 : 4;
	CopyBuffer(index_buffer, num_indicies, index_size_in_bytes, index_buffer_data);
}

void CommandList::CopyByteAddressBuffer(ByteAddressBuffer& byte_address_buffer, size_t buffer_size,
                                        const void* buffer_data)
{
	CopyBuffer(byte_address_buffer, 1, buffer_size, buffer_data, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void CommandList::CopyStructuredBuffer(StructuredBuffer& structured_buffer, size_t num_elements, size_t element_size,
                                       const void* buffer_data)
{
	CopyBuffer(structured_buffer, num_elements, element_size, buffer_data, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void CommandList::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitive_topology)
{
	d3d12_command_list_->IASetPrimitiveTopology(primitive_topology);
}

void CommandList::LoadTextureFromFile(Texture& texture, const std::string& filename, TextureUsage texture_usage)
{
	std::filesystem::path file_path(filename);
	
	if (!std::filesystem::exists(file_path))
	{
		throw std::exception("File not found.");
	}

	const std::wstring wfilename = utf8_to_utf16(filename);

		TexMetadata metadata;
		ScratchImage scratch_image;

		if (file_path.extension() == ".dds")
		{
			ThrowIfFailed(LoadFromDDSFile(
				wfilename.c_str(),
				DDS_FLAGS_FORCE_RGB,
				&metadata,
				scratch_image));
		}
		else if (file_path.extension() == ".hdr")
		{
			ThrowIfFailed(LoadFromHDRFile(
				wfilename.c_str(),
				&metadata,
				scratch_image));
		}
		else if (file_path.extension() == ".tga")
		{
			ThrowIfFailed(LoadFromTGAFile(
				wfilename.c_str(),
				&metadata,
				scratch_image));
		}
		else
		{
			ThrowIfFailed(LoadFromWICFile(
				file_path.c_str(),
				WIC_FLAGS_FORCE_RGB,
				&metadata,
				scratch_image));
		}

		// Force albedo textures to use sRGB
		if (texture_usage == TextureUsage::Albedo)
		{
			metadata.format = MakeSRGB(metadata.format);
		}

		D3D12_RESOURCE_DESC texture_desc = {};
		switch (metadata.dimension)
		{
		case TEX_DIMENSION_TEXTURE1D:
			texture_desc = CD3DX12_RESOURCE_DESC::Tex1D(
				metadata.format,
				static_cast<UINT64>(metadata.width),
				static_cast<UINT16>(metadata.arraySize));
			break;
		case TEX_DIMENSION_TEXTURE2D:
			texture_desc = CD3DX12_RESOURCE_DESC::Tex2D(
				metadata.format,
				static_cast<UINT64>(metadata.width),
				static_cast<UINT>(metadata.height),
				static_cast<UINT16>(metadata.arraySize));
			break;
		case TEX_DIMENSION_TEXTURE3D:
			texture_desc = CD3DX12_RESOURCE_DESC::Tex3D(
				metadata.format,
				static_cast<UINT64>(metadata.width),
				static_cast<UINT>(metadata.height),
				static_cast<UINT16>(metadata.depth));
			break;
		default:
			throw std::exception("Invalid texture dimension.");
			break;
		}

		auto device = NeelEngine::Get().GetDevice();
		Microsoft::WRL::ComPtr<ID3D12Resource> texture_resource;

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texture_desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&texture_resource)));

		texture.SetTextureUsage(texture_usage);
		texture.SetD3D12Resource(texture_resource);
		texture.CreateViews();
		texture.SetName(filename);
		texture.SetFilename(filename);

		// Update the global state tracker.
		ResourceStateTracker::AddGlobalResourceState(
			texture_resource.Get(), D3D12_RESOURCE_STATE_COMMON);

		std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratch_image.GetImageCount());
		const Image* p_images = scratch_image.GetImages();
		for (int i = 0; i < scratch_image.GetImageCount(); ++i)
		{
			auto& subresource = subresources[i];
			subresource.RowPitch = p_images[i].rowPitch;
			subresource.SlicePitch = p_images[i].slicePitch;
			subresource.pData = p_images[i].pixels;
		}

		CopyTextureSubresource(
			texture,
			0,
			static_cast<uint32_t>(subresources.size()),
			subresources.data());

		if (subresources.size() < texture_resource->GetDesc().MipLevels)
		{
			GenerateMips(texture);
		}
}

void CommandList::ClearTexture(const Texture& texture, const float clear_color[4])
{
	TransitionBarrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
	d3d12_command_list_->ClearRenderTargetView(texture.GetRenderTargetView(), clear_color, 0, nullptr);

	TrackResource(texture);
}

void CommandList::GenerateMips(Texture& texture)
{
	if (d3d12_command_list_type_ == D3D12_COMMAND_LIST_TYPE_COPY)
	{
		if (!compute_command_list_)
		{
			compute_command_list_ = NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE)->GetCommandList();
		}
		compute_command_list_->GenerateMips(texture);
		return;
	}
	auto resource = texture.GetD3D12Resource();

	// If the texture doesn't have a valid resource, do nothing.
	if (!resource) return;
	auto resource_desc = resource->GetDesc();

	// If the texture only has a single mip level (level 0)
	// do nothing.
	if (resource_desc.MipLevels == 1) return;
	// Currently, only non-multi-sampled 2D textures are supported.
	if (resource_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		resource_desc.DepthOrArraySize != 1 ||
		resource_desc.SampleDesc.Count > 1)
	{
		throw std::exception("GenerateMips is only supported for non-multi-sampled 2D Textures.");
	}

	ComPtr<ID3D12Resource> uav_resource = resource;
	// Create an alias of the original resource.
	// This is done to perform a GPU copy of resources with different formats.
	// BGR -> RGB texture copies will fail GPU validation unless performed 
	// through an alias of the BRG resource in a placed heap.
	ComPtr<ID3D12Resource> alias_resource;

	// If the passed-in resource does not allow for UAV access
	// then create a staging resource that is used to generate
	// the mipmap chain.
	if (!texture.CheckUAVSupport() ||
		(resource_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
	{
		auto device = NeelEngine::Get().GetDevice();

		// Describe an alias resource that is used to copy the original texture.
		auto alias_desc = resource_desc;
		// Placed resources can't be render targets or depth-stencil views.
		alias_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		alias_desc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		// Describe a UAV compatible resource that is used to perform
		// mipmapping of the original texture.
		auto uav_desc = alias_desc;   // The flags for the UAV description must match that of the alias description.
		uav_desc.Format = Texture::GetUAVCompatableFormat(resource_desc.Format);

		D3D12_RESOURCE_DESC resource_descs[] = {
			alias_desc,
			uav_desc
		};

		// Create a heap that is large enough to store a copy of the original resource.
		auto allocation_info = device->GetResourceAllocationInfo(0, _countof(resource_descs), resource_descs);

		D3D12_HEAP_DESC heap_desc = {};
		heap_desc.SizeInBytes = allocation_info.SizeInBytes;
		heap_desc.Alignment = allocation_info.Alignment;
		heap_desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
		heap_desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heap_desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heap_desc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;

		ComPtr<ID3D12Heap> heap;
		ThrowIfFailed(device->CreateHeap(&heap_desc, IID_PPV_ARGS(&heap)));

		// Make sure the heap does not go out of scope until the command list
		// is finished executing on the command queue.
		TrackResource(heap);

		// Create a placed resource that matches the description of the 
		// original resource. This resource is used to copy the original 
		// texture to the UAV compatible resource.
		ThrowIfFailed(device->CreatePlacedResource(
			heap.Get(),
			0,
			&alias_desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&alias_resource)
		));

		ResourceStateTracker::AddGlobalResourceState(alias_resource.Get(), D3D12_RESOURCE_STATE_COMMON);
		// Ensure the scope of the alias resource.
		TrackResource(alias_resource);

		// Create a UAV compatible resource in the same heap as the alias
		// resource.
		ThrowIfFailed(device->CreatePlacedResource(
			heap.Get(),
			0,
			&uav_desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&uav_resource)
		));

		ResourceStateTracker::AddGlobalResourceState(uav_resource.Get(), D3D12_RESOURCE_STATE_COMMON);
		// Ensure the scope of the UAV compatible resource.
		TrackResource(uav_resource);

		// Add an aliasing barrier for the alias resource.
		AliasingBarrier(nullptr, alias_resource);

		// Copy the original resource to the alias resource.
		// This ensures GPU validation.
		CopyResource(alias_resource, resource);

		// Add an aliasing barrier for the UAV compatible resource.
		AliasingBarrier(alias_resource, uav_resource);
	}
	
	Texture t(uav_resource, texture.GetTextureUsage());	
	// Generate mips with the UAV compatible resource.
	GenerateMipsUAV(t, Texture::IsSRGBFormat(resource_desc.Format));

	if (alias_resource)
	{
		AliasingBarrier(uav_resource, alias_resource);
		// Copy the alias resource back to the original resource.
		CopyResource(resource, alias_resource);
	}
}

void CommandList::GenerateMipsUAV(Texture& texture, bool is_srgb)
{
	if (!generate_mips_pso_)
	{
		generate_mips_pso_ = std::make_unique<GenerateMipsPSO>();
	}

	d3d12_command_list_->SetPipelineState(generate_mips_pso_->GetPipelineState().Get());
	SetComputeRootSignature(generate_mips_pso_->GetRootSignature());

	GenerateMipsCB generate_mips_cb;
	generate_mips_cb.IsSRGB = is_srgb;

	auto resource = texture.GetD3D12Resource();
	auto resource_desc = resource->GetDesc();

	// Create an SRV that uses the format of the original texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = is_srgb ? Texture::GetSRGBFormat(resource_desc.Format) : resource_desc.Format;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;  // Only 2D textures are supported (this was checked in the calling function).
	srv_desc.Texture2D.MipLevels = resource_desc.MipLevels;

	for (uint32_t src_mip = 0; src_mip < resource_desc.MipLevels - 1u; )
	{
		uint64_t src_width = resource_desc.Width >> src_mip;
		uint32_t src_height = resource_desc.Height >> src_mip;
		uint32_t dst_width = static_cast<uint32_t>(src_width >> 1);
		uint32_t dst_height = src_height >> 1;

		// 0b00(0): Both width and height are even.
		// 0b01(1): Width is odd, height is even.
		// 0b10(2): Width is even, height is odd.
		// 0b11(3): Both width and height are odd.
		generate_mips_cb.SrcDimension = (src_height & 1) << 1 | (src_width & 1);

		// How many mipmap levels to compute this pass (max 4 mips per pass)
		DWORD mip_count;

		// The number of times we can half the size of the texture and get
		// exactly a 50% reduction in size.
		// A 1 bit in the width or height indicates an odd dimension.
		// The case where either the width or the height is exactly 1 is handled
		// as a special case (as the dimension does not require reduction).
		_BitScanForward(&mip_count, (dst_width == 1 ? dst_height : dst_width) |
			(dst_height == 1 ? dst_width : dst_height));
		// Maximum number of mips to generate is 4.
		mip_count = std::min<DWORD>(4, mip_count + 1);
		// Clamp to total number of mips left over.
		mip_count = (src_mip + mip_count) >= resource_desc.MipLevels ?
			resource_desc.MipLevels - src_mip - 1 : mip_count;

		// Dimensions should not reduce to 0.
		// This can happen if the width and height are not the same.
		dst_width = std::max<DWORD>(1, dst_width);
		dst_height = std::max<DWORD>(1, dst_height);

		generate_mips_cb.SrcMipLevel = src_mip;
		generate_mips_cb.NumMipLevels = mip_count;
		generate_mips_cb.TexelSize.x = 1.0f / static_cast<float>(dst_width);
		generate_mips_cb.TexelSize.y = 1.0f / static_cast<float>(dst_height);

		SetCompute32BitConstants(generatemips::kGenerateMipsCb, generate_mips_cb);

		SetShaderResourceView(generatemips::kSrcMip, 0, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, src_mip, 1, &srv_desc);

		for (uint32_t mip = 0; mip < mip_count; ++mip)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
			uav_desc.Format = resource_desc.Format;
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uav_desc.Texture2D.MipSlice = src_mip + mip + 1;

			SetUnorderedAccessView(generatemips::kOutMip, mip, texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, src_mip + mip + 1, 1, &uav_desc);
		}

		// Pad any unused mip levels with a default UAV. Doing this keeps the DX12 runtime happy.
		if (mip_count < 4)
		{
			dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(generatemips::kOutMip, mip_count, 4 - mip_count, generate_mips_pso_->GetDefaultUAV());
		}

		Dispatch(math::DivideByMultiple(dst_width, 8), math::DivideByMultiple(dst_height, 8));

		UAVBarrier(texture);

		src_mip += mip_count;
	}
}

void CommandList::ClearDepthStencilTexture(const Texture& texture, D3D12_CLEAR_FLAGS clear_flags, float depth,
                                           uint8_t stencil)
{
	TransitionBarrier(texture, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, true);
	d3d12_command_list_->ClearDepthStencilView(texture.GetDepthStencilView(), clear_flags, depth, stencil, 0, nullptr);

	TrackResource(texture);
}

void CommandList::CopyTextureSubresource(Texture& texture, uint32_t first_subresource, uint32_t num_subresources,
                                         D3D12_SUBRESOURCE_DATA* subresource_data)
{
	auto device = NeelEngine::Get().GetDevice();
	auto destination_resource = texture.GetD3D12Resource();

	if (destination_resource)
	{
		// Resource must be in the copy-destination State.
		TransitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST);
		FlushResourceBarriers();

		UINT64 required_size = GetRequiredIntermediateSize(destination_resource.Get(), first_subresource,
		                                                  num_subresources);

		// Create a temporary (intermediate) resource for uploading the subresources
		ComPtr<ID3D12Resource> intermediate_resource;
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(required_size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&intermediate_resource)
		));

		UpdateSubresources(d3d12_command_list_.Get(), destination_resource.Get(), intermediate_resource.Get(), 0,
		                   first_subresource, num_subresources, subresource_data);

		TrackResource(intermediate_resource);
		TrackResource(destination_resource);
	}
}

void CommandList::SetGraphicsDynamicConstantBuffer(uint32_t root_parameter_index, size_t size_in_bytes,
                                                   const void* buffer_data)
{
	// Constant buffers must be 256-byte aligned.
	auto heap_allococation = upload_buffer_->Allocate(size_in_bytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(heap_allococation.CPU, buffer_data, size_in_bytes);

	d3d12_command_list_->SetGraphicsRootConstantBufferView(root_parameter_index, heap_allococation.GPU);
}

void CommandList::SetGraphics32BitConstants(uint32_t root_parameter_index, uint32_t num_constants,
                                            const void* constants)
{
	d3d12_command_list_->SetGraphicsRoot32BitConstants(root_parameter_index, num_constants, constants, 0);
}

void CommandList::SetComputeDynamicConstantBuffer(uint32_t root_parameter_index, size_t size_in_bytes,
	const void* buffer_data)
{
	// Constant buffers must be 256-byte aligned.
	auto heap_allococation = upload_buffer_->Allocate(size_in_bytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(heap_allococation.CPU, buffer_data, size_in_bytes);

	d3d12_command_list_->SetComputeRootConstantBufferView(root_parameter_index, heap_allococation.GPU);
}

void CommandList::SetCompute32BitConstants(uint32_t root_parameter_index, uint32_t num_constants, const void* constants)
{
	d3d12_command_list_->SetComputeRoot32BitConstants(root_parameter_index, num_constants, constants, 0);
}

void CommandList::SetComputeAccelerationStructure(uint32_t root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address)
{
	d3d12_command_list_->SetComputeRootShaderResourceView(root_parameter_index, address);
}

void CommandList::SetVertexBuffer(uint32_t slot, const VertexBuffer& vertex_buffer)
{
	TransitionBarrier(vertex_buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	auto vertex_buffer_views = vertex_buffer.GetVertexBufferViews();

	d3d12_command_list_->IASetVertexBuffers(slot, vertex_buffer_views.size(), vertex_buffer_views.data());

	TrackResource(vertex_buffer);
}

void CommandList::SetDynamicVertexBuffer(uint32_t slot, size_t num_vertices, size_t vertex_size,
                                         const void* vertex_buffer_data)
{
	size_t buffer_size = num_vertices * vertex_size;

	auto heap_allocation = upload_buffer_->Allocate(buffer_size, vertex_size);
	memcpy(heap_allocation.CPU, vertex_buffer_data, buffer_size);

	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = {};
	vertex_buffer_view.BufferLocation = heap_allocation.GPU;
	vertex_buffer_view.SizeInBytes = static_cast<UINT>(buffer_size);
	vertex_buffer_view.StrideInBytes = static_cast<UINT>(vertex_size);

	d3d12_command_list_->IASetVertexBuffers(slot, 1, &vertex_buffer_view);
}

void CommandList::SetIndexBuffer(const IndexBuffer& index_buffer)
{
	TransitionBarrier(index_buffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	auto index_buffer_view = index_buffer.GetIndexBufferView();

	d3d12_command_list_->IASetIndexBuffer(&index_buffer_view);

	TrackResource(index_buffer);
}

void CommandList::SetDynamicIndexBuffer(size_t num_indicies, DXGI_FORMAT index_format, const void* index_buffer_data)
{
	size_t index_size_in_bytes = index_format == DXGI_FORMAT_R16_UINT ? 2 : 4;
	size_t buffer_size = num_indicies * index_size_in_bytes;

	auto heap_allocation = upload_buffer_->Allocate(buffer_size, index_size_in_bytes);
	memcpy(heap_allocation.CPU, index_buffer_data, buffer_size);

	D3D12_INDEX_BUFFER_VIEW index_buffer_view = {};
	index_buffer_view.BufferLocation = heap_allocation.GPU;
	index_buffer_view.SizeInBytes = static_cast<UINT>(buffer_size);
	index_buffer_view.Format = index_format;

	d3d12_command_list_->IASetIndexBuffer(&index_buffer_view);
}

void CommandList::SetGraphicsDynamicStructuredBuffer(uint32_t slot, size_t num_elements, size_t element_size,
                                                     const void* buffer_data)
{
	size_t buffer_size = num_elements * element_size;

	auto heap_allocation = upload_buffer_->Allocate(buffer_size, element_size);

	memcpy(heap_allocation.CPU, buffer_data, buffer_size);

	d3d12_command_list_->SetGraphicsRootShaderResourceView(slot, heap_allocation.GPU);
}

void CommandList::SetViewport(const D3D12_VIEWPORT& viewport)
{
	SetViewports({viewport});
}

void CommandList::SetViewports(const std::vector<D3D12_VIEWPORT>& viewports)
{
	assert(viewports.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	d3d12_command_list_->RSSetViewports(static_cast<UINT>(viewports.size()),
	                                    viewports.data());
}

void CommandList::SetScissorRect(const D3D12_RECT& scissor_rect)
{
	SetScissorRects({scissor_rect});
}

void CommandList::SetScissorRects(const std::vector<D3D12_RECT>& scissor_rects)
{
	assert(scissor_rects.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	d3d12_command_list_->RSSetScissorRects(static_cast<UINT>(scissor_rects.size()),
	                                       scissor_rects.data());
}

void CommandList::SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state)
{
	d3d12_command_list_->SetPipelineState(pipeline_state.Get());

	TrackResource(pipeline_state);
}

void CommandList::SetStateObject(Microsoft::WRL::ComPtr<ID3D12StateObject> state_object)
{
	d3d12_command_list_->SetPipelineState1(state_object.Get());

	TrackResource(state_object);
}

void CommandList::SetGraphicsRootSignature(const RootSignature& rootSignature)
{
	auto d3d12_root_signature = rootSignature.GetRootSignature().Get();
	if (root_signature_ != d3d12_root_signature)
	{
		root_signature_ = d3d12_root_signature;

		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			dynamic_descriptor_heap_[i]->ParseRootSignature(rootSignature);
		}

		d3d12_command_list_->SetGraphicsRootSignature(root_signature_);

		TrackResource(root_signature_);
	}
}

void CommandList::SetComputeRootSignature(const RootSignature& rootSignature)
{
	auto d3d12_root_signature = rootSignature.GetRootSignature().Get();
	if (root_signature_ != d3d12_root_signature)
	{
		root_signature_ = d3d12_root_signature;

		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			dynamic_descriptor_heap_[i]->ParseRootSignature(rootSignature);
		}

		d3d12_command_list_->SetComputeRootSignature(root_signature_);

		TrackResource(root_signature_);
	}
}

void CommandList::SetShaderResourceView(uint32_t root_parameter_index,
                                        uint32_t descriptor_offset,
                                        const Resource& resource,
                                        D3D12_RESOURCE_STATES state_after,
                                        UINT first_subresource,
                                        UINT num_subresources,
                                        const D3D12_SHADER_RESOURCE_VIEW_DESC* srv)
{
	if (num_subresources < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		for (uint32_t i = 0; i < num_subresources; ++i)
		{
			TransitionBarrier(resource, state_after, first_subresource + i);
		}
	}
	else
	{
		TransitionBarrier(resource, state_after);
	}

	dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
		root_parameter_index, descriptor_offset, 1, resource.GetShaderResourceView(srv));

	TrackResource(resource);
}

void CommandList::SetUnorderedAccessView(uint32_t root_parameter_index,
                                         uint32_t descrptor_offset,
                                         const Resource& resource,
                                         D3D12_RESOURCE_STATES state_after,
                                         UINT first_subresource,
                                         UINT num_subresources,
                                         const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav)
{
	if (num_subresources < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		for (uint32_t i = 0; i < num_subresources; ++i)
		{
			TransitionBarrier(resource, state_after, first_subresource + i);
		}
	}
	else
	{
		TransitionBarrier(resource, state_after);
	}

	dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
		root_parameter_index, descrptor_offset, 1, resource.GetUnorderedAccessView(uav));

	TrackResource(resource);
}


void CommandList::SetRenderTarget(const RenderTarget& render_target)
{
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> render_target_descriptors;
	render_target_descriptors.reserve(AttachmentPoint::kNumAttachmentPoints);

	const auto& textures = render_target.GetTextures();

	// Bind color targets (max of 8 render targets can be bound to the rendering pipeline.
	for (int i = 0; i < 8; ++i)
	{
		auto& texture = textures[i];

		if (texture.IsValid())
		{
			TransitionBarrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
			render_target_descriptors.push_back(texture.GetRenderTargetView());

			TrackResource(texture);
		}
	}

	const auto& depth_texture = render_target.GetTexture(AttachmentPoint::kDepthStencil);

	CD3DX12_CPU_DESCRIPTOR_HANDLE depth_stencil_descriptor(D3D12_DEFAULT);
	if (depth_texture.GetD3D12Resource())
	{
		TransitionBarrier(depth_texture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		depth_stencil_descriptor = depth_texture.GetDepthStencilView();

		TrackResource(depth_texture);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE* p_dsv = depth_stencil_descriptor.ptr != 0 ? &depth_stencil_descriptor : nullptr;

	d3d12_command_list_->OMSetRenderTargets(static_cast<UINT>(render_target_descriptors.size()),
	                                        render_target_descriptors.data(), FALSE, p_dsv);
}

void CommandList::Draw(uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance)
{
	FlushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamic_descriptor_heap_[i]->CommitStagedDescriptorsForDraw(*this);
	}

	d3d12_command_list_->DrawInstanced(vertex_count, instance_count, start_vertex, start_instance);
}

void CommandList::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t start_index, int32_t base_vertex,
                              uint32_t start_instance)
{
	FlushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamic_descriptor_heap_[i]->CommitStagedDescriptorsForDraw(*this);
	}

	d3d12_command_list_->DrawIndexedInstanced(index_count, instance_count, start_index, base_vertex, start_instance);
}

void CommandList::Dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)
{
	FlushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamic_descriptor_heap_[i]->CommitStagedDescriptorsForDispatch(*this);
	}

	d3d12_command_list_->Dispatch(num_groups_x, num_groups_y, num_groups_z);
}

void CommandList::DispatchRays(D3D12_DISPATCH_RAYS_DESC& dispatch_desc)
{
	FlushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamic_descriptor_heap_[i]->CommitStagedDescriptorsForDispatch(*this);
	}

	d3d12_command_list_->DispatchRays(&dispatch_desc);
}

bool CommandList::Close(CommandList& pending_command_list)
{
	// Flush any remaining barriers.
	FlushResourceBarriers();

	d3d12_command_list_->Close();

	// Flush pending resource barriers.
	uint32_t num_pending_barriers = resource_state_tracker_->FlushPendingResourceBarriers(pending_command_list);
	// Commit the final resource State to the global State.
	resource_state_tracker_->CommitFinalResourceStates();

	return num_pending_barriers > 0;
}

void CommandList::Close()
{
	FlushResourceBarriers();
	d3d12_command_list_->Close();
}


void CommandList::Reset()
{
	ThrowIfFailed(d3d12_command_allocator_->Reset());
	ThrowIfFailed(d3d12_command_list_->Reset(d3d12_command_allocator_.Get(), nullptr));

	resource_state_tracker_->Reset();
	upload_buffer_->Reset();

	ReleaseTrackedObjects();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamic_descriptor_heap_[i]->Reset();
		descriptor_heaps_[i] = nullptr;
	}

	root_signature_ = nullptr;
	compute_command_list_ = nullptr;
}

void CommandList::TrackResource(Microsoft::WRL::ComPtr<ID3D12Object> object)
{
	tracked_objects_.push_back(object);
}

void CommandList::TrackResource(const Resource& res)
{
	TrackResource(res.GetD3D12Resource());
}

void CommandList::ReleaseTrackedObjects()
{
	tracked_objects_.clear();
}

void CommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heap_type, ID3D12DescriptorHeap* heap)
{
	if (descriptor_heaps_[heap_type] != heap)
	{
		descriptor_heaps_[heap_type] = heap;
		BindDescriptorHeaps();
	}
}

void CommandList::BindDescriptorHeaps()
{
	UINT num_descriptor_heaps = 0;
	ID3D12DescriptorHeap* descriptor_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

	for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		ID3D12DescriptorHeap* descriptor_heap = descriptor_heaps_[i];
		if (descriptor_heap)
		{
			descriptor_heaps[num_descriptor_heaps++] = descriptor_heap;
		}
	}

	d3d12_command_list_->SetDescriptorHeaps(num_descriptor_heaps, descriptor_heaps);
}
