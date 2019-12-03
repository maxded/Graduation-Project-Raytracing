#pragma once

#include "buffer.h"
#include "descriptor_allocation.h"

class ByteAddressBuffer : public Buffer
{
public:
	ByteAddressBuffer(const std::string& name = "Byte Address Buffer");
	ByteAddressBuffer(const D3D12_RESOURCE_DESC& res_desc,
	                  size_t num_elements, size_t element_size,
	                  const std::string& name = "Byte Address Buffer");

	size_t GetBufferSize() const
	{
		return buffer_size_;
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
		const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr) const override
	{
		return srv_.GetDescriptorHandle();
	}

	/**
	 * Get the UAV for a (sub)resource.
	 */
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr) const override
	{
		// Buffers only have a single subresource.
		return uav_.GetDescriptorHandle();
	}

protected:

private:
	size_t buffer_size_;

	DescriptorAllocation srv_;
	DescriptorAllocation uav_;
};
