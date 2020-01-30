#include "neel_engine_pch.h"

#include "generate_mips_pso.h"

#include "neel_engine.h"
#include "helpers.h"

#include <d3dx12.h>
#include <d3dcompiler.h>

GenerateMipsPSO::GenerateMipsPSO()
{
	auto device = NeelEngine::Get().GetDevice();

	Microsoft::WRL::ComPtr<ID3DBlob> cs;
	ThrowIfFailed(D3DReadFileToBlob(L"EngineShaders/generate_mips_cs.cso", &cs));

	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
	feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
	{
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_DESCRIPTOR_RANGE1 source_mip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	CD3DX12_DESCRIPTOR_RANGE1 out_mip(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

	CD3DX12_ROOT_PARAMETER1 root_parameters[generatemips::kNumRootParameters];
	root_parameters[generatemips::kGenerateMipsCb].InitAsConstants(sizeof(GenerateMipsCB) / 4, 0);
	root_parameters[generatemips::kSrcMip].InitAsDescriptorTable(1, &source_mip);
	root_parameters[generatemips::kOutMip].InitAsDescriptorTable(1, &out_mip);

	CD3DX12_STATIC_SAMPLER_DESC linear_clamp_sampler(
		0,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc(
		generatemips::kNumRootParameters,
		root_parameters, 1, &linear_clamp_sampler
	);

	root_signature_.SetRootSignatureDesc(
		root_signature_desc.Desc_1_1,
		feature_data.HighestVersion
	);

	// Create the PSO for GenerateMips shader.
	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_CS CS;
	} pipeline_state_stream;

	pipeline_state_stream.pRootSignature = root_signature_.GetRootSignature().Get();
	pipeline_state_stream.CS = CD3DX12_SHADER_BYTECODE(cs.Get());

	D3D12_PIPELINE_STATE_STREAM_DESC pipeline_state_stream_desc = {
		sizeof(PipelineStateStream), &pipeline_state_stream
	};

	ThrowIfFailed(device->CreatePipelineState(&pipeline_state_stream_desc, IID_PPV_ARGS(&pipeline_state_)));

	// Create some default texture UAV's to pad any unused UAV's during mip map generation.
	default_uav_ = NeelEngine::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4);

	for (UINT i = 0; i < 4; ++i)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uav_desc.Texture2D.MipSlice = i;
		uav_desc.Texture2D.PlaneSlice = 0;

		device->CreateUnorderedAccessView(
			nullptr, nullptr, &uav_desc,
			default_uav_.GetDescriptorHandle(i)
		);
	}
}
