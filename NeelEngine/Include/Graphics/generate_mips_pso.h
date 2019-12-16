#pragma once

#include "root_signature.h"
#include "descriptor_allocation.h"

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>


struct alignas(16) GenerateMipsCB
{
	uint32_t SrcMipLevel;           // Texture level of source mip
	uint32_t NumMipLevels;          // Number of OutMips to write: [1-4]
	uint32_t SrcDimension;          // Width and height of the source texture are even or odd.
	uint32_t IsSRGB;                // Must apply gamma correction to sRGB textures.
	DirectX::XMFLOAT2 TexelSize;    // 1.0 / OutMip1.Dimensions
};

// I don't use scoped enums (C++11) to avoid the explicit cast that is required to 
// treat these as root indices.
namespace generatemips
{
	enum
	{
		kGenerateMipsCb,
		kSrcMip,
		kOutMip,
		kNumRootParameters
	};
}

class GenerateMipsPSO
{
public:
	GenerateMipsPSO();

	const RootSignature& GetRootSignature() const
	{
		return root_signature_;
	}

	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState() const
	{
		return pipeline_state_;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetDefaultUAV() const
	{
		return default_uav_.GetDescriptorHandle();
	}

private:
	RootSignature root_signature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;
	// Default (no resource) UAV's to pad the unused UAV descriptors.
	// If generating less than 4 mip map levels, the unused mip maps
	// need to be padded with default UAVs (to keep the DX12 runtime happy).
	DescriptorAllocation default_uav_;
};