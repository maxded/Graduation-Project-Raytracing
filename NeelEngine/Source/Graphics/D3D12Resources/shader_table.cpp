#include "neel_engine_pch.h"

#include "shader_table.h"
#include "neel_engine.h"
#include "helpers.h"


ShaderTable::ShaderTable()
	: Resource()
	, shader_record_size_(0)
{
}

void ShaderTable::push_back(const ShaderRecord& shader_record)
{
	shader_records_.push_back(shader_record);	
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderTable::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc) const
{
	throw std::exception("ShaderTable::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderTable::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc) const
{
	throw std::exception("ShaderTable::GetUnorderedAccessView should not be called.");
}
