#include "reflections_demo.h"

#include "neel_engine.h"
#include "commandqueue.h"
#include "window.h"
#include "camera.h"
#include "helpers.h"
#include "root_parameters.h"
#include <DirectXColors.h>
#include "CompiledShaders/Raytracing_Shadows.hlsl.h"
#include "shader_table.h"

using namespace Microsoft::WRL;
using namespace DirectX;

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include <d3dcompiler.h>

const wchar_t* ReflectionsDemo::c_raygen_shadow_pass_ = L"ShadowPassRaygenShader";
const wchar_t* ReflectionsDemo::c_miss_shadow_pass_   = L"ShadowPassMissShader";

enum TonemapMethod : uint32_t
{
	kTmLinear,
	kTmReinhard,
	kTmReinhardSq,
	kTmAcesFilmic,
};

struct TonemapParameters
{
	TonemapParameters()
		: TonemapMethod(kTmAcesFilmic)
		  , Exposure(0.0f)
		  , MaxLuminance(1.0f)
		  , K(1.0f)
		  , A(0.22f)
		  , B(0.3f)
		  , C(0.1f)
		  , D(0.2f)
		  , E(0.01f)
		  , F(0.3f)
		  , LinearWhite(11.2f)
		  , Gamma(2.2f)
	{
	}

	// The method to use to perform tonemapping.
	TonemapMethod TonemapMethod;
	// Exposure should be expressed as a relative exposure value (-2, -1, 0, +1, +2 )
	float Exposure;

	// The maximum luminance to use for linear tonemapping.
	float MaxLuminance;

	// Reinhard constant. Generally this is 1.0.
	float K;

	// ACES Filmic parameters
	// See: https://www.slideshare.net/ozlael/hable-john-uncharted2-hdr-lighting/142
	float A; // Shoulder strength
	float B; // Linear strength
	float C; // Linear angle
	float D; // Toe strength
	float E; // Toe Numerator
	float F; // Toe denominator
	// Note E/F = Toe angle.
	float LinearWhite;
	float Gamma;
};

TonemapParameters g_tonemap_parameters;

ReflectionsDemo::ReflectionsDemo(const std::wstring& name, int width, int height, bool v_sync)
	: Game(name, width, height, v_sync)
	, shift_(false)
	, width_(width)
	, height_(height)
	, render_scale_(1.0f)
	, scissor_rect_(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, animate_lights_(false)
	, visualise_lights_(true)
{
	// Create camera.
	Camera::Create();

	XMVECTOR camera_pos = XMVectorSet(0, 0.0f, -20.0f, 1);
	XMVECTOR camera_target = XMVectorSet(0, 0, 0, 1);
	XMVECTOR camera_up = XMVectorSet(0, 1, 0, 1);

	Camera& camera = Camera::Get();

	camera.SetLookAt(camera_pos, camera_target, camera_up);
	camera.SetProjection(45.0f, width / static_cast<float>(height), 0.01f, 125.0f);
}

ReflectionsDemo::~ReflectionsDemo()
{
	
}

bool ReflectionsDemo::LoadContent()
{
	auto device			= NeelEngine::Get().GetDevice();
	auto command_queue	= NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto command_list	= command_queue->GetCommandList();

	// Load scene from gltf file.
	scene_.LoadFromFile("Assets/Sponza/Sponza.gltf", *command_list, true);

	// Create the render targets.
	{
		DXGI_FORMAT hdr_format				= DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT depth_buffer_format		= DXGI_FORMAT_D32_FLOAT;
		DXGI_FORMAT shadow_buffer_format	= DXGI_FORMAT_R8_UINT;

		// Create an off-screen render target with a single color buffer and a depth buffer.
		int width  = NeelEngine::Get().GetWindow()->GetClientWidth();
		int height = NeelEngine::Get().GetWindow()->GetClientHeight();

		// HDR render target. R16G16B16A16_FLOAT.
		{
			auto color_desc = CD3DX12_RESOURCE_DESC::Tex2D(hdr_format, width, height);
			color_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE color_clear_value;
			color_clear_value.Format = color_desc.Format;
			color_clear_value.Color[0] = 0.4f;
			color_clear_value.Color[1] = 0.6f;
			color_clear_value.Color[2] = 0.9f;
			color_clear_value.Color[3] = 1.0f;

			// Create render target texture.
			Texture hdr_texture = Texture(color_desc, &color_clear_value, TextureUsage::RenderTarget, "HDR Texture");

			// Attach.
			hdr_render_target_.AttachTexture(AttachmentPoint::kColor0, hdr_texture);
		}

		// Depth render target. D32_FLOAT.
		{
			auto depth_desc = CD3DX12_RESOURCE_DESC::Tex2D(depth_buffer_format, width, height);
			depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_CLEAR_VALUE depth_clear_value;
			depth_clear_value.Format = depth_desc.Format;
			depth_clear_value.DepthStencil = { 1.0f, 0 };

			// Create depth buffer render target texture.
			Texture depth_texture = Texture(depth_desc, &depth_clear_value, TextureUsage::Depth, "Depth Render Target");

			// Create a view for the depth buffer (SRV).
			D3D12_SHADER_RESOURCE_VIEW_DESC view = {};
			view.Format = DXGI_FORMAT_R32_FLOAT;
			view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			view.Texture2D.MipLevels = 1;
			view.Texture2D.MostDetailedMip = 0;
			view.Texture2D.PlaneSlice = 0;

			depth_buffer_view_ = view;

			// Attach.
			hdr_render_target_.AttachTexture(AttachmentPoint::kDepthStencil, depth_texture);			
		}

		// Shadow texture. R8_UINT.
		{
			// Create a shadow buffer for the raytracing shadow pass.
			auto shadow_desc = CD3DX12_RESOURCE_DESC::Tex2D(shadow_buffer_format, width, height);
			shadow_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			// Create shadow buffer render target texture.
			shadow_texture_ = Texture(shadow_desc,nullptr, TextureUsage::ShadowMap, "ShadowMap");
		}		
	}

	// Check root signature highest version support.
	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
	feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
	{
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	}

	// Create the HDR Root Signature.
	{
		// Allow input layout and deny unnecessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 descriptor_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 3);

		CD3DX12_ROOT_PARAMETER1 root_parameters[HDRRootSignatureParams::NumRootParameters];
		root_parameters[HDRRootSignatureParams::Materials].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[HDRRootSignatureParams::MeshConstantBuffer].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
		root_parameters[HDRRootSignatureParams::LightPropertiesCb].InitAsConstants(sizeof(SceneLightProperties) / 4, 2, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[HDRRootSignatureParams::PointLights].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[HDRRootSignatureParams::SpotLights].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[HDRRootSignatureParams::DirectionalLights].InitAsShaderResourceView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[HDRRootSignatureParams::Textures].InitAsDescriptorTable(1, &descriptor_range, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC static_sampler
		(
			0, // shaderRegister
			D3D12_FILTER_ANISOTROPIC, // filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressW
			0.0f,	// mipLODBias
			8		// maxAnisotropy
		);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
		root_signature_description.Init_1_1(HDRRootSignatureParams::NumRootParameters, root_parameters, 1, &static_sampler, root_signature_flags);

		hdr_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);
	}

	// Create HDR pipeline state object with shader permutations.
	{
		// Setup the HDR pipeline State.
		struct HDRPipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE PRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		} hdr_pipeline_state_stream;

		std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout =
		{
			{	"POSITION",		0, DXGI_FORMAT_R32G32B32_FLOAT,		Mesh::vertex_slot_,		0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{	"NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,		Mesh::normal_slot_,		0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{	"TANGENT",		0, DXGI_FORMAT_R32G32B32A32_FLOAT,	Mesh::tangent_slot_,	0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{	"TEXCOORD",		0, DXGI_FORMAT_R32G32_FLOAT,		Mesh::texcoord0_slot_,	0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		};

		// Load the HDR vertex shader.
		Microsoft::WRL::ComPtr<ID3DBlob> vs;
		ThrowIfFailed(D3DReadFileToBlob(L"HDR_VS.cso", &vs));

		hdr_pipeline_state_stream.PRootSignature = hdr_root_signature_.GetRootSignature().Get();
		hdr_pipeline_state_stream.InputLayout = { &input_layout[0], static_cast<UINT>(input_layout.size()) };
		hdr_pipeline_state_stream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);;
		hdr_pipeline_state_stream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		hdr_pipeline_state_stream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		hdr_pipeline_state_stream.DSVFormat = hdr_render_target_.GetDepthStencilFormat();
		hdr_pipeline_state_stream.RTVFormats = hdr_render_target_.GetRenderTargetFormats();

		Microsoft::WRL::ComPtr<ID3DBlob> pixel_blob = CompileShaderPerumutation("main", ShaderOptions::USE_AUTO_COLOR);
		hdr_pipeline_state_stream.PS = CD3DX12_SHADER_BYTECODE(pixel_blob.Get());

		D3D12_PIPELINE_STATE_STREAM_DESC hdr_pipeline_state_stream_desc = { sizeof(HDRPipelineStateStream), &hdr_pipeline_state_stream };
		ThrowIfFailed(device->CreatePipelineState(&hdr_pipeline_state_stream_desc, IID_PPV_ARGS(&hdr_pipeline_state_map_[ShaderOptions::USE_AUTO_COLOR])));

		// Compile shader permutations for all required options for meshes.
		for (const auto& mesh : scene_.GetMeshes())
		{
			std::vector<ShaderOptions> required_options = mesh.RequiredShaderOptions();

			for (const ShaderOptions options : required_options)
			{
				if (hdr_pipeline_state_map_[options] == nullptr)
				{
					Microsoft::WRL::ComPtr<ID3DBlob> ps_blob = CompileShaderPerumutation("main", options);
					hdr_pipeline_state_stream.PS = CD3DX12_SHADER_BYTECODE(ps_blob.Get());

					D3D12_PIPELINE_STATE_STREAM_DESC state_stream_desc = { sizeof(HDRPipelineStateStream), &hdr_pipeline_state_stream };
					ThrowIfFailed(device->CreatePipelineState(&state_stream_desc, IID_PPV_ARGS(&hdr_pipeline_state_map_[options])));
				}
			}
		}
	}

	// Create raytracing Root Signatures.
	{
		CD3DX12_DESCRIPTOR_RANGE1 uav_descriptor;
		uav_descriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE1 srv_descriptor;
		srv_descriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

		CD3DX12_ROOT_PARAMETER1 root_parameters[GlobalRootSignatureParams::NumRootParameters];
		root_parameters[GlobalRootSignatureParams::RenderTarget].InitAsDescriptorTable(1, &uav_descriptor);
		root_parameters[GlobalRootSignatureParams::SceneConstantBuffer].InitAsConstantBufferView(0);
		root_parameters[GlobalRootSignatureParams::DepthMap].InitAsDescriptorTable(1, &srv_descriptor);
		root_parameters[GlobalRootSignatureParams::AccelerationStructure].InitAsShaderResourceView(0);

		CD3DX12_STATIC_SAMPLER_DESC linear_clamp_sampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
		root_signature_description.Init_1_1(GlobalRootSignatureParams::NumRootParameters, root_parameters, 1, &linear_clamp_sampler);

		raytracing_global_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);
	}

	// Create raytracing pipeline state object.
	{
		CD3DX12_STATE_OBJECT_DESC raytracing_pipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

		// DXIL library
		// This contains the shaders and their entrypoints for the state object.
		auto lib = raytracing_pipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
		D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)g_pRaytracing_Shadows, ARRAYSIZE((g_pRaytracing_Shadows)));
		lib->SetDXILLibrary(&libdxil);

		lib->DefineExport(c_raygen_shadow_pass_);
		lib->DefineExport(c_miss_shadow_pass_);

		// Shader config
		// Defines the maximum sizes in bytes for the ray payload and attribute structure.
		auto shader_config = raytracing_pipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
		UINT payload_size = 4 * sizeof(float);   // float4 color
		UINT attribute_size = 2 * sizeof(float); // float2 barycentrics
		shader_config->Config(payload_size, attribute_size);

		// Local Root Signature

		// GLobal root signature
		auto global_root_signature = raytracing_pipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
		global_root_signature->SetRootSignature(raytracing_global_root_signature_.GetRootSignature().Get());

		// Pipeline config
		auto pipeline_config = raytracing_pipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
		pipeline_config->Config(1); // Max recusion of rays.

#if _DEBUG
		PrintStateObjectDesc(raytracing_pipeline);
#endif

		// Create state object
		ThrowIfFailed(device->CreateStateObject(raytracing_pipeline, IID_PPV_ARGS(&shadow_pass_state_object_)));
	}

	// Create Acceleration Structures.
	{
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs(scene_.GetTotalMeshes());

		int index = 0;
		for (auto& geometry : scene_.GetMeshes())
		{
			// Upload geometry base transform.
			XMFLOAT3X4 transform = {};
			XMStoreFloat3x4(&transform, geometry.GetBaseTransform());
			auto gpu_address = command_list->AllocateUploadBuffer(transform);
			
			for (auto& submesh : geometry.GetSubMeshes())
			{
				// Transition buffers to D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE for building acceleration structures.
				command_list->TransitionBarrier(submesh.IBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				command_list->TransitionBarrier(submesh.VBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

				geometry_descs[index].Type									= D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
				geometry_descs[index].Triangles.IndexBuffer					= submesh.IBuffer.GetIndexBufferView().BufferLocation;
				geometry_descs[index].Triangles.IndexCount					= submesh.IBuffer.GetNumIndicies();
				geometry_descs[index].Triangles.IndexFormat					= submesh.IBuffer.GetIndexBufferView().Format;
				geometry_descs[index].Triangles.Transform3x4				= gpu_address;
				geometry_descs[index].Triangles.VertexCount					= submesh.VBuffer.GetNumVertices();
				geometry_descs[index].Triangles.VertexFormat				= DXGI_FORMAT_R32G32B32_FLOAT;
				geometry_descs[index].Triangles.VertexBuffer.StartAddress	= submesh.VBuffer.GetVertexBufferViews()[0].BufferLocation;
				geometry_descs[index].Triangles.VertexBuffer.StrideInBytes	= submesh.VBuffer.GetVertexBufferViews()[0].StrideInBytes;
				geometry_descs[index].Flags									= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

				index++;
			}
		}

		command_list->FlushResourceBarriers();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

		// Top level 
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC top_level_build_desc = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& top_level_inputs = top_level_build_desc.Inputs;
		top_level_inputs.DescsLayout		= D3D12_ELEMENTS_LAYOUT_ARRAY;
		top_level_inputs.Flags				= build_flags;
		top_level_inputs.NumDescs			= 1;
		top_level_inputs.pGeometryDescs		= nullptr;
		top_level_inputs.Type				= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		// Bottom level
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottom_level_build_desc = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottom_level_inputs = bottom_level_build_desc.Inputs;
		bottom_level_inputs.DescsLayout		= D3D12_ELEMENTS_LAYOUT_ARRAY;
		bottom_level_inputs.Flags			= build_flags;
		bottom_level_inputs.NumDescs		= scene_.GetTotalMeshes();
		bottom_level_inputs.pGeometryDescs	= geometry_descs.data();
		bottom_level_inputs.Type			= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

		// Allocate space on GPU to build acceleration structures
		bottom_level_acceleration_structure_.AllocateAccelerationStructureBuffer(bottom_level_inputs, *command_list);
		top_level_acceleration_structure_.AllocateAccelerationStructureBuffer(top_level_inputs, *command_list);

		// Create an instance desc for the bottom-level acceleration structure.
		D3D12_RAYTRACING_INSTANCE_DESC instance_desc = {};
		instance_desc.Transform[0][0] = instance_desc.Transform[1][1] = instance_desc.Transform[2][2] = 1;
		instance_desc.InstanceMask = 1;
		instance_desc.AccelerationStructure = bottom_level_acceleration_structure_.GetD3D12Resource()->GetGPUVirtualAddress();

		auto gpu_adress = command_list->AllocateUploadBuffer(instance_desc);

		{
			bottom_level_build_desc.ScratchAccelerationStructureData = bottom_level_acceleration_structure_.GetScratchResource()->GetGPUVirtualAddress();
			bottom_level_build_desc.DestAccelerationStructureData = bottom_level_acceleration_structure_.GetD3D12Resource()->GetGPUVirtualAddress();
		}

		{
			top_level_build_desc.ScratchAccelerationStructureData = top_level_acceleration_structure_.GetScratchResource()->GetGPUVirtualAddress();
			top_level_build_desc.DestAccelerationStructureData = top_level_acceleration_structure_.GetD3D12Resource()->GetGPUVirtualAddress();
			top_level_build_desc.Inputs.InstanceDescs = gpu_adress;
		}

		// Build ray tracing acceleration structures.
		command_list->GetGraphicsCommandList()->BuildRaytracingAccelerationStructure(&bottom_level_build_desc, 0, nullptr);
		command_list->UAVBarrier(bottom_level_acceleration_structure_, true);
		command_list->GetGraphicsCommandList()->BuildRaytracingAccelerationStructure(&top_level_build_desc, 0, nullptr);
	}

	// Create raytracing Shader Tables.
	{
		void* raygen_shader_id;
		void* miss_shader_id;

		auto GetShaderidentifiers = [&](auto* state_object_properties)
		{
			raygen_shader_id	= state_object_properties->GetShaderIdentifier(c_raygen_shadow_pass_);
			miss_shader_id		= state_object_properties->GetShaderIdentifier(c_miss_shadow_pass_);
		};

		UINT shader_id_size;
		{
			Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> state_object_properties;
			ThrowIfFailed(shadow_pass_state_object_.As(&state_object_properties));
			GetShaderidentifiers(state_object_properties.Get());
			shader_id_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		}

		// RayGen Shader Table
		{
			UINT num_shader_records = 1;
			UINT shader_record_size = shader_id_size;
			ShaderTable raygen_shader_table(num_shader_records, shader_record_size, "RayGenShaderTable");
			raygen_shader_table.push_back(ShaderRecord(raygen_shader_id, shader_id_size));

			raygen_shader_table_ = raygen_shader_table.GetD3D12Resource();
		}

		// Miss Shader Table
		{
			UINT num_shader_records = 1;
			UINT shader_record_size = shader_id_size;
			ShaderTable miss_shader_table(num_shader_records, shader_record_size, "MissShaderTable");
			miss_shader_table.push_back(ShaderRecord(miss_shader_id, shader_id_size));

			miss_shader_table_ = miss_shader_table.GetD3D12Resource();
		}
	}
	
	// Create the SDR Root Signature.
	{
		CD3DX12_DESCRIPTOR_RANGE1 descriptor_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 root_parameters[SDRRootSignatureParams::NumRootParameters];
		root_parameters[SDRRootSignatureParams::TonemapProperties].InitAsConstants(sizeof(TonemapParameters) / 4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[SDRRootSignatureParams::HDRTexture].InitAsDescriptorTable(1, &descriptor_range, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC linear_clamps_sampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
		root_signature_description.Init_1_1(2, root_parameters, 1, &linear_clamps_sampler);

		sdr_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);

	}

	// Create the SDR pipeline state object.
	{
		// Create the SDR PSO
		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> ps;
		ThrowIfFailed(D3DReadFileToBlob(L"HDRtoSDR_VS.cso", &vs));
		ThrowIfFailed(D3DReadFileToBlob(L"HDRtoSDR_PS.cso", &ps));

		CD3DX12_RASTERIZER_DESC rasterizer_desc(D3D12_DEFAULT);
		rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE;

		struct SDRPipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE PRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		} sdr_pipeline_state_stream;

		sdr_pipeline_state_stream.PRootSignature = sdr_root_signature_.GetRootSignature().Get();
		sdr_pipeline_state_stream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		sdr_pipeline_state_stream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		sdr_pipeline_state_stream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		sdr_pipeline_state_stream.Rasterizer = rasterizer_desc;
		sdr_pipeline_state_stream.RTVFormats = p_window_->GetRenderTarget().GetRenderTargetFormats();

		D3D12_PIPELINE_STATE_STREAM_DESC sdr_pipeline_state_stream_desc = {
			sizeof(SDRPipelineStateStream), &sdr_pipeline_state_stream
		};
		ThrowIfFailed(device->CreatePipelineState(&sdr_pipeline_state_stream_desc, IID_PPV_ARGS(&sdr_pipeline_state_)));
	}

	auto fence_value = command_queue->ExecuteCommandList(command_list);
	command_queue->WaitForFenceValue(fence_value);

	return true;
}

void ReflectionsDemo::UnloadContent()
{
	
}

static double g_fps = 0.0;

void ReflectionsDemo::OnUpdate(UpdateEventArgs& e)
{
	static uint64_t frame_count = 0;
	static double total_time = 0.0;

	total_time += e.ElapsedTime;
	frame_count++;

	Game::OnUpdate(e);

	if (total_time > 1.0)
	{
		g_fps = frame_count / total_time;

		//char buffer[512];
		//sprintf_s(buffer, "FPS: %f\n", g_fps);
		//OutputDebugStringA(buffer);

		frame_count = 0;
		total_time = 0.0;
	}

	Camera& camera = Camera::Get();

	XMMATRIX view_matrix = camera.GetViewMatrix();

	// Update lights.
	{
		const int num_point_lights = 12;
		const int num_spot_lights = 4;
		const int num_directional_lights = 1;

		static const XMVECTORF32 light_colors[] =
		{
			Colors::Red, Colors::Green, Colors::Blue, Colors::Orange, Colors::DarkTurquoise, Colors::Indigo, Colors::Violet,
			Colors::Aqua, Colors::CadetBlue, Colors::GreenYellow, Colors::Lime, Colors::Azure
		};

		static float light_anim_time = 0.0f;
		if (animate_lights_)
		{
			light_anim_time += static_cast<float>(e.ElapsedTime) * 0.1f * XM_PI;
		}

		const float radius = 3.5f;
		const float offset = 2.0f * XM_PI / num_point_lights;
		const float offset2 = offset + (offset / 2.0f);

		// Setup the light buffers.
		point_lights_.resize(num_point_lights);
		for (int i = 0; i < num_point_lights; ++i)
		{
			PointLight& l = point_lights_[i];

			l.PositionWS = {
				static_cast<float>(std::sin(light_anim_time + offset * i)) * radius,
				2.0f,
				static_cast<float>(std::cos(light_anim_time + offset * i)) * radius,
				1.0f
			};
			XMVECTOR position_ws = XMLoadFloat4(&l.PositionWS);
			XMVECTOR position_vs = XMVector3TransformCoord(position_ws, view_matrix);
			XMStoreFloat4(&l.PositionVS, position_vs);

			l.Color = XMFLOAT4(light_colors[i]);
			l.Intensity = 1.0f;
			l.Attenuation = 0.0f;
		}

		spot_lights_.resize(num_spot_lights);
		for (int i = 0; i < num_spot_lights; ++i)
		{
			SpotLight& l = spot_lights_[i];

			l.PositionWS = {
				static_cast<float>(std::sin(light_anim_time + offset * i + offset2)) * radius,
				9.0f,
				static_cast<float>(std::cos(light_anim_time + offset * i + offset2)) * radius,
				1.0f
			};
			XMVECTOR position_ws = XMLoadFloat4(&l.PositionWS);
			XMVECTOR position_vs = XMVector3TransformCoord(position_ws, view_matrix);
			XMStoreFloat4(&l.PositionVS, position_vs);

			XMVECTOR direction_ws = XMVector3Normalize(XMVectorSetW(XMVectorNegate(position_ws), 0));
			XMVECTOR direction_vs = XMVector3Normalize(XMVector3TransformNormal(direction_ws, view_matrix));
			XMStoreFloat4(&l.DirectionWS, direction_ws);
			XMStoreFloat4(&l.DirectionVS, direction_vs);

			l.Color = XMFLOAT4(light_colors[num_point_lights + i]);
			l.Intensity = 1.0f;
			l.SpotAngle = XMConvertToRadians(45.0f);
			l.Attenuation = 0.0f;
		}

		directional_lights_.resize(num_directional_lights);
		for (int i = 0; i < num_directional_lights; ++i)
		{
			DirectionalLight& l = directional_lights_[i];

			XMVECTOR direction_ws = { -0.57f, 0.57f, 0.57f, 0.0 };
			XMVECTOR direction_vs = XMVector3Normalize(XMVector3TransformNormal(direction_ws, view_matrix));

			XMStoreFloat4(&l.DirectionWS, direction_ws);
			XMStoreFloat4(&l.DirectionVS, direction_vs);

			l.Color = XMFLOAT4(Colors::White);
		}
	}

	// Update scene constants.
	{
		DirectX::XMMATRIX view_proj = camera.GetViewMatrix() * camera.GetProjectionMatrix();

		scene_buffer_.InverseViewProj = DirectX::XMMatrixInverse(nullptr, view_proj);
		scene_buffer_.CamPos = camera.GetTranslation();
	}
}

void ReflectionsDemo::OnRender(RenderEventArgs& e)
{
	Game::OnRender(e);

	auto command_queue = NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto command_list = command_queue->GetCommandList();

	// Raytracing shadow pass.
	{	
		// Bind the heaps, acceleration strucutre and dispatch rays.
		D3D12_DISPATCH_RAYS_DESC dispatch_desc = {};

		dispatch_desc.MissShaderTable.StartAddress				= miss_shader_table_->GetGPUVirtualAddress();
		dispatch_desc.MissShaderTable.SizeInBytes				= miss_shader_table_->GetDesc().Width;
		dispatch_desc.MissShaderTable.StrideInBytes				= dispatch_desc.MissShaderTable.SizeInBytes;
		dispatch_desc.RayGenerationShaderRecord.StartAddress	= raygen_shader_table_->GetGPUVirtualAddress();
		dispatch_desc.RayGenerationShaderRecord.SizeInBytes		= raygen_shader_table_->GetDesc().Width;
		dispatch_desc.Width		= width_;
		dispatch_desc.Height	= height_;
		dispatch_desc.Depth		= 1;

		command_list->SetComputeRootSignature(raytracing_global_root_signature_);
		command_list->SetStateObject(shadow_pass_state_object_);

		command_list->SetUnorderedAccessView(GlobalRootSignatureParams::RenderTarget, 0, shadow_texture_);
		command_list->SetComputeDynamicConstantBuffer(GlobalRootSignatureParams::SceneConstantBuffer, scene_buffer_);
		command_list->SetShaderResourceView(
			GlobalRootSignatureParams::DepthMap, 
			0, 
			hdr_render_target_.GetTexture(AttachmentPoint::kDepthStencil),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 
			0, 
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			&depth_buffer_view_);
		command_list->SetComputeAccelerationStructure(GlobalRootSignatureParams::AccelerationStructure, top_level_acceleration_structure_.GetD3D12Resource()->GetGPUVirtualAddress());

		command_list->DispatchRays(dispatch_desc);

		// Make sure to finish writes to render target.
		command_list->UAVBarrier(hdr_render_target_.GetTexture(AttachmentPoint::kColor1), true);
	}

	// Prepare HDR render pass.
	{
		// Clear the render targets.		
		FLOAT clear_color[] = { 0.4f, 0.6f, 0.9f, 1.0f };
		command_list->ClearTexture(hdr_render_target_.GetTexture(AttachmentPoint::kColor0), clear_color);
		command_list->ClearDepthStencilTexture(hdr_render_target_.GetTexture(AttachmentPoint::kDepthStencil),
				D3D12_CLEAR_FLAG_DEPTH);
		
		command_list->SetRenderTarget(hdr_render_target_);
		command_list->SetViewport(hdr_render_target_.GetViewport());
		command_list->SetScissorRect(scissor_rect_);
	
		command_list->SetGraphicsRootSignature(hdr_root_signature_);
	}

	// HDR render pass.
	{
		RenderContext render_context
		{
			*command_list,
			ShaderOptions::USE_AUTO_COLOR,
			ShaderOptions::None,
			hdr_pipeline_state_map_
		};
	
		// Upload lights
		SceneLightProperties light_props;
		light_props.NumPointLights = static_cast<uint32_t>(point_lights_.size());
		light_props.NumSpotLights = static_cast<uint32_t>(spot_lights_.size());
		light_props.NumDirectionalLights = static_cast<uint32_t>(directional_lights_.size());

		command_list->SetGraphics32BitConstants(HDRRootSignatureParams::LightPropertiesCb, light_props);
		command_list->SetGraphicsDynamicStructuredBuffer(HDRRootSignatureParams::PointLights, point_lights_);
		command_list->SetGraphicsDynamicStructuredBuffer(HDRRootSignatureParams::SpotLights, spot_lights_);
		command_list->SetGraphicsDynamicStructuredBuffer(HDRRootSignatureParams::DirectionalLights, directional_lights_);

		// Loop over all instances of meshes in the scene and render.
		for (auto& instance : scene_.GetInstances())
		{
			Mesh& mesh = scene_.GetMeshes()[instance.MeshIndex];
			mesh.SetBaseTransform(instance.Transform);
			mesh.Render(render_context);
		}

#if defined(DEBUG_)
		// Render light sources.
		if (visualise_lights_ && scene_.BasicGeometryLoaded())
		{
			DirectX::XMMATRIX translation_matrix = DirectX::XMMatrixIdentity();
			DirectX::XMMATRIX rotation_matrix = DirectX::XMMatrixIdentity();
			DirectX::XMMATRIX scaling_matrix = DirectX::XMMatrixIdentity();

			DirectX::XMMATRIX world_matrix = DirectX::XMMatrixIdentity();

			// Render geometry for point light sources
			for (const auto& l : point_lights_)
			{
				DirectX::XMVECTOR light_position = DirectX::XMLoadFloat4(&l.PositionWS);
				DirectX::XMVECTOR light_scaling{ 0.1f, 0.1f, 0.1f };

				translation_matrix	= DirectX::XMMatrixTranslationFromVector(light_position);
				scaling_matrix		= DirectX::XMMatrixScalingFromVector(light_scaling);

				world_matrix = scaling_matrix * rotation_matrix * translation_matrix;

				scene_.SphereMesh->SetEmissive(DirectX::XMFLOAT3(l.Color.x, l.Color.y, l.Color.z));

				scene_.SphereMesh->SetWorldMatrix(world_matrix);
				scene_.SphereMesh->Render(render_context);
			}
		}
#endif
	}

	// Prepare HDR -> SDR render pass.
	{
		command_list->SetRenderTarget(p_window_->GetRenderTarget());
		command_list->SetViewport(p_window_->GetRenderTarget().GetViewport());
		command_list->SetScissorRect(scissor_rect_);
		command_list->SetPipelineState(sdr_pipeline_state_);
		command_list->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		command_list->SetGraphicsRootSignature(sdr_root_signature_);
	}

	// HDR -> SDR render pass. (tonemap directly into window's render target (backbuffer))
	{
		command_list->SetGraphics32BitConstants(0, g_tonemap_parameters);
		command_list->SetShaderResourceView(1, 0, hdr_render_target_.GetTexture(AttachmentPoint::kColor0));

		command_list->Draw(3);
	}

	// Execute.
	command_queue->ExecuteCommandList(command_list);

	// Present
	p_window_->Present();
}

void ReflectionsDemo::OnKeyPressed(KeyEventArgs& e)
{
	Game::OnKeyPressed(e);

	switch (e.Key)
	{
		case KeyCode::Escape:
			NeelEngine::Get().Quit(0);
			break;
		case KeyCode::Enter:
			if (e.Alt)
			{
				case KeyCode::F11:
			{
				p_window_->ToggleFullscreen();
			}
				break;
			}
		case KeyCode::V:
			p_window_->ToggleVSync();
			break;
		case KeyCode::Space:	
			animate_lights_ = !animate_lights_;
			break;	
		case KeyCode::ShiftKey:
			shift_ = true;
			break;
		case KeyCode::L:
			visualise_lights_ = !visualise_lights_;
			break;
		default:
			break;
	}
}

void ReflectionsDemo::OnKeyReleased(KeyEventArgs& e)
{
	Game::OnKeyReleased(e);

	switch (e.Key)
	{
		case KeyCode::ShiftKey:
			shift_ = false;
			break;
		default:
			break;
	}
}

void ReflectionsDemo::OnMouseMoved(MouseMotionEventArgs& e)
{
	Game::OnMouseMoved(e);
}

void ReflectionsDemo::OnMouseWheel(MouseWheelEventArgs& e)
{
	Game::OnMouseWheel(e);
}

void ReflectionsDemo::OnResize(ResizeEventArgs& e)
{
	Game::OnResize(e);

	Camera& camera = Camera::Get();

	if (width_ != e.Width || height_ != e.Height)
	{
		width_ = std::max(1, e.Width);
		height_ = std::max(1, e.Height);

		float fov = camera.GetFoV();
		float aspect_ratio = width_ / static_cast<float>(height_);
		camera.SetProjection(fov, aspect_ratio, 0.1f, 100.0f);

		RescaleHDRRenderTarget(render_scale_);
	}
}

void ReflectionsDemo::RescaleHDRRenderTarget(float scale)
{
	uint32_t width = static_cast<uint32_t>(width_ * scale);
	uint32_t height = static_cast<uint32_t>(height_ * scale);

	width = clamp<uint32_t>(width, 1, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);
	height = clamp<uint32_t>(height, 1, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);

	hdr_render_target_.Resize(width, height);
}
