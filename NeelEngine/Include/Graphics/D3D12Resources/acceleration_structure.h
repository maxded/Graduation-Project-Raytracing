#pragma once
#include "resource.h"
#include "descriptor_allocation.h"
#include "commandlist.h"

class AccelerationStructure : public Resource
{
public:
	AccelerationStructure(const std::string name = "AccelerationStructure");
	virtual ~AccelerationStructure();
	
	void AllocateAccelerationStructureBuffer(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs, CommandList& command_list);

	/**
	* Get the SRV for a resource.
	*/
	D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr) const override;

	/**
	* Get the UAV for a (sub)resource.
	*/
	D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr) const override;

	Microsoft::WRL::ComPtr<ID3D12Resource> GetScratchResource() { return scratch_resource_; }
protected:
	
private:
	DescriptorAllocation CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc) const;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> scratch_resource_;

	mutable std::unordered_map<size_t, DescriptorAllocation> shader_resource_views_;

	mutable std::mutex shader_resource_views_mutex_;
};




