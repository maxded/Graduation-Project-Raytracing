#pragma once

#include "d3dx12.h"

#include <wrl.h>

class RootSignature
{
public:
	// TODO: Add (deep) copy/move constructors and assignment operators!
	RootSignature();
	RootSignature(
		const D3D12_ROOT_SIGNATURE_DESC1& root_signature_desc,
		D3D_ROOT_SIGNATURE_VERSION root_signature_version
	);

	virtual ~RootSignature();

	void Destroy();

	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const
	{
		return root_signature_;
	}

	void SetRootSignatureDesc(
		const D3D12_ROOT_SIGNATURE_DESC1& root_signature_desc,
		D3D_ROOT_SIGNATURE_VERSION root_signature_version
	);

	const D3D12_ROOT_SIGNATURE_DESC1& GetRootSignatureDesc() const
	{
		return root_signature_desc_;
	}

	uint32_t GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) const;
	uint32_t GetNumDescriptors(uint32_t root_index) const;

protected:

private:
	D3D12_ROOT_SIGNATURE_DESC1 root_signature_desc_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;

	// Need to know the number of descriptors per descriptor table.
	// A maximum of 32 descriptor tables are supported (since a 32-bit
	// mask is used to represent the descriptor tables in the root signature.
	uint32_t num_descriptors_per_table_[32];

	// A bit mask that represents the root parameter indices that are 
	// descriptor tables for Samplers.
	uint32_t sampler_table_bit_mask_;
	// A bit mask that represents the root parameter indices that are 
	// CBV, UAV, and SRV descriptor tables.
	uint32_t descriptor_table_bit_mask_;
};
