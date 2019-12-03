#pragma once

#include "d3dx12.h"

#include <wrl.h>

#include <cstdint>
#include <memory>
#include <queue>
#include <functional>

class CommandList;
class RootSignature;

class DynamicDescriptorHeap
{
public:
	DynamicDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE heap_type,
		uint32_t num_descriptors_per_heap = 1024);

	virtual ~DynamicDescriptorHeap();

	/**
	 * Stages a contiguous range of CPU visible descriptors.
	 * Descriptors are not copied to the GPU visible descriptor heap until
	 * the CommitStagedDescriptors function is called.
	 */
	void StageDescriptors(uint32_t root_parameter_index, uint32_t offset, uint32_t num_descriptors,
	                      const D3D12_CPU_DESCRIPTOR_HANDLE src_descriptors);

	/**
	 * Copy all of the staged descriptors to the GPU visible descriptor heap and
	 * bind the descriptor heap and the descriptor tables to the command list.
	 * The passed-in function object is used to set the GPU visible descriptors
	 * on the command list. Two possible functions are:
	 *   * Before a draw    : ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable
	 *   * Before a dispatch: ID3D12GraphicsCommandList::SetComputeRootDescriptorTable
	 *
	 * Since the DynamicDescriptorHeap can't know which function will be used, it must
	 * be passed as an argument to the function.
	 */
	void CommitStagedDescriptors(CommandList& command_list,
	                             std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)>
	                             set_func);
	void CommitStagedDescriptorsForDraw(CommandList& command_list);
	void CommitStagedDescriptorsForDispatch(CommandList& command_list);

	/**
	 * Copies a single CPU visible descriptor to a GPU visible descriptor heap.
	 * This is useful for the
	 *   * ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat
	 *   * ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint
	 * methods which require both a CPU and GPU visible descriptors for a UAV
	 * resource.
	 *
	 * @param commandList The command list is required in case the GPU visible
	 * descriptor heap needs to be updated on the command list.
	 * @param cpu_descriptor The CPU descriptor to copy into a GPU visible
	 * descriptor heap.
	 *
	 * @return The GPU visible descriptor.
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptor(CommandList& comand_list, D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor);

	/**
	 * Parse the root signature to determine which root parameters contain
	 * descriptor tables and determine the number of descriptors needed for
	 * each table.
	 */
	void ParseRootSignature(const RootSignature& root_signature);

	/**
	 * Reset used descriptors. This should only be done if any descriptors
	 * that are being referenced by a command list has finished executing on the
	 * command queue.
	 */
	void Reset();

protected:

private:
	// Request a descriptor heap if one is available.
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RequestDescriptorHeap();
	// Create a new descriptor heap of no descriptor heap is available.
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap() const;

	// Compute the number of stale descriptors that need to be copied
	// to GPU visible descriptor heap.
	uint32_t ComputeStaleDescriptorCount() const;

	/**
	 * The maximum number of descriptor tables per root signature.
	 * A 32-bit mask is used to keep track of the root parameter indices that
	 * are descriptor tables.
	 */
	static const uint32_t max_descriptor_tables_ = 32;

	/**
	 * A structure that represents a descriptor table entry in the root signature.
	 */
	struct DescriptorTableCache
	{
		DescriptorTableCache()
			: NumDescriptors(0)
			  , BaseDescriptor(nullptr)
		{
		}

		// Reset the table cache.
		void Reset()
		{
			NumDescriptors = 0;
			BaseDescriptor = nullptr;
		}

		// The number of descriptors in this descriptor table.
		uint32_t NumDescriptors;
		// The pointer to the descriptor in the descriptor handle cache.
		D3D12_CPU_DESCRIPTOR_HANDLE* BaseDescriptor;
	};

	// Describes the type of descriptors that can be staged using this 
	// dynamic descriptor heap.
	// Valid values are:
	//   * D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	//   * D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
	// This parameter also determines the type of GPU visible descriptor heap to 
	// create.
	D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type_;

	// The number of descriptors to allocate in new GPU visible descriptor heaps.
	uint32_t num_descriptors_per_heap_;

	// The increment size of a descriptor.
	uint32_t descriptor_handle_increment_size_;

	// The descriptor handle cache.
	std::unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> descriptor_handle_cache_;

	// Descriptor handle cache per descriptor table.
	DescriptorTableCache descriptor_table_cache_[max_descriptor_tables_];

	// Each bit in the bit mask represents the index in the root signature
	// that contains a descriptor table.
	uint32_t descriptor_table_bit_mask_;
	// Each bit set in the bit mask represents a descriptor table
	// in the root signature that has changed since the last time the 
	// descriptors were copied.
	uint32_t stale_descriptor_table_bit_mask_;

	using DescriptorHeapPool = std::queue<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>>;

	DescriptorHeapPool descriptor_heap_pool_;
	DescriptorHeapPool available_descriptor_heaps_;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> current_descriptor_heap_;
	CD3DX12_GPU_DESCRIPTOR_HANDLE current_gpu_descriptor_handle_;
	CD3DX12_CPU_DESCRIPTOR_HANDLE current_cpu_descriptor_handle_;

	uint32_t num_free_handles_;
};
