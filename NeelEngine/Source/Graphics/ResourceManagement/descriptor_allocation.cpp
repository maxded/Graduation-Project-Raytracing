#include "neel_engine_pch.h"

#include "descriptor_allocation.h"

#include "neel_engine.h"
#include "descriptor_allocator_page.h"

DescriptorAllocation::DescriptorAllocation()
	: descriptor_{0}
	  , num_handles_(0)
	  , descriptor_size_(0)
	  , page_(nullptr)
{
}

DescriptorAllocation::DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t num_handles,
                                           uint32_t descriptor_size, std::shared_ptr<DescriptorAllocatorPage> page)
	: descriptor_(descriptor)
	  , num_handles_(num_handles)
	  , descriptor_size_(descriptor_size)
	  , page_(page)
{
}


DescriptorAllocation::~DescriptorAllocation()
{
	Free();
}

DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& allocation)
	: descriptor_(allocation.descriptor_)
	  , num_handles_(allocation.num_handles_)
	  , descriptor_size_(allocation.descriptor_size_)
	  , page_(std::move(allocation.page_))
{
	allocation.descriptor_.ptr = 0;
	allocation.num_handles_ = 0;
	allocation.descriptor_size_ = 0;
}

DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& other)
{
	// Free this descriptor if it points to anything.
	Free();

	descriptor_ = other.descriptor_;
	num_handles_ = other.num_handles_;
	descriptor_size_ = other.descriptor_size_;
	page_ = std::move(other.page_);

	other.descriptor_.ptr = 0;
	other.num_handles_ = 0;
	other.descriptor_size_ = 0;

	return *this;
}

void DescriptorAllocation::Free()
{
	if (!IsNull() && page_)
	{
		page_->Free(std::move(*this), NeelEngine::GetFrameCount());

		descriptor_.ptr = 0;
		num_handles_ = 0;
		descriptor_size_ = 0;
		page_.reset();
	}
}

// Check if this a valid descriptor.
bool DescriptorAllocation::IsNull() const
{
	return descriptor_.ptr == 0;
}

// Get a descriptor at a particular offset in the allocation.
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetDescriptorHandle(uint32_t offset) const
{
	assert(offset < num_handles_);
	return {descriptor_.ptr + (descriptor_size_ * offset)};
}

uint32_t DescriptorAllocation::GetNumHandles() const
{
	return num_handles_;
}

std::shared_ptr<DescriptorAllocatorPage> DescriptorAllocation::GetDescriptorAllocatorPage() const
{
	return page_;
}
