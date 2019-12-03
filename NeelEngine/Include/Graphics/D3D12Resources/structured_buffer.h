#pragma once

#include "buffer.h"

#include "byte_address_buffer.h"

class StructuredBuffer : public Buffer
{
public:
	StructuredBuffer(const std::string& name = "Structured Buffer");
	StructuredBuffer(const D3D12_RESOURCE_DESC& res_desc,
	                 size_t num_elements, size_t element_size,
	                 const std::string& name = "Structured Buffer");

	/**
	* Get the number of elements contained in this buffer.
	*/
	virtual size_t GetNumElements() const
	{
		return num_elements_;
	}

	/**
	* Get the size in bytes of each element in this buffer.
	*/
	virtual size_t GetElementSize() const
	{
		return element_size_;
	}

	/**
	 * Create the views for the buffer resource.
	 * Used by the CommandList when setting the buffer contents.
	 */
	virtual void CreateViews(size_t num_elements, size_t element_size) override;

	/**
	 * Get the SRV for a resource.
	 */
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(
		const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr) const
	{
		return srv_.GetDescriptorHandle();
	}

	/**
	 * Get the UAV for a (sub)resource.
	 */
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr) const override
	{
		// Buffers don't have subresources.
		return uav_.GetDescriptorHandle();
	}

	const ByteAddressBuffer& GetCounterBuffer() const
	{
		return counter_buffer_;
	}

private:
	size_t num_elements_;
	size_t element_size_;

	DescriptorAllocation srv_;
	DescriptorAllocation uav_;

	// A buffer to store the internal counter for the structured buffer.
	ByteAddressBuffer counter_buffer_;
};
