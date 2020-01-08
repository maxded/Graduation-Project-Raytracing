#include "neel_engine_pch.h"

#include "shader_table.h"
#include "neel_engine.h"
#include "helpers.h"


ShaderTable::ShaderTable(UINT num_shader_records, UINT shader_record_size, const std::string& resource_name)
	: Resource(resource_name)
	, shader_record_size_(shader_record_size)
{
	auto device = NeelEngine::Get().GetDevice();
	
	shader_record_size_ = math::AlignUp(shader_record_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	shader_records_.reserve(num_shader_records);

	UINT buffer_size = num_shader_records * shader_record_size_;

	// Allocate.
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
		, D3D12_HEAP_FLAG_NONE
		, &CD3DX12_RESOURCE_DESC::Buffer(buffer_size)
		, D3D12_RESOURCE_STATE_GENERIC_READ
		, nullptr
		, IID_PPV_ARGS(&d3d12_resource_)));

	SetName(resource_name);

	mapped_shader_records_ = MapCpuWriteOnly();
}

void ShaderTable::push_back(const ShaderRecord& shader_record)
{
	ThrowIfFalse(shader_records_.size() < shader_records_.capacity());
	shader_records_.push_back(shader_record);
	shader_record.CopyTo(mapped_shader_records_);
	mapped_shader_records_ += shader_record_size_;
	
}

uint8_t* ShaderTable::MapCpuWriteOnly()
{
	uint8_t* mapped_data;
	
	CD3DX12_RANGE read_range(0, 0);       
	ThrowIfFailed(d3d12_resource_->Map(0, &read_range, reinterpret_cast<void**>(&mapped_data)));
	return mapped_data;
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderTable::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc) const
{
	throw std::exception("ShaderTable::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderTable::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc) const
{
	throw std::exception("ShaderTable::GetUnorderedAccessView should not be called.");
}
