#include "neel_engine_pch.h"
#include "descriptor_allocator.h"
#include "descriptor_allocator_page.h"

DescriptorAllocator::DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t num_descriptors_per_heap)
	: heap_type_(type)
	  , num_descriptors_per_heap_(num_descriptors_per_heap)
{
}

DescriptorAllocator::~DescriptorAllocator()
{
}

std::shared_ptr<DescriptorAllocatorPage> DescriptorAllocator::CreateAllocatorPage()
{
	auto new_page = std::make_shared<DescriptorAllocatorPage>(heap_type_, num_descriptors_per_heap_);

	heap_pool_.emplace_back(new_page);
	available_heaps_.insert(heap_pool_.size() - 1);

	return new_page;
}

DescriptorAllocation DescriptorAllocator::Allocate(uint32_t num_descriptors)
{
	std::lock_guard<std::mutex> lock(allocation_mutex_);

	DescriptorAllocation allocation;

	for (auto iter = available_heaps_.begin(); iter != available_heaps_.end(); ++iter)
	{
		auto allocator_page = heap_pool_[*iter];

		allocation = allocator_page->Allocate(num_descriptors);

		if (allocator_page->NumFreeHandles() == 0)
		{
			iter = available_heaps_.erase(iter);
		}

		// A valid allocation has been found.
		if (!allocation.IsNull())
		{
			break;
		}
	}

	// No available heap could satisfy the requested number of descriptors.
	if (allocation.IsNull())
	{
		num_descriptors_per_heap_ = std::max(num_descriptors_per_heap_, num_descriptors);
		auto new_page = CreateAllocatorPage();

		allocation = new_page->Allocate(num_descriptors);
	}

	return allocation;
}

void DescriptorAllocator::ReleaseStaleDescriptors(uint64_t frame_number)
{
	std::lock_guard<std::mutex> lock(allocation_mutex_);

	for (size_t i = 0; i < heap_pool_.size(); ++i)
	{
		auto page = heap_pool_[i];

		page->ReleaseStaleDescriptors(frame_number);

		if (page->NumFreeHandles() > 0)
		{
			available_heaps_.insert(i);
		}
	}
}
