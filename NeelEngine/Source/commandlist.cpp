#include "neel_engine_pch.h"

#include "commandlist.h"

#include "neel_engine.h"
#include "dynamic_descriptor_heap.h"
#include "index_buffer.h"
#include "render_target.h"
#include "resource_state_tracker.h"
#include "structured_buffer.h"
#include "vertex_buffer.h"
#include "upload_buffer.h"
#include "root_signature.h"

std::map<std::wstring, ID3D12Resource*> CommandList::texture_cache_;
std::mutex CommandList::texture_cache_mutex_;

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
	UAVBarrier(resource.GetD3D12Resource());
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

void CommandList::ClearTexture(const Texture& texture, const float clear_color[4])
{
	TransitionBarrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
	d3d12_command_list_->ClearRenderTargetView(texture.GetRenderTargetView(), clear_color, 0, nullptr);

	TrackResource(texture);
}

void CommandList::ClearDepthStencilTexture(const Texture& texture, D3D12_CLEAR_FLAGS clear_flags, float depth,
                                           uint8_t stencil)
{
	TransitionBarrier(texture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
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

		UINT64 requiredSize = GetRequiredIntermediateSize(destination_resource.Get(), first_subresource,
		                                                  num_subresources);

		// Create a temporary (intermediate) resource for uploading the subresources
		ComPtr<ID3D12Resource> intermediate_resource;
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(requiredSize),
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

void CommandList::SetCompute32BitConstants(uint32_t root_parameter_index, uint32_t num_constants, const void* constants)
{
	d3d12_command_list_->SetComputeRoot32BitConstants(root_parameter_index, num_constants, constants, 0);
}

void CommandList::SetVertexBuffer(uint32_t slot, const VertexBuffer& vertex_buffer)
{
	TransitionBarrier(vertex_buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	auto vertex_buffer_views = vertex_buffer.GetVertexBufferViews();

	d3d12_command_list_->IASetVertexBuffers(slot, vertex_buffer.GetVertexBufferViews().size(), &vertex_buffer_views[0]);

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
