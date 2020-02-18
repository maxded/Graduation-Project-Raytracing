#pragma once
#include "resource.h"

#include <vector>

class ShaderRecord
{
public:
	ShaderRecord(void* shader_identifier, UINT shader_identifier_size)
		: shader_identifier_(shader_identifier, shader_identifier_size)
	{}

	ShaderRecord(void* shader_identifier, UINT shader_identifier_size, void* local_root_arguments, UINT local_root_arguments_size)
		: shader_identifier_(shader_identifier, shader_identifier_size)
		, local_root_arguments_(local_root_arguments, local_root_arguments_size)
	{}

	void CopyTo(void* dest) const
	{
		uint8_t* byte_dest = static_cast<uint8_t*>(dest);
		memcpy(byte_dest, shader_identifier_.Ptr, shader_identifier_.Size);
		if (local_root_arguments_.Ptr)
		{
			memcpy(byte_dest + shader_identifier_.Size, local_root_arguments_.Ptr, local_root_arguments_.Size);
		}
	}

	struct PointerWithSize
	{
		void* Ptr;
		UINT Size;

		PointerWithSize() : Ptr(nullptr), Size(0) {}
		PointerWithSize(void* ptr, UINT size) : Ptr(ptr), Size(size) {};
	};

	PointerWithSize shader_identifier_;
	PointerWithSize local_root_arguments_;
};

class ShaderTable : public Resource
{
public:
	ShaderTable();

	void push_back(const ShaderRecord& shader_record);

	UINT GetShaderRecordSize() const { return shader_record_size_; }

	void SetShaderRecordSize(UINT size) { shader_record_size_ = size; }

	std::vector<ShaderRecord>& GetShaderRecords() { return shader_records_; }

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

private:	
	std::vector<ShaderRecord> shader_records_;
	UINT shader_record_size_;
};