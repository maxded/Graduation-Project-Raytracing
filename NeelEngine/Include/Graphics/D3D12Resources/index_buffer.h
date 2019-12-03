#pragma once

#include "buffer.h"

class IndexBuffer : public Buffer
{
public:
	IndexBuffer(const std::string& name = "Index Buffer");
	virtual ~IndexBuffer();

	// Inherited from Buffer
	virtual void CreateViews(size_t num_elements, size_t element_size) override;

	void CreateViews(uint32_t num_elements, uint32_t total_size, DXGI_FORMAT format);

	size_t GetNumIndicies() const
	{
		return num_indicies_;
	}

	DXGI_FORMAT GetIndexFormat() const
	{
		return index_format_;
	}

	/**
	 * Get the index buffer view for biding to the Input Assembler stage.
	 */
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const
	{
		return index_buffer_view_;
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
	size_t num_indicies_;
	DXGI_FORMAT index_format_;

	D3D12_INDEX_BUFFER_VIEW index_buffer_view_;
};
