#include "neel_engine_pch.h"

#include "upload_buffer.h"
#include "neel_engine.h"

UploadBuffer::UploadBuffer(size_t page_size)
	: page_pool_({})
	  , available_pages_({})
	  , current_page_(nullptr)
	  , page_size_(page_size)
{
}

UploadBuffer::~UploadBuffer()
{
}

UploadBuffer::Allocation UploadBuffer::Allocate(size_t size_in_bytes, size_t alignment)
{
	if (size_in_bytes > page_size_)
	{
		throw std::bad_alloc();
	}

	// If there is no current page, or the requested allocation exceeds the
	// remaining space in the current page, request a new page.
	if (!current_page_ || !current_page_->HasSpace(size_in_bytes, alignment))
	{
		current_page_ = RequestPage();
	}

	return current_page_->Allocate(size_in_bytes, alignment);
}

std::shared_ptr<UploadBuffer::Page> UploadBuffer::RequestPage()
{
	std::shared_ptr<Page> page;

	if (!available_pages_.empty())
	{
		page = available_pages_.front();
		available_pages_.pop_front();
	}
	else
	{
		page = std::make_shared<Page>(page_size_);
		page_pool_.push_back(page);
	}

	return page;
}

void UploadBuffer::Reset()
{
	current_page_ = nullptr;
	// Reset all available pages
	available_pages_ = page_pool_;

	for (auto page : available_pages_)
	{
		page->Reset();
	}
}

UploadBuffer::Page::Page(size_t size_in_bytes)
	: page_size_(size_in_bytes)
	  , offset_(0)
	  , cpu_ptr_(nullptr)
	  , gpu_ptr_(D3D12_GPU_VIRTUAL_ADDRESS(0))
{
	auto device = NeelEngine::Get().GetDevice();

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(page_size_),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&d3d12_resource_)
	));

	gpu_ptr_ = d3d12_resource_->GetGPUVirtualAddress();
	d3d12_resource_->Map(0, nullptr, &cpu_ptr_);
}

UploadBuffer::Page::~Page()
{
	d3d12_resource_->Unmap(0, nullptr);
	cpu_ptr_ = nullptr;
	gpu_ptr_ = D3D12_GPU_VIRTUAL_ADDRESS(0);
}

bool UploadBuffer::Page::HasSpace(size_t size_in_bytes, size_t alignment) const
{
	size_t aligned_size = math::AlignUp(size_in_bytes, alignment);
	size_t aligned_offset = math::AlignUp(offset_, alignment);

	return aligned_offset + aligned_size <= page_size_;
}

UploadBuffer::Allocation UploadBuffer::Page::Allocate(size_t size_in_bytes, size_t alignment)
{
	size_t aligned_size = math::AlignUp(size_in_bytes, alignment);
	offset_ = math::AlignUp(offset_, alignment);

	Allocation allocation;
	allocation.CPU = static_cast<uint8_t*>(cpu_ptr_) + offset_;
	allocation.GPU = gpu_ptr_ + offset_;

	offset_ += aligned_size;

	return allocation;
}

void UploadBuffer::Page::Reset()
{
	offset_ = 0;
}
