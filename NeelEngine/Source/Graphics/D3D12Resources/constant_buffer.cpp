#include "neel_engine_pch.h"

#include "constant_buffer.h"

#include "neel_engine.h"


ConstantBuffer::ConstantBuffer(const std::string& name)
	: Buffer(name)
	  , size_in_bytes_(0)
{
	constant_buffer_view_ = NeelEngine::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

ConstantBuffer::~ConstantBuffer()
{
}

void ConstantBuffer::CreateViews(size_t num_elements, size_t element_size)
{
	size_in_bytes_ = num_elements * element_size;

	D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12_constant_buffer_view_desc;
	d3d12_constant_buffer_view_desc.BufferLocation = d3d12_resource_->GetGPUVirtualAddress();
	d3d12_constant_buffer_view_desc.SizeInBytes = static_cast<UINT>(math::AlignUp(size_in_bytes_, 16));

	auto device = NeelEngine::Get().GetDevice();

	device->CreateConstantBufferView(&d3d12_constant_buffer_view_desc, constant_buffer_view_.GetDescriptorHandle());
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc) const
{
	throw std::exception("ConstantBuffer::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBuffer::GetUnorderedAccessView(
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc) const
{
	throw std::exception("ConstantBuffer::GetUnorderedAccessView should not be called.");
}
