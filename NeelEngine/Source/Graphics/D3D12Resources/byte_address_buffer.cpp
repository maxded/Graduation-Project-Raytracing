#include "neel_engine_pch.h"

#include "byte_address_buffer.h"

#include "neel_engine.h"

ByteAddressBuffer::ByteAddressBuffer(const std::string& name)
	: Buffer(name)
	, buffer_size_(0)
{
	srv_ = NeelEngine::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	uav_ = NeelEngine::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

ByteAddressBuffer::ByteAddressBuffer(const D3D12_RESOURCE_DESC& res_desc,
                                     size_t num_elements, size_t element_size,
                                     const std::string& name)
	: Buffer(res_desc, num_elements, element_size, name)
	, buffer_size_(0)
{
}

void ByteAddressBuffer::CreateViews(size_t num_elements, size_t element_size)
{
	auto device = NeelEngine::Get().GetDevice();

	// Make sure buffer size is aligned to 4 bytes.
	buffer_size_ = math::AlignUp(num_elements * element_size, 4);

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.Buffer.NumElements = static_cast<UINT>(buffer_size_) / 4;
	srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	device->CreateShaderResourceView(d3d12_resource_.Get(), &srv_desc, srv_.GetDescriptorHandle());

	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
	uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
	uav_desc.Buffer.NumElements = static_cast<UINT>(buffer_size_) / 4;
	uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

	device->CreateUnorderedAccessView(d3d12_resource_.Get(), nullptr, &uav_desc, uav_.GetDescriptorHandle());
}
