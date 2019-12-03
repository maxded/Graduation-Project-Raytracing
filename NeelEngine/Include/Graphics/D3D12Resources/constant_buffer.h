#pragma once

#include "buffer.h"
#include "descriptor_allocation.h"

class ConstantBuffer : public Buffer
{
public:
	ConstantBuffer(const std::string& name = "Constant Buffer");
	virtual ~ConstantBuffer();

	// Inherited from Buffer
	virtual void CreateViews(size_t num_elements, size_t element_size) override;

	size_t GetSizeInBytes() const
	{
		return size_in_bytes_;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetConstantBufferView() const
	{
		return constant_buffer_view_.GetDescriptorHandle();
	}

	/**
	* Get the SRV for a resource.
	*/
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(
		const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr) const override;

	/**
	* Get the UAV for a (sub)resource.
	*/
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr) const override;


protected:

private:
	size_t size_in_bytes_;
	DescriptorAllocation constant_buffer_view_;
};
