#include "neel_engine_pch.h"
#include "index_buffer.h"

IndexBuffer::IndexBuffer(const std::string& name)
	: Buffer(name)
	  , num_indicies_(0)
	  , index_format_(DXGI_FORMAT_UNKNOWN)
	  , index_buffer_view_({})
{
}

IndexBuffer::~IndexBuffer()
{
}

void IndexBuffer::CreateViews(size_t num_elements, size_t element_size)
{
	assert(element_size == 2 || element_size == 4 && "Indices must be 16, or 32-bit integers.");

	num_indicies_ = num_elements;
	index_format_ = (element_size == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	index_buffer_view_.BufferLocation = d3d12_resource_->GetGPUVirtualAddress();
	index_buffer_view_.SizeInBytes = static_cast<UINT>(num_elements * element_size);
	index_buffer_view_.Format = index_format_;
}

void IndexBuffer::CreateViews(uint32_t num_elements, uint32_t total_size, DXGI_FORMAT format)
{
	num_indicies_ = num_elements;
	index_format_ = format;

	index_buffer_view_.BufferLocation = d3d12_resource_->GetGPUVirtualAddress();
	index_buffer_view_.SizeInBytes = static_cast<UINT>(total_size);
	index_buffer_view_.Format = index_format_;
}

D3D12_CPU_DESCRIPTOR_HANDLE IndexBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc) const
{
	throw std::exception("IndexBuffer::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE IndexBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc) const
{
	throw std::exception("IndexBuffer::GetUnorderedAccessView should not be called.");
}
