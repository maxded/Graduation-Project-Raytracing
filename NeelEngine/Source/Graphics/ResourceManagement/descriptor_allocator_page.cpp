#include "neel_engine_pch.h"

#include "descriptor_allocator_page.h"
#include "neel_engine.h"

DescriptorAllocatorPage::DescriptorAllocatorPage(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t num_descriptors)
	: heap_type_(type)
	  , num_descriptors_in_heap_(num_descriptors)
{
	auto device = NeelEngine::Get().GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.Type = heap_type_;
	heap_desc.NumDescriptors = num_descriptors_in_heap_;

	ThrowIfFailed(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&d3d12_descriptor_heap_)));

	base_descriptor_ = d3d12_descriptor_heap_->GetCPUDescriptorHandleForHeapStart();
	descriptor_handle_increment_size_ = device->GetDescriptorHandleIncrementSize(heap_type_);
	num_free_handles_ = num_descriptors_in_heap_;

	// Initialize the free lists
	AddNewBlock(0, num_free_handles_);
}

D3D12_DESCRIPTOR_HEAP_TYPE DescriptorAllocatorPage::GetHeapType() const
{
	return heap_type_;
}

uint32_t DescriptorAllocatorPage::NumFreeHandles() const
{
	return num_free_handles_;
}

bool DescriptorAllocatorPage::HasSpace(uint32_t num_descriptors) const
{
	return free_list_by_size_.lower_bound(num_descriptors) != free_list_by_size_.end();
}

void DescriptorAllocatorPage::AddNewBlock(uint32_t offset, uint32_t num_descriptors)
{
	auto offset_it = free_list_by_offset_.emplace(offset, num_descriptors);
	auto size_it = free_list_by_size_.emplace(num_descriptors, offset_it.first);
	offset_it.first->second.FreeListBySizeIt = size_it;
}

DescriptorAllocation DescriptorAllocatorPage::Allocate(uint32_t num_descriptors)
{
	std::lock_guard<std::mutex> lock(allocation_mutex_);

	// There are less than the requested number of descriptors left in the heap.
	// Return a NULL descriptor and try another heap.
	if (num_descriptors > num_free_handles_)
	{
		return DescriptorAllocation();
	}

	// Get the first block that is large enough to satisfy the request.
	auto smallest_block_it = free_list_by_size_.lower_bound(num_descriptors);
	if (smallest_block_it == free_list_by_size_.end())
	{
		// There was no free block that could satisfy the request.
		return DescriptorAllocation();
	}

	// The size of the smallest block that satisfies the request.
	auto block_size = smallest_block_it->first;

	// The pointer to the same entry in the FreeListByOffset map.
	auto offset_it = smallest_block_it->second;

	// The offset in the descriptor heap.
	auto offset = offset_it->first;

	// Remove the existing free block from the free list.
	free_list_by_size_.erase(smallest_block_it);
	free_list_by_offset_.erase(offset_it);

	// Compute the new free block that results from splitting this block.
	auto new_offset = offset + num_descriptors;
	auto new_size = block_size - num_descriptors;

	if (new_size > 0)
	{
		// If the allocation didn't exactly match the requested size,
		// return the left-over to the free list.
		AddNewBlock(new_offset, new_size);
	}

	// Decrement free handles.
	num_free_handles_ -= num_descriptors;

	return DescriptorAllocation(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(base_descriptor_, offset, descriptor_handle_increment_size_),
		num_descriptors, descriptor_handle_increment_size_, shared_from_this());
}

uint32_t DescriptorAllocatorPage::ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle) const
{
	return static_cast<uint32_t>(handle.ptr - base_descriptor_.ptr) / descriptor_handle_increment_size_;
}

void DescriptorAllocatorPage::Free(DescriptorAllocation&& descriptor, uint64_t frame_number)
{
	// Compute the offset of the descriptor within the descriptor heap.
	auto offset = ComputeOffset(descriptor.GetDescriptorHandle());

	std::lock_guard<std::mutex> lock(allocation_mutex_);

	// Don't add the block directly to the free list until the frame has completed.
	stale_descriptors_.emplace(offset, descriptor.GetNumHandles(), frame_number);
}

void DescriptorAllocatorPage::FreeBlock(uint32_t offset, uint32_t num_descriptors)
{
	// Find the first element whose offset is greater than the specified offset.
	// This is the block that should appear after the block that is being freed.
	auto next_block_it = free_list_by_offset_.upper_bound(offset);

	// Find the block that appears before the block being freed.
	auto prev_block_it = next_block_it;
	// If it's not the first block in the list.
	if (prev_block_it != free_list_by_offset_.begin())
	{
		// Go to the previous block in the list.
		--prev_block_it;
	}
	else
	{
		// Otherwise, just set it to the end of the list to indicate that no
		// block comes before the one being freed.
		prev_block_it = free_list_by_offset_.end();
	}

	// Add the number of free handles back to the heap.
	// This needs to be done before merging any blocks since merging
	// blocks modifies the numDescriptors variable.
	num_free_handles_ += num_descriptors;

	if (prev_block_it != free_list_by_offset_.end() &&
		offset == prev_block_it->first + prev_block_it->second.Size)
	{
		// The previous block is exactly behind the block that is to be freed.
		//
		// PrevBlock.Offset           Offset
		// |                          |
		// |<-----PrevBlock.Size----->|<------Size-------->|
		//

		// Increase the block size by the size of merging with the previous block.
		offset = prev_block_it->first;
		num_descriptors += prev_block_it->second.Size;

		// Remove the previous block from the free list.
		free_list_by_size_.erase(prev_block_it->second.FreeListBySizeIt);
		free_list_by_offset_.erase(prev_block_it);
	}

	if (next_block_it != free_list_by_offset_.end() &&
		offset + num_descriptors == next_block_it->first)
	{
		// The next block is exactly in front of the block that is to be freed.
		//
		// Offset               NextBlock.Offset 
		// |                    |
		// |<------Size-------->|<-----NextBlock.Size----->|

		// Increase the block size by the size of merging with the next block.
		num_descriptors += next_block_it->second.Size;

		// Remove the next block from the free list.
		free_list_by_size_.erase(next_block_it->second.FreeListBySizeIt);
		free_list_by_offset_.erase(next_block_it);
	}

	// Add the freed block to the free list.
	AddNewBlock(offset, num_descriptors);
}

void DescriptorAllocatorPage::ReleaseStaleDescriptors(uint64_t frame_number)
{
	std::lock_guard<std::mutex> lock(allocation_mutex_);

	while (!stale_descriptors_.empty() && stale_descriptors_.front().FrameNumber <= frame_number)
	{
		auto& stale_descriptor = stale_descriptors_.front();

		// The offset of the descriptor in the heap.
		auto offset = stale_descriptor.Offset;
		// The number of descriptors that were allocated.
		auto num_descriptors = stale_descriptor.Size;

		FreeBlock(offset, num_descriptors);

		stale_descriptors_.pop();
	}
}
