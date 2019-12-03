#include "neel_engine_pch.h"
#include "vertex_buffer.h"

VertexBuffer::VertexBuffer(const std::string& name)
	: Buffer(name)
	  , num_vertices_(0)
	  , vertex_stride_(0)
	  , gpu_offset_(0)
	  , vertex_buffer_views_({})
{
}

VertexBuffer::~VertexBuffer()
{
}

void VertexBuffer::CreateViews(size_t num_elements, size_t element_size)
{
	num_vertices_ = num_elements;
	vertex_stride_ = element_size;

	vertex_buffer_views_.resize(1);

	vertex_buffer_views_[0].BufferLocation = d3d12_resource_->GetGPUVirtualAddress();
	vertex_buffer_views_[0].SizeInBytes = static_cast<UINT>(num_vertices_ * vertex_stride_);
	vertex_buffer_views_[0].StrideInBytes = static_cast<UINT>(vertex_stride_);
}

void VertexBuffer::CreateViews(std::vector<uint32_t> num_elements, std::vector<uint32_t> element_size)
{
	vertex_buffer_views_.resize(num_elements.size());

	for (int i = 0; i < num_elements.size(); i++)
	{
		uint32_t num_vertices = num_elements[i];
		uint32_t elem_size = element_size[i];

		vertex_buffer_views_[i].BufferLocation = d3d12_resource_->GetGPUVirtualAddress() + gpu_offset_;
		vertex_buffer_views_[i].SizeInBytes = static_cast<UINT>(num_vertices * elem_size);
		vertex_buffer_views_[i].StrideInBytes = static_cast<UINT>(elem_size);

		gpu_offset_ = num_vertices * elem_size;
	}

	num_vertices_ = num_elements[0];
	vertex_stride_ = 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE VertexBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc) const
{
	throw std::exception("VertexBuffer::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE VertexBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc) const
{
	throw std::exception("VertexBuffer::GetUnorderedAccessView should not be called.");
}
