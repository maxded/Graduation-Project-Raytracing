#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <map> // for std::map
#include <memory> // for std::unique_ptr
#include <mutex> // for std::mutex
#include <vector> // for std::vector

#include "texture_usage.h"

class Buffer;
class ByteAddressBuffer;
class ConstantBuffer;
class DynamicDescriptorHeap;
class IndexBuffer;
class RenderTarget;
class Resource;
class ResourceStateTracker;
class RootSignature;
class StructuredBuffer;
class Texture;
class UploadBuffer;
class VertexBuffer;
class GenerateMipsPSO;

class CommandList
{
public:
	CommandList(D3D12_COMMAND_LIST_TYPE type);
	virtual ~CommandList();

	/**
	 * Get the type of command list.
	 */
	D3D12_COMMAND_LIST_TYPE GetCommandListType() const
	{
		return d3d12_command_list_type_;
	}

	/**
	 * Get direct access to the ID3D12GraphicsCommandList2 interface.
	 */
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> GetGraphicsCommandList() const
	{
		return d3d12_command_list_;
	}

	/**
	 * Transition a resource to a particular State.
	 *
	 * @param resource The resource to transition.
	 * @param state_after The State to transition the resource to. The before State is resolved by the resource State tracker.
	 * @param subresource The subresource to transition. By default, this is D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES which indicates that all subresources are transitioned to the same State.
	 * @param flush_barriers Force flush any barriers. Resource barriers need to be flushed before a command (draw, dispatch, or copy) that expects the resource to be in a particular State can run.
	 */
	void TransitionBarrier(const Resource& resource, D3D12_RESOURCE_STATES state_after,
	                       UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool flush_barriers = false);
	void TransitionBarrier(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES state_after,
	                       UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool flush_barriers = false);

	/**
	 * Add a UAV barrier to ensure that any writes to a resource have completed
	 * before reading from the resource.
	 *
	 * @param resource The resource to add a UAV barrier for.
	 * @param flush_barriers Force flush any barriers. Resource barriers need to be
	 * flushed before a command (draw, dispatch, or copy) that expects the resource
	 * to be in a particular State can run.
	 */
	void UAVBarrier(const Resource& resource, bool flush_barriers = false);
	void UAVBarrier(Microsoft::WRL::ComPtr<ID3D12Resource> resource, bool flush_barriers = false);

	/**
	 * Add an aliasing barrier to indicate a transition between usages of two
	 * different resources that occupy the same space in a heap.
	 *
	 * @param before_resource The resource that currently occupies the heap.
	 * @param after_resource The resource that will occupy the space in the heap.
	 */
	void AliasingBarrier(const Resource& before_resource, const Resource& after_resource, bool flush_barriers = false);
	void AliasingBarrier(Microsoft::WRL::ComPtr<ID3D12Resource> before_resource,
	                     Microsoft::WRL::ComPtr<ID3D12Resource> after_resource, bool flush_barriers = false);
	
	/**
	* Allocate UAV buffer 
	*/
	void AllocateUAVBuffer(UINT64 buffer_size, Resource& resource, D3D12_RESOURCE_STATES initial_state= D3D12_RESOURCE_STATE_COMMON);
	void AllocateUAVBuffer(UINT64 buffer_size, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON);

	/**
	* Allocate UAV buffer
	*/
	D3D12_GPU_VIRTUAL_ADDRESS AllocateUploadBuffer(size_t size_in_bytes, const void* buffer_data);
	
	template <typename T>
	D3D12_GPU_VIRTUAL_ADDRESS AllocateUploadBuffer(const T& data)
	{
		return AllocateUploadBuffer(sizeof(T), &data);
	}
	
	/**
	 * Flush any barriers that have been pushed to the command list.
	 */
	void FlushResourceBarriers();

	/**
	 * Copy resources.
	 */
	void CopyResource(Resource& dst_res, const Resource& src_res);
	void CopyResource(Microsoft::WRL::ComPtr<ID3D12Resource> dst_res, Microsoft::WRL::ComPtr<ID3D12Resource> src_res);

	/**
	 * Resolve a multisampled resource into a non-multisampled resource.
	 */
	void ResolveSubresource(Resource& dst_res, const Resource& src_res, uint32_t dst_subresource = 0,
	                        uint32_t src_subresource = 0);

	// Copy the contents of a CPU buffer to a GPU buffer (possibly replacing the previous buffer contents).
	void CopyBuffer(Buffer& buffer, size_t num_elements, size_t element_size, const void* buffer_data,
	                D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	void CopyBuffer(Buffer& buffer, size_t buffer_size, const void* buffer_data,
	                D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	/**
	 * Copy the contents to a vertex buffer in GPU memory.
	 */
	void CopyVertexBuffer(VertexBuffer& vertex_buffer, size_t num_vertices, size_t vertex_stride,
	                      const void* vertex_buffer_data);

	template <typename T>
	void CopyVertexBuffer(VertexBuffer& vertex_buffer, const std::vector<T>& vertex_buffer_data)
	{
		CopyVertexBuffer(vertex_buffer, vertex_buffer_data.size(), sizeof(T), vertex_buffer_data.data());
	}

	/**
	 * Copy the contents to a index buffer in GPU memory.
	 */
	void CopyIndexBuffer(IndexBuffer& index_buffer, size_t num_indicies, DXGI_FORMAT index_format,
	                     const void* index_buffer_data);

	template <typename T>
	void CopyIndexBuffer(IndexBuffer& index_buffer, const std::vector<T>& index_buffer_data)
	{
		assert(sizeof(T) == 2 || sizeof(T) == 4);

		DXGI_FORMAT index_format = (sizeof(T) == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		CopyIndexBuffer(index_buffer, index_buffer_data.size(), index_format, index_buffer_data.data());
	}

	/**
	 * Copy the contents to a byte address buffer in GPU memory.
	 */
	void CopyByteAddressBuffer(ByteAddressBuffer& byte_address_buffer, size_t buffer_size, const void* buffer_data);

	template <typename T>
	void CopyByteAddressBuffer(ByteAddressBuffer& byte_address_buffer, const T& data)
	{
		CopyByteAddressBuffer(byte_address_buffer, sizeof(T), &data);
	}

	/**
	 * Copy the contents to a structured buffer in GPU memory.
	 */
	void CopyStructuredBuffer(StructuredBuffer& structured_buffer, size_t num_elements, size_t element_size,
	                          const void* buffer_data);

	template <typename T>
	void CopyStructuredBuffer(StructuredBuffer& structured_buffer, const std::vector<T>& buffer_data)
	{
		CopyStructuredBuffer(structured_buffer, buffer_data.size(), sizeof(T), buffer_data.data());
	}

	/**
	 * Set the current primitive topology for the rendering pipeline.
	 */
	void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitive_topology);

	/**
	* Load a texture by a filename.
	*/
	void LoadTextureFromFile(Texture& texture, const std::string& filename, TextureUsage texture_usage = TextureUsage::Albedo);

	/**
	 * Clear a texture.
	 */
	void ClearTexture(const Texture& texture, const float clear_color[4]);

	/**
	* Generate mips for the texture.
	* The first subresource is used to generate the mip chain.
	* Mips are automatically generated for textures loaded from files.
	*/
	void GenerateMips(Texture& texture);

	/**
	 * Clear depth/stencil texture.
	 */
	void ClearDepthStencilTexture(const Texture& texture, D3D12_CLEAR_FLAGS clear_flags, float depth = 1.0f,
	                              uint8_t stencil = 0);

	/**
	 * Copy subresource data to a texture.
	 */
	void CopyTextureSubresource(Texture& texture, uint32_t first_subresource, uint32_t num_subresources,
	                            D3D12_SUBRESOURCE_DATA* subresource_data);

	/**
	 * Set a dynamic constant buffer data to an inline descriptor in the root
	 * signature.
	 */
	void SetGraphicsDynamicConstantBuffer(uint32_t root_parameter_index, size_t size_in_bytes, const void* buffer_data);

	template <typename T>
	void SetGraphicsDynamicConstantBuffer(uint32_t root_parameter_index, const T& data)
	{
		SetGraphicsDynamicConstantBuffer(root_parameter_index, sizeof(T), &data);
	}

	/**
	 * Set a set of 32-bit constants on the graphics pipeline.
	 */
	void SetGraphics32BitConstants(uint32_t root_parameter_index, uint32_t num_constants, const void* constants);

	template <typename T>
	void SetGraphics32BitConstants(uint32_t root_parameter_index, const T& constants)
	{
		static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
		SetGraphics32BitConstants(root_parameter_index, sizeof(T) / sizeof(uint32_t), &constants);
	}

	/**
	* Set a dynamic constant buffer data to an inline descriptor in the root
	* signature.
	*/
	void SetComputeDynamicConstantBuffer(uint32_t root_parameter_index, size_t size_in_bytes, const void* buffer_data);

	template <typename T>
	void SetComputeDynamicConstantBuffer(uint32_t root_parameter_index, const T& data)
	{
		SetComputeDynamicConstantBuffer(root_parameter_index, sizeof(T), &data);
	}
	
	/**
	 * Set a set of 32-bit constants on the compute pipeline.
	 */
	void SetCompute32BitConstants(uint32_t root_parameter_index, uint32_t num_constants, const void* constants);

	template <typename T>
	void SetCompute32BitConstants(uint32_t root_parameter_index, const T& constants)
	{
		static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
		SetCompute32BitConstants(root_parameter_index, sizeof(T) / sizeof(uint32_t), &constants);
	}

	void SetComputeAccelerationStructure(uint32_t root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address);

	/**
	* Set dynamic structured buffer contents.
	*/
	void SetComputeDynamicStructuredBuffer(uint32_t slot, size_t num_elements, size_t element_size,
		const void* buffer_data);

	template <typename T>
	void SetComputeDynamicStructuredBuffer(uint32_t slot, const std::vector<T>& buffer_data)
	{
		SetComputeDynamicStructuredBuffer(slot, buffer_data.size(), sizeof(T), buffer_data.data());
	}

	
	/**
	 * Set the vertex buffer to the rendering pipeline.
	 */
	void SetVertexBuffer(uint32_t slot, const VertexBuffer& vertex_buffer);

	/**
	 * Set dynamic vertex buffer data to the rendering pipeline.
	 */
	void SetDynamicVertexBuffer(uint32_t slot, size_t num_vertices, size_t vertex_size, const void* vertex_buffer_data);

	template <typename T>
	void SetDynamicVertexBuffer(uint32_t slot, const std::vector<T>& vertex_buffer_data)
	{
		SetDynamicVertexBuffer(slot, vertex_buffer_data.size(), sizeof(T), vertex_buffer_data.data());
	}

	/**
	 * Bind the index buffer to the rendering pipeline.
	 */
	void SetIndexBuffer(const IndexBuffer& index_buffer);

	/**
	 * Bind dynamic index buffer data to the rendering pipeline.
	 */
	void SetDynamicIndexBuffer(size_t num_indicies, DXGI_FORMAT index_format, const void* index_buffer_data);

	template <typename T>
	void SetDynamicIndexBuffer(const std::vector<T>& index_buffer_data)
	{
		static_assert(sizeof(T) == 2 || sizeof(T) == 4);

		DXGI_FORMAT index_format = (sizeof(T) == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		SetDynamicIndexBuffer(index_buffer_data.size(), index_format, index_buffer_data.data());
	}

	/**
	 * Set dynamic structured buffer contents.
	 */
	void SetGraphicsDynamicStructuredBuffer(uint32_t slot, size_t num_elements, size_t element_size,
	                                        const void* buffer_data);

	template <typename T>
	void SetGraphicsDynamicStructuredBuffer(uint32_t slot, const std::vector<T>& buffer_data)
	{
		SetGraphicsDynamicStructuredBuffer(slot, buffer_data.size(), sizeof(T), buffer_data.data());
	}

	/**
	 * Set viewports.
	 */
	void SetViewport(const D3D12_VIEWPORT& viewport);
	void SetViewports(const std::vector<D3D12_VIEWPORT>& viewports);

	/**
	 * Set scissor rects.
	 */
	void SetScissorRect(const D3D12_RECT& scissor_rect);
	void SetScissorRects(const std::vector<D3D12_RECT>& scissor_rects);

	/**
	 * Set the pipeline State object on the command list.
	 */
	void SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state);

	/**
	* Set the State object on the command list.
	*/
	void SetStateObject(Microsoft::WRL::ComPtr<ID3D12StateObject> state_object);

	/**
	 * Set the current root signature on the command list.
	 */
	void SetGraphicsRootSignature(const RootSignature& root_signature);
	void SetComputeRootSignature(const RootSignature& root_signature);

	/**
	 * Set the SRV on the graphics pipeline.
	 */
	void SetShaderResourceView(
		uint32_t root_parameter_index,
		uint32_t descriptor_offset,
		const Resource& resource,
		D3D12_RESOURCE_STATES state_after = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		const D3D12_SHADER_RESOURCE_VIEW_DESC* srv = nullptr,
		UINT first_subresource = 0,
		UINT num_subresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES	
	);

	/**
	 * Set the UAV on the graphics pipeline.
	 */
	void SetUnorderedAccessView(
		uint32_t root_parameter_index,
		uint32_t descriptor_offset,
		const Resource& resource,
		D3D12_RESOURCE_STATES state_after = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		UINT first_subresource = 0,
		UINT num_subresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav = nullptr
	);

	/**
	* Begin render pass.
	*/
	void BeginRenderPass(const RenderTarget& render_target, D3D12_RENDER_PASS_FLAGS flags = D3D12_RENDER_PASS_FLAG_NONE);

	/**
	* End render pass.
	*/
	void EndRenderPass();
	
	/**
	 * Set the render targets for the graphics rendering pipeline.
	 */
	void SetRenderTarget(const RenderTarget& render_target);

	/**
	 * Draw geometry.
	 */
	void Draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t start_vertex = 0,
	          uint32_t start_instance = 0);
	void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t start_index = 0,
	                 int32_t base_vertex = 0, uint32_t start_instance = 0);

	/**
	 * Dispatch a compute shader.
	 */
	void Dispatch(uint32_t num_groups_x, uint32_t num_groups_y = 1, uint32_t num_groups_z = 1);

	/**
	* Dispatch rays.
	*/
	void DispatchRays(D3D12_DISPATCH_RAYS_DESC& dispatch_desc);
	

	/***************************************************************************
	 * Methods defined below are only intended to be used by internal classes. *
	 ***************************************************************************/

	/**
	 * Close the command list.
	 * Used by the command queue.
	 *
	 * @param pending_command_list The command list that is used to execute pending
	 * resource barriers (if any) for this command list.
	 *
	 * @return true if there are any pending resource barriers that need to be
	 * processed.
	 */
	bool Close(CommandList& pending_command_list);
	// Just close the command list. This is useful for pending command lists.
	void Close();

	/**
	 * Reset the command list. This should only be called by the CommandQueue
	 * before the command list is returned from CommandQueue::GetCommandList.
	 */
	void Reset();

	/**
	 * Release tracked objects. Useful if the swap chain needs to be resized.
	 */
	void ReleaseTrackedObjects();

	/**
	 * Set the currently bound descriptor heap.
	 * Should only be called by the DynamicDescriptorHeap class.
	 */
	void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heap_type, ID3D12DescriptorHeap* heap);

	std::shared_ptr<CommandList> GetGenerateMipsCommandList() const
	{
		return compute_command_list_;
	}

protected:

private:
	void TrackResource(Microsoft::WRL::ComPtr<ID3D12Object> object);
	void TrackResource(const Resource& res);

	// Generate mips for UAV compatible textures.
	void GenerateMipsUAV(Texture& texture, bool is_srgb);

	// Binds the current descriptor heaps to the command list.
	void BindDescriptorHeaps();

	using TrackedObjects = std::vector<Microsoft::WRL::ComPtr<ID3D12Object>>;

	D3D12_COMMAND_LIST_TYPE d3d12_command_list_type_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> d3d12_command_list_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> d3d12_command_allocator_;

	// For copy queues, it may be necessary to generate mips while loading textures.
	// Mips can't be generated on copy queues but must be generated on compute or
	// direct queues. In this case, a Compute command list is generated and executed 
	// after the copy queue is finished uploading the first sub resource.
	std::shared_ptr<CommandList> compute_command_list_;

	// Keep track of the currently bound root signatures to minimize root
	// signature changes.
	ID3D12RootSignature* root_signature_{};

	// Resource created in an upload heap. Useful for drawing of dynamic geometry
	// or for uploading constant buffer data that changes every draw call.
	std::unique_ptr<UploadBuffer> upload_buffer_;

	// Resource State tracker is used by the command list to track (per command list)
	// the current State of a resource. The resource State tracker also tracks the 
	// global State of a resource in order to minimize resource State transitions.
	std::unique_ptr<ResourceStateTracker> resource_state_tracker_;

	// The dynamic descriptor heap allows for descriptors to be staged before
	// being committed to the command list. Dynamic descriptors need to be
	// committed before a Draw or Dispatch.
	std::unique_ptr<DynamicDescriptorHeap> dynamic_descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	// Keep track of the currently bound descriptor heaps. Only change descriptor 
	// heaps if they are different than the currently bound descriptor heaps.
	ID3D12DescriptorHeap* descriptor_heaps_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};

	// Objects that are being tracked by a command list that is "in-flight" on 
	// the command-queue and cannot be deleted. To ensure objects are not deleted 
	// until the command list is finished executing, a reference to the object
	// is stored. The referenced objects are released when the command list is 
	// reset.
	TrackedObjects tracked_objects_;

	// Pipeline state object for Mip map generation.
	std::unique_ptr<GenerateMipsPSO> generate_mips_pso_;
};

