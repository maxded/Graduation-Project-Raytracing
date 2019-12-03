#pragma once
#include <defines.h>

#include <wrl.h>
#include <d3d12.h>

#include <memory>
#include <deque>

/**
 * An UploadBuffer provides a convenient method to upload resources to the GPU.
 */
class UploadBuffer
{
public:
	struct Allocation
	{
		uint8_t* CPU;
		D3D12_GPU_VIRTUAL_ADDRESS GPU;
	};

	/**
	* @param page_size The size to use to allocate new pages in GPU memory.
	*/
	explicit UploadBuffer(size_t page_size = _2MB);

	virtual ~UploadBuffer();

	/**
	 * The maximum size of an allocation is the size of a single page.
	 */
	size_t GetPageSize() const { return page_size_; }

	/**
	 * Allocate memory in an Upload heap.
	 * An allocation must not exceed the size of a page.
	 * Use a memcpy or similar method to copy the
	 * buffer data to CPU pointer in the Allocation structure returned from
	 * this function.
	 */
	Allocation Allocate(size_t size_in_bytes, size_t alignment);

	/**
	 * Release all allocated pages. This should only be done when the command list
	 * is finished executing on the CommandQueue.
	 */
	void Reset();

private:
	// A single page for the allocator.
	struct Page
	{
		Page(size_t size_in_bytes);
		~Page();

		// Check to see if the page has room to satisfy the requested
		// allocation.
		bool HasSpace(size_t size_in_bytes, size_t alignment) const;

		// Allocate memory from the page.
		// Throws std::bad_alloc if the the allocation size is larger
		// that the page size or the size of the allocation exceeds the 
		// remaining space in the page.
		Allocation Allocate(size_t size_in_bytes, size_t alignment);

		// Reset the page for reuse.
		void Reset();

	private:

		Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource_;

		// Base pointer.
		void* cpu_ptr_;
		D3D12_GPU_VIRTUAL_ADDRESS gpu_ptr_;

		// Allocated page size.
		size_t page_size_;
		// Current allocation offset in bytes.
		size_t offset_;
	};

	// A pool of memory pages.
	using PagePool = std::deque<std::shared_ptr<Page>>;

	// Request a page from the pool of available pages
	// or create a new page if there are no available pages.
	std::shared_ptr<Page> RequestPage();

	PagePool page_pool_;
	PagePool available_pages_;

	std::shared_ptr<Page> current_page_;

	// The size of each page of memory.
	size_t page_size_;
};
