#pragma once

#include "descriptor_allocation.h"

#include <d3d12.h>

#include <wrl.h>

#include <map>
#include <memory>
#include <mutex>
#include <queue>

class DescriptorAllocatorPage : public std::enable_shared_from_this<DescriptorAllocatorPage>
{
public:
	DescriptorAllocatorPage(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t num_descriptors);

	D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() const;

	/**
	* Check to see if this descriptor page has a contiguous block of descriptors
	* large enough to satisfy the request.
	*/
	bool HasSpace(uint32_t num_descriptors) const;

	/**
	* Get the number of available handles in the heap.
	*/
	uint32_t NumFreeHandles() const;

	/**
	* Allocate a number of descriptors from this descriptor heap.
	* If the allocation cannot be satisfied, then a NULL descriptor
	* is returned.
	*/
	DescriptorAllocation Allocate(uint32_t num_descriptors);

	/**
	* Return a descriptor back to the heap.
	* @param frame_number Stale descriptors are not freed directly, but put
	* on a stale allocations queue. Stale allocations are returned to the heap
	* using the DescriptorAllocatorPage::ReleaseStaleAllocations method.
	*/
	void Free(DescriptorAllocation&& descriptor_handle, uint64_t frame_number);

	/**
	* Returned the stale descriptors back to the descriptor heap.
	*/
	void ReleaseStaleDescriptors(uint64_t frame_number);

protected:

	// Compute the offset of the descriptor handle from the start of the heap.
	uint32_t ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle) const;

	// Adds a new block to the free list.
	void AddNewBlock(uint32_t offset, uint32_t num_descriptors);

	// Free a block of descriptors.
	// This will also merge free blocks in the free list to form larger blocks
	// that can be reused.
	void FreeBlock(uint32_t offset, uint32_t num_descriptors);

private:
	// The offset (in descriptors) within the descriptor heap.
	using OffsetType = uint32_t;
	// The number of descriptors that are available.
	using SizeType = uint32_t;

	struct FreeBlockInfo;
	// A map that lists the free blocks by the offset within the descriptor heap.
	using FreeListByOffset = std::map<OffsetType, FreeBlockInfo>;

	// A map that lists the free blocks by size.
	// Needs to be a multimap since multiple blocks can have the same size.
	using FreeListBySize = std::multimap<SizeType, FreeListByOffset::iterator>;

	struct FreeBlockInfo
	{
		FreeBlockInfo(SizeType size)
			: Size(size)
		{
		}

		SizeType Size;
		FreeListBySize::iterator FreeListBySizeIt;
	};

	struct StaleDescriptorInfo
	{
		StaleDescriptorInfo(OffsetType offset, SizeType size, uint64_t frame)
			: Offset(offset)
			  , Size(size)
			  , FrameNumber(frame)
		{
		}

		// The offset within the descriptor heap.
		OffsetType Offset;
		// The number of descriptors
		SizeType Size;
		// The frame number that the descriptor was freed.
		uint64_t FrameNumber;
	};

	// Stale descriptors are queued for release until the frame that they were freed
	// has completed.
	using StaleDescriptorQueue = std::queue<StaleDescriptorInfo>;

	FreeListByOffset free_list_by_offset_;
	FreeListBySize free_list_by_size_;
	StaleDescriptorQueue stale_descriptors_;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> d3d12_descriptor_heap_;
	D3D12_DESCRIPTOR_HEAP_TYPE heap_type_;
	CD3DX12_CPU_DESCRIPTOR_HANDLE base_descriptor_;
	uint32_t descriptor_handle_increment_size_;
	uint32_t num_descriptors_in_heap_;
	uint32_t num_free_handles_;

	std::mutex allocation_mutex_;
};
