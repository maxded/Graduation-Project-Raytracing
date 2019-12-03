#include "neel_engine_pch.h"

#include "structured_buffer.h"

#include "neel_engine.h"


StructuredBuffer::StructuredBuffer(const std::string& name)
	: Buffer(name)
	  , num_elements_(0)
	  , element_size_(0)
	  , counter_buffer_(
		  CD3DX12_RESOURCE_DESC::Buffer(4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		  1, 4, name + " Counter")
{
	srv_ = NeelEngine::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	uav_ = NeelEngine::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

StructuredBuffer::StructuredBuffer(const D3D12_RESOURCE_DESC& res_desc,
                                   size_t num_elements, size_t element_size,
                                   const std::string& name)
	: Buffer(res_desc, num_elements, element_size, name)
	  , num_elements_(num_elements)
	  , element_size_(element_size)
	  , counter_buffer_(
		  CD3DX12_RESOURCE_DESC::Buffer(4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		  1, 4, name + " Counter")
{
	srv_ = NeelEngine::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	uav_ = NeelEngine::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void StructuredBuffer::CreateViews(size_t num_elements, size_t element_size)
{
	auto device = NeelEngine::Get().GetDevice();

	num_elements_ = num_elements;
	element_size_ = element_size;

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.Buffer.NumElements = static_cast<UINT>(num_elements_);
	srv_desc.Buffer.StructureByteStride = static_cast<UINT>(element_size_);
	srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	device->CreateShaderResourceView(d3d12_resource_.Get(),
	                                 &srv_desc,
	                                 srv_.GetDescriptorHandle());

	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
	uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uav_desc.Format = DXGI_FORMAT_UNKNOWN;
	uav_desc.Buffer.CounterOffsetInBytes = 0;
	uav_desc.Buffer.NumElements = static_cast<UINT>(num_elements_);
	uav_desc.Buffer.StructureByteStride = static_cast<UINT>(element_size_);
	uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	device->CreateUnorderedAccessView(d3d12_resource_.Get(),
	                                  counter_buffer_.GetD3D12Resource().Get(),
	                                  &uav_desc,
	                                  uav_.GetDescriptorHandle());
}
