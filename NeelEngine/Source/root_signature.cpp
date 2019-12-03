#include "neel_engine_pch.h"
#include "root_signature.h"

#include "neel_engine.h"

RootSignature::RootSignature()
	: root_signature_desc_{}
	  , num_descriptors_per_table_{0}
	  , sampler_table_bit_mask_(0)
	  , descriptor_table_bit_mask_(0)
{
}

RootSignature::RootSignature(
	const D3D12_ROOT_SIGNATURE_DESC1& root_signature_desc, D3D_ROOT_SIGNATURE_VERSION root_signature_version)
	: root_signature_desc_{}
	  , num_descriptors_per_table_{0}
	  , sampler_table_bit_mask_(0)
	  , descriptor_table_bit_mask_(0)
{
	SetRootSignatureDesc(root_signature_desc, root_signature_version);
}

RootSignature::~RootSignature()
{
	Destroy();
}

void RootSignature::Destroy()
{
	for (UINT i = 0; i < root_signature_desc_.NumParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER1& root_parameter = root_signature_desc_.pParameters[i];
		if (root_parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			delete[] root_parameter.DescriptorTable.pDescriptorRanges;
		}
	}

	delete[] root_signature_desc_.pParameters;
	root_signature_desc_.pParameters = nullptr;
	root_signature_desc_.NumParameters = 0;

	delete[] root_signature_desc_.pStaticSamplers;
	root_signature_desc_.pStaticSamplers = nullptr;
	root_signature_desc_.NumStaticSamplers = 0;

	descriptor_table_bit_mask_ = 0;
	sampler_table_bit_mask_ = 0;

	memset(num_descriptors_per_table_, 0, sizeof(num_descriptors_per_table_));
}

void RootSignature::SetRootSignatureDesc(
	const D3D12_ROOT_SIGNATURE_DESC1& root_signature_desc,
	D3D_ROOT_SIGNATURE_VERSION root_signature_version
)
{
	// Make sure any previously allocated root signature description is cleaned 
	// up first.
	Destroy();

	auto device = NeelEngine::Get().GetDevice();

	UINT num_parameters = root_signature_desc.NumParameters;
	D3D12_ROOT_PARAMETER1* p_parameters = num_parameters > 0 ? new D3D12_ROOT_PARAMETER1[num_parameters] : nullptr;

	for (UINT i = 0; i < num_parameters; ++i)
	{
		const D3D12_ROOT_PARAMETER1& root_parameter = root_signature_desc.pParameters[i];
		p_parameters[i] = root_parameter;

		if (root_parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			UINT num_descriptor_ranges = root_parameter.DescriptorTable.NumDescriptorRanges;
			D3D12_DESCRIPTOR_RANGE1* p_descriptor_ranges = num_descriptor_ranges > 0
				                                             ? new D3D12_DESCRIPTOR_RANGE1[num_descriptor_ranges]
				                                             : nullptr;

			memcpy(p_descriptor_ranges, root_parameter.DescriptorTable.pDescriptorRanges,
			       sizeof(D3D12_DESCRIPTOR_RANGE1) * num_descriptor_ranges);

			p_parameters[i].DescriptorTable.NumDescriptorRanges = num_descriptor_ranges;
			p_parameters[i].DescriptorTable.pDescriptorRanges = p_descriptor_ranges;

			// Set the bit mask depending on the type of descriptor table.
			if (num_descriptor_ranges > 0)
			{
				switch (p_descriptor_ranges[0].RangeType)
				{
					case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
					case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
					case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
						descriptor_table_bit_mask_ |= (1 << i);
						break;
					case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
						sampler_table_bit_mask_ |= (1 << i);
						break;
				}
			}

			// Count the number of descriptors in the descriptor table.
			for (UINT j = 0; j < num_descriptor_ranges; ++j)
			{
				num_descriptors_per_table_[i] += p_descriptor_ranges[j].NumDescriptors;
			}
		}
	}

	root_signature_desc_.NumParameters = num_parameters;
	root_signature_desc_.pParameters = p_parameters;

	UINT num_static_samplers = root_signature_desc.NumStaticSamplers;
	D3D12_STATIC_SAMPLER_DESC* p_static_samplers = num_static_samplers > 0
		                                             ? new D3D12_STATIC_SAMPLER_DESC[num_static_samplers]
		                                             : nullptr;

	if (p_static_samplers)
	{
		memcpy(p_static_samplers, root_signature_desc.pStaticSamplers,
		       sizeof(D3D12_STATIC_SAMPLER_DESC) * num_static_samplers);
	}

	root_signature_desc_.NumStaticSamplers = num_static_samplers;
	root_signature_desc_.pStaticSamplers = p_static_samplers;

	D3D12_ROOT_SIGNATURE_FLAGS flags = root_signature_desc.Flags;
	root_signature_desc_.Flags = flags;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC version_root_signature_desc;
	version_root_signature_desc.Init_1_1(num_parameters, p_parameters, num_static_samplers, p_static_samplers, flags);

	// Serialize the root signature.
	Microsoft::WRL::ComPtr<ID3DBlob> root_signature_blob;
	Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&version_root_signature_desc,
	                                                    root_signature_version, &root_signature_blob, &error_blob));

	// Create the root signature.
	ThrowIfFailed(device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(),
	                                          root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature_)));
}

uint32_t RootSignature::GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) const
{
	uint32_t descriptor_table_bit_mask = 0;
	switch (descriptor_heap_type)
	{
		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
			descriptor_table_bit_mask = descriptor_table_bit_mask_;
			break;
		case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
			descriptor_table_bit_mask = sampler_table_bit_mask_;
			break;
		default:
			break;
	}

	return descriptor_table_bit_mask;
}

uint32_t RootSignature::GetNumDescriptors(uint32_t root_index) const
{
	assert(root_index < 32);
	return num_descriptors_per_table_[root_index];
}
