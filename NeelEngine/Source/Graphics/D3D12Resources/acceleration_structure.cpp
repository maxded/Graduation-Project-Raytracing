#include "neel_engine_pch.h"

#include "acceleration_structure.h"
#include "neel_engine.h"
#include "commandqueue.h"
#include "commandlist.h"
#include "helpers.h"

AccelerationStructure::AccelerationStructure(const std::string name)
	: Resource(name)
{
	
}

AccelerationStructure::~AccelerationStructure()
{
	
}

void AccelerationStructure::AllocateAccelerationStructureBuffer(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs, CommandList& command_list)
{
	auto device	= NeelEngine::Get().GetDevice();
	
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO acceleration_structure_prebuild_info = {};
	device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &acceleration_structure_prebuild_info);
	ThrowIfFalse(acceleration_structure_prebuild_info.ResultDataMaxSizeInBytes > 0);

	command_list.AllocateUAVBuffer(acceleration_structure_prebuild_info.ResultDataMaxSizeInBytes, &d3d12_resource_, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
	command_list.AllocateUAVBuffer(acceleration_structure_prebuild_info.ScratchDataSizeInBytes, &scratch_resource_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	scratch_resource_->SetName(L"ScratchResource");
}

D3D12_CPU_DESCRIPTOR_HANDLE AccelerationStructure::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc) const
{
	std::size_t hash = 0;
	if (srv_desc)
	{
		hash = std::hash<D3D12_SHADER_RESOURCE_VIEW_DESC>{}(*srv_desc);
	}

	std::lock_guard<std::mutex> lock(shader_resource_views_mutex_);

	auto iter = shader_resource_views_.find(hash);
	if (iter == shader_resource_views_.end())
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.RaytracingAccelerationStructure.Location = d3d12_resource_->GetGPUVirtualAddress();
		
		auto srv = CreateShaderResourceView(&desc);
		iter = shader_resource_views_.insert({ hash, std::move(srv) }).first;
	}

	return iter->second.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE AccelerationStructure::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc) const
{
	throw std::exception("AccelerationStructure::GetShaderResourceView should not be called.");
}

DescriptorAllocation AccelerationStructure::CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc) const
{
	auto& engine	= NeelEngine::Get();
	auto device		= engine.GetDevice();
	auto srv		= engine.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	device->CreateShaderResourceView(nullptr, srv_desc, srv.GetDescriptorHandle());

	return srv;	
}

