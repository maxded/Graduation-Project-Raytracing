#include "reflections_demo.h"

#include "neel_engine.h"
#include "commandqueue.h"
#include "window.h"
#include "camera.h"
#include "helpers.h"
#include "root_parameters.h"

using namespace Microsoft::WRL;
using namespace DirectX;

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include <d3dcompiler.h>
#include <DirectXColors.h>
#include "CompiledShaders/Raytracing.hlsl.h"

const wchar_t* ReflectionsDemo::c_raygen_shader_		= L"RaygenShader";
const wchar_t* ReflectionsDemo::c_closesthit_shader_	= L"ClosesthitShader";
const wchar_t* ReflectionsDemo::c_miss_shader_			= L"MissShader";
const wchar_t* ReflectionsDemo::c_hitgroup_				= L"Hitgroup";

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
		: TonemapMethod(kTmReinhard)
		  , Exposure(-1.0f)
		  , MaxLuminance(255.0f)
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

enum OutputTexture
{
	HDRTexture = 0,
	AlbedoTexture,
	NormalTexture,
	MetalTexture,
	RoughTexture,
	EmissiveTexture,
	OcclusionTexture,
	DepthTexture,
	RayTracedShadowsTexture,
	RayTracedReflectionsTexture
};

struct OutputMode
{
	OutputMode()
		: Texture(HDRTexture)
		, NearPlane(0.0f)
		, FarPlane(0.0f)
	{}
	
	OutputTexture Texture;
	float NearPlane;
	float FarPlane;
};

TonemapParameters g_tonemap_parameters;
OutputMode g_output_mode;

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

	XMVECTOR camera_pos		= XMVectorSet(9.0f, 1.5f, -0.09f, 1);
	XMVECTOR camera_target	= XMVectorSet(0, 1.5f, 0, 1);
	XMVECTOR camera_up		= XMVectorSet(0, 1, 0, 1);

	Camera& camera = Camera::Get();
	
	camera.SetLookAt(camera_pos, camera_target, camera_up);
	camera.SetProjection(45.0f, width / static_cast<float>(height), 0.1f, 100.0f);
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
	scene_.LoadFromFile("Assets/Sponza/Sponza.gltf", *command_list, false);

	int width	= NeelEngine::Get().GetWindow()->GetClientWidth();
	int height	= NeelEngine::Get().GetWindow()->GetClientHeight();

	
	//=============================================================================
	// Rendertargets.
	//=============================================================================

	
	// Geometry render pass (render target).
	{
		// Formats.
		DXGI_FORMAT albedo_buffer_format				= DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT normal_buffer_format				= DXGI_FORMAT_R32G32B32A32_FLOAT;
		DXGI_FORMAT metal_rough_buffer_format			= DXGI_FORMAT_R8G8_UNORM;
		DXGI_FORMAT occlusion_emissive_buffer_format	= DXGI_FORMAT_R8G8B8A8_UNORM;	
		DXGI_FORMAT depth_stencil_buffer_format			= DXGI_FORMAT_D32_FLOAT;

		// Albedo render target. R8G8B8A8_UNORM.
		{
			auto albedo_desc = CD3DX12_RESOURCE_DESC::Tex2D(albedo_buffer_format, width, height);
			albedo_desc.MipLevels = 1;
			albedo_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE color_clear_value;
			color_clear_value.Format = albedo_desc.Format;
			color_clear_value.Color[0] = 0.4f;
			color_clear_value.Color[1] = 0.6f;
			color_clear_value.Color[2] = 0.9f;
			color_clear_value.Color[3] = 1.0f;

			// Create render target texture.
			Texture albedo_texture = Texture(albedo_desc, &color_clear_value, TextureUsage::RenderTarget, "GBuffer: Albedo Texture");

			// Attach.
			geometry_pass_render_target_.AttachTexture(AttachmentPoint::kColor0, albedo_texture);
		}

		// Normal render target. R32B32G32A32_FLOAT.
		{
			auto normal_desc = CD3DX12_RESOURCE_DESC::Tex2D(normal_buffer_format, width, height);
			normal_desc.MipLevels = 1;
			normal_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE color_clear_value;
			color_clear_value.Format = normal_desc.Format;
			color_clear_value.Color[0] = 0.4f;
			color_clear_value.Color[1] = 0.6f;
			color_clear_value.Color[2] = 0.9f;
			color_clear_value.Color[3] = 1.0f;

			// Create render target texture.
			Texture normal_texture = Texture(normal_desc, &color_clear_value, TextureUsage::RenderTarget, "GBuffer: Normal Texture");

			// Attach.
			geometry_pass_render_target_.AttachTexture(AttachmentPoint::kColor1, normal_texture);
		}

		// metal-rough (combined) render target. R8G8_UNORM.
		{
			auto metal_rough_desc = CD3DX12_RESOURCE_DESC::Tex2D(metal_rough_buffer_format, width, height);
			metal_rough_desc.MipLevels = 1;
			metal_rough_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE color_clear_value;
			color_clear_value.Format = metal_rough_desc.Format;
			color_clear_value.Color[0] = 0.4f;
			color_clear_value.Color[1] = 0.6f;
			color_clear_value.Color[2] = 0.9f;
			color_clear_value.Color[3] = 1.0f;

			// Create render target texture.
			Texture metal_rough_texture = Texture(metal_rough_desc, &color_clear_value, TextureUsage::RenderTarget, "GBuffer: Metal-Rough Texture");

			// Attach.
			geometry_pass_render_target_.AttachTexture(AttachmentPoint::kColor2, metal_rough_texture);
		}

		// Occlusion-emissive render target. R8G8B8A8_UNORM.
		{
			auto occlusion_emissive_desc = CD3DX12_RESOURCE_DESC::Tex2D(occlusion_emissive_buffer_format, width, height);
			occlusion_emissive_desc.MipLevels = 1;
			occlusion_emissive_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE color_clear_value;
			color_clear_value.Format = occlusion_emissive_desc.Format;
			color_clear_value.Color[0] = 0.4f;
			color_clear_value.Color[1] = 0.6f;
			color_clear_value.Color[2] = 0.9f;
			color_clear_value.Color[3] = 1.0f;

			// Create render target texture.
			Texture occlusion_emissive_texture = Texture(occlusion_emissive_desc, &color_clear_value, TextureUsage::RenderTarget, "GBuffer: Occlusion-Emissive Texture");

			// Attach.
			geometry_pass_render_target_.AttachTexture(AttachmentPoint::kColor3, occlusion_emissive_texture);
		}
		
		// Depth-stencil render target. D32_FLOAT_S8X24_UINT.
		{
			auto depth_desc = CD3DX12_RESOURCE_DESC::Tex2D(depth_stencil_buffer_format, width, height);
			depth_desc.MipLevels = 1;
			depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_CLEAR_VALUE depth_clear_value;
			depth_clear_value.Format = depth_desc.Format;
			depth_clear_value.DepthStencil = { 1.0f, 0 };

			// Create depth buffer render target texture.
			Texture depth_texture = Texture(depth_desc, &depth_clear_value, TextureUsage::Depth, "Depth-Stencil Render Target");

			// Create a view for the depth buffer (SRV).
			D3D12_SHADER_RESOURCE_VIEW_DESC view = {};
			view.Format						= DXGI_FORMAT_R32_FLOAT;
			view.Shader4ComponentMapping	= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			view.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE2D;
			view.Texture2D.MipLevels		= 1;
			view.Texture2D.MostDetailedMip	= 0;
			view.Texture2D.PlaneSlice		= 0;

			depth_buffer_view_ = view;

			// Attach.
			geometry_pass_render_target_.AttachTexture(AttachmentPoint::kDepthStencil, depth_texture);			
		}

	}

	// Light accumulation render pass (render target).
	{
		DXGI_FORMAT hdr_format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		// HDR render target. R16G16B16A16_FLOAT.
		{
			auto color_desc = CD3DX12_RESOURCE_DESC::Tex2D(hdr_format, width, height);
			color_desc.MipLevels = 1;
			color_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE color_clear_value{};
			color_clear_value.Format = color_desc.Format;
			color_clear_value.Color[0] = 0.4f;
			color_clear_value.Color[1] = 0.6f;
			color_clear_value.Color[2] = 0.9f;
			color_clear_value.Color[3] = 1.0f;

			// Create render target texture.
			Texture hdr_texture = Texture(color_desc, &color_clear_value, TextureUsage::RenderTarget, "Light Accumulation : HDR Texture");

			// Attach.
			light_accumulation_pass_render_target_.AttachTexture(AttachmentPoint::kColor0, hdr_texture);
		}

		// Set current display texture.
		current_display_texture_ = &light_accumulation_pass_render_target_.GetTexture(AttachmentPoint::kColor0);
	}

	//=============================================================================
	// GBuffer.
	//=============================================================================


	// UAV - raytracing output texture.
	{
		DXGI_FORMAT raytracing_buffer_format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		//  raytracing output texture. DXGI_FORMAT_R16G16B16A16_FLOAT.
		{
			// Create a raytracing buffer for the raytracing pass.
			auto raytracing_desc = CD3DX12_RESOURCE_DESC::Tex2D(raytracing_buffer_format, width, height);
			raytracing_desc.MipLevels = 1;
			raytracing_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			// Create raytracing buffer render target texture.
			raytracing_output_texture_ = Texture(raytracing_desc, nullptr, TextureUsage::UAV, "Raytracing output");
		}
	}

	//=============================================================================
	// Rootsignatures.
	//=============================================================================

	
	// Check root signature highest version support.
	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
	feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
	{
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	}

	// Create the geometry pass root signature.
	{
		// Allow input layout and deny unnecessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 descriptor_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, scene_.GetTextures().size(), 1);

		CD3DX12_ROOT_PARAMETER1 root_parameters[GeometryPassRootSignatureParams::NumRootParameters];
		root_parameters[GeometryPassRootSignatureParams::MaterialConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[GeometryPassRootSignatureParams::MeshConstantBuffer].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
		root_parameters[GeometryPassRootSignatureParams::Textures].InitAsDescriptorTable(1, &descriptor_range, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[GeometryPassRootSignatureParams::Materials].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

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
		root_signature_description.Init_1_1(GeometryPassRootSignatureParams::NumRootParameters, root_parameters, 1, &static_sampler, root_signature_flags);

		geometry_pass_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);
	}

	// Create raytracing global root signature.
	{
		CD3DX12_DESCRIPTOR_RANGE1 uav_descriptor;
		uav_descriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE1 srv_descriptor;
		srv_descriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 8);

		CD3DX12_DESCRIPTOR_RANGE1 textures_descriptor;
		textures_descriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, scene_.GetTextures().size(), 12);

		CD3DX12_ROOT_PARAMETER1 root_parameters[RtGlobalRootSignatureParams::NumRootParameters];
		root_parameters[RtGlobalRootSignatureParams::RenderTarget].InitAsDescriptorTable(1, &uav_descriptor);
		root_parameters[RtGlobalRootSignatureParams::SceneConstantData].InitAsConstantBufferView(0);
		root_parameters[RtGlobalRootSignatureParams::LightPropertiesCb].InitAsConstants(sizeof(SceneLightProperties) / 4, 1, 0);
		root_parameters[RtGlobalRootSignatureParams::PointLights].InitAsShaderResourceView(0);
		root_parameters[RtGlobalRootSignatureParams::SpotLights].InitAsShaderResourceView(1);
		root_parameters[RtGlobalRootSignatureParams::DirectionalLights].InitAsShaderResourceView(2);
		root_parameters[RtGlobalRootSignatureParams::Materials].InitAsShaderResourceView(3);
		root_parameters[RtGlobalRootSignatureParams::AccelerationStructure].InitAsShaderResourceView(4);
		root_parameters[RtGlobalRootSignatureParams::MeshInfo].InitAsShaderResourceView(5);
		root_parameters[RtGlobalRootSignatureParams::Attributes].InitAsShaderResourceView(6);
		root_parameters[RtGlobalRootSignatureParams::Indices].InitAsShaderResourceView(7);
		root_parameters[RtGlobalRootSignatureParams::GBuffer].InitAsDescriptorTable(1, &srv_descriptor);
		root_parameters[RtGlobalRootSignatureParams::Textures].InitAsDescriptorTable(1, &textures_descriptor);

		CD3DX12_STATIC_SAMPLER_DESC static_sampler
		(
			0, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP // addressW
		);

		
		D3D12_STATIC_SAMPLER_DESC default_sampler;
		default_sampler.Filter			= D3D12_FILTER_ANISOTROPIC;
		default_sampler.AddressU		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		default_sampler.AddressV		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		default_sampler.AddressW		= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		default_sampler.MipLODBias		= 0.0f;
		default_sampler.MaxAnisotropy	= 16;
		default_sampler.ComparisonFunc	= D3D12_COMPARISON_FUNC_LESS_EQUAL;
		default_sampler.MinLOD			= 0.0f;
		default_sampler.MaxLOD			= D3D12_FLOAT32_MAX;
		default_sampler.MaxAnisotropy	= 8;
		default_sampler.BorderColor		= D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		default_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		default_sampler.ShaderRegister	= 0;
		default_sampler.RegisterSpace	= 0;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
		root_signature_description.Init_1_1(RtGlobalRootSignatureParams::NumRootParameters, root_parameters, 1, &static_sampler);

		raytracing_global_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);
	}

	// Create raytracing local root signature.
	{
		CD3DX12_ROOT_PARAMETER1 root_parameters[RtLocalRootSignatureParams::NumRootParameters];
		root_parameters[RtLocalRootSignatureParams::MeshInfoIndex].InitAsConstants(sizeof(MeshInfoIndex) / 4, 2, 0);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
		root_signature_description.Init_1_1(RtLocalRootSignatureParams::NumRootParameters, root_parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

		raytracing_local_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);
	}

	// Create the light accumulation pass root signature.
	{
		CD3DX12_DESCRIPTOR_RANGE1 descriptor_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 3);

		CD3DX12_ROOT_PARAMETER1 root_parameters[LightAccumulationPassRootSignatureParams::NumRootParameters];
		root_parameters[LightAccumulationPassRootSignatureParams::SceneConstantData].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[LightAccumulationPassRootSignatureParams::LightPropertiesCb].InitAsConstants(sizeof(SceneLightProperties) / 4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[LightAccumulationPassRootSignatureParams::PointLights].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[LightAccumulationPassRootSignatureParams::SpotLights].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[LightAccumulationPassRootSignatureParams::DirectionalLights].InitAsShaderResourceView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[LightAccumulationPassRootSignatureParams::GBuffer].InitAsDescriptorTable(1, &descriptor_range, D3D12_SHADER_VISIBILITY_PIXEL);
		
		CD3DX12_STATIC_SAMPLER_DESC point_clamp_sampler
		(
			0,
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
		root_signature_description.Init_1_1(LightAccumulationPassRootSignatureParams::NumRootParameters, root_parameters, 1, &point_clamp_sampler);

		light_accumulation_pass_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);
	}
	
	// Create the composite pass root signature.
	{
		CD3DX12_DESCRIPTOR_RANGE1 descriptor_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 root_parameters[CompositePassRootSignatureParams::NumRootParameters];
		root_parameters[CompositePassRootSignatureParams::TonemapProperties].InitAsConstants(sizeof(TonemapParameters) / 4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[CompositePassRootSignatureParams::OutputMode].InitAsConstants(sizeof(OutputMode) / 4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[CompositePassRootSignatureParams::OutputTexture].InitAsDescriptorTable(1, &descriptor_range, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC point_clamp_sampler
		(
			0,
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
		root_signature_description.Init_1_1(CompositePassRootSignatureParams::NumRootParameters, root_parameters, 1, &point_clamp_sampler);

		composite_pass_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);
	}

	
	//=============================================================================
	// Pipelinestates.
	//=============================================================================

	
	// Create geometry pass pipeline state object with shader permutations.
	{
		// Setup the HDR pipeline State.
		struct GeometryPipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE PRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		} geometry_pipeline_state_stream;

		// Create input layout.
		std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout =
		{
			{	"POSITION",		0, DXGI_FORMAT_R32G32B32_FLOAT,		Mesh::vertex_slot_,		0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{	"NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,		Mesh::normal_slot_,		0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{	"TANGENT",		0, DXGI_FORMAT_R32G32B32A32_FLOAT,	Mesh::tangent_slot_,	0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{	"TEXCOORD",		0, DXGI_FORMAT_R32G32_FLOAT,		Mesh::texcoord0_slot_,	0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		};

		// Load shaders.
		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> ps;
		ThrowIfFailed(D3DReadFileToBlob(L"Shaders/GeometryPass_VS.cso", &vs));
		ThrowIfFailed(D3DReadFileToBlob(L"Shaders/GeometryPass_PS.cso", &ps));

		geometry_pipeline_state_stream.PRootSignature			= geometry_pass_root_signature_.GetRootSignature().Get();
		geometry_pipeline_state_stream.InputLayout				= { &input_layout[0], static_cast<UINT>(input_layout.size()) };
		geometry_pipeline_state_stream.Rasterizer				= CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);;
		geometry_pipeline_state_stream.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		geometry_pipeline_state_stream.VS						= CD3DX12_SHADER_BYTECODE(vs.Get());
		geometry_pipeline_state_stream.PS						= CD3DX12_SHADER_BYTECODE(ps.Get());
		geometry_pipeline_state_stream.DSVFormat				= geometry_pass_render_target_.GetDepthStencilFormat();
		geometry_pipeline_state_stream.RTVFormats				= geometry_pass_render_target_.GetRenderTargetFormats();

		// Create the pipeline state.
		D3D12_PIPELINE_STATE_STREAM_DESC geometry_pipeline_stream_desc = {
			sizeof(GeometryPipelineStateStream), & geometry_pipeline_state_stream
		};
		ThrowIfFailed(device->CreatePipelineState(&geometry_pipeline_stream_desc, IID_PPV_ARGS(&geometry_pass_pipeline_state_)));
		
	}

	// Create raytracing pipeline state object.
	{
		CD3DX12_STATE_OBJECT_DESC raytracing_pipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

		// DXIL library
		// This contains the shaders and their entrypoints for the state object.
		auto lib = raytracing_pipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
		D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)g_pRaytracing, ARRAYSIZE((g_pRaytracing)));
		lib->SetDXILLibrary(&libdxil);

		lib->DefineExport(c_raygen_shader_);
		lib->DefineExport(c_closesthit_shader_);
		lib->DefineExport(c_miss_shader_);

		auto hit_group = raytracing_pipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
		hit_group->SetClosestHitShaderImport(c_closesthit_shader_);
		hit_group->SetHitGroupExport(c_hitgroup_);
		hit_group->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

		// Shader config
		// Defines the maximum sizes in bytes for the ray payload and attribute structure.
		auto shader_config = raytracing_pipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
		UINT payload_size	= 7 * sizeof(float);   // float4 color
		UINT attribute_size = 2 * sizeof(float);  // float2 barycentrics
		shader_config->Config(payload_size, attribute_size);

		// Local root signature.
		auto local_root_signature = raytracing_pipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
		local_root_signature->SetRootSignature(raytracing_local_root_signature_.GetRootSignature().Get());
		// Define explicit shader association for the local root signature. 
		{
			auto root_signature_association = raytracing_pipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
			root_signature_association->SetSubobjectToAssociate(*local_root_signature);
			root_signature_association->AddExport(c_hitgroup_);
		}
		
		// GLobal root signature
		auto global_root_signature = raytracing_pipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
		global_root_signature->SetRootSignature(raytracing_global_root_signature_.GetRootSignature().Get());

		// Pipeline config
		auto pipeline_config = raytracing_pipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
		pipeline_config->Config(3); // Max recusion of rays.

#if _DEBUG
		PrintStateObjectDesc(raytracing_pipeline);
#endif

		// Create state object
		ThrowIfFailed(device->CreateStateObject(raytracing_pipeline, IID_PPV_ARGS(&raytracing_pass_state_object_)));
	}

	// Create the light accumulation pass pipeline state object.
	{
		struct LightAccumulationPipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE PRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		} light_accumulation_pipeline_state_stream;

		// Load shaders.
		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> ps;
		ThrowIfFailed(D3DReadFileToBlob(L"Shaders/LightAccumulationPass_VS.cso", &vs));
		ThrowIfFailed(D3DReadFileToBlob(L"Shaders/LightAccumulationPass_PS.cso", &ps));

		// Create rasterizer state.
		CD3DX12_RASTERIZER_DESC rasterizer_desc(D3D12_DEFAULT);
		rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE;

		light_accumulation_pipeline_state_stream.PRootSignature			= light_accumulation_pass_root_signature_.GetRootSignature().Get();
		light_accumulation_pipeline_state_stream.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		light_accumulation_pipeline_state_stream.VS						= CD3DX12_SHADER_BYTECODE(vs.Get());
		light_accumulation_pipeline_state_stream.PS						= CD3DX12_SHADER_BYTECODE(ps.Get());
		light_accumulation_pipeline_state_stream.Rasterizer				= rasterizer_desc;
		light_accumulation_pipeline_state_stream.RTVFormats				= light_accumulation_pass_render_target_.GetRenderTargetFormats();

		// Create the pipeline state.
		D3D12_PIPELINE_STATE_STREAM_DESC light_accumulation_pipeline_state_stream_desc = {
			sizeof(LightAccumulationPipelineStateStream), & light_accumulation_pipeline_state_stream
		};
		ThrowIfFailed(device->CreatePipelineState(&light_accumulation_pipeline_state_stream_desc, IID_PPV_ARGS(&light_accumulation_pass_pipeline_state_)));
	}
	
	// Create the composite pass pipeline state object.
	{
		struct CompositePipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE PRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		} composite_pipeline_state_stream;

		// Load shaders.
		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> ps;
		ThrowIfFailed(D3DReadFileToBlob(L"Shaders/CompositePass_VS.cso", &vs));
		ThrowIfFailed(D3DReadFileToBlob(L"Shaders/CompositePass_PS.cso", &ps));

		// Create rasterizer state.
		CD3DX12_RASTERIZER_DESC rasterizer_desc(D3D12_DEFAULT);
		rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE;

		composite_pipeline_state_stream.PRootSignature			= composite_pass_root_signature_.GetRootSignature().Get();
		composite_pipeline_state_stream.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		composite_pipeline_state_stream.VS						= CD3DX12_SHADER_BYTECODE(vs.Get());
		composite_pipeline_state_stream.PS						= CD3DX12_SHADER_BYTECODE(ps.Get());
		composite_pipeline_state_stream.Rasterizer				= rasterizer_desc;
		composite_pipeline_state_stream.RTVFormats				= p_window_->GetRenderTarget().GetRenderTargetFormats();

		// Create the pipeline state.
		D3D12_PIPELINE_STATE_STREAM_DESC composite_pipeline_state_stream_desc = {
			sizeof(CompositePipelineStateStream), &composite_pipeline_state_stream
		};
		ThrowIfFailed(device->CreatePipelineState(&composite_pipeline_state_stream_desc, IID_PPV_ARGS(&composite_pass_pipeline_state_)));
	}

	
	//=============================================================================
	// Raytracing.
	//=============================================================================

	// Create Acceleration Structures.
	{
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs(scene_.GetTotalMeshes());

		// Variables to be used later.
		size_t total_geometry_vertex_buffer_size = 0;
		size_t total_geometry_index_buffer_size = 0;
		
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

				geometry_descs[index].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
				geometry_descs[index].Triangles.IndexBuffer = submesh.IBuffer.GetIndexBufferView().BufferLocation;
				geometry_descs[index].Triangles.IndexCount = submesh.IBuffer.GetNumIndicies();
				geometry_descs[index].Triangles.IndexFormat = submesh.IBuffer.GetIndexBufferView().Format;
				geometry_descs[index].Triangles.Transform3x4 = gpu_address;
				geometry_descs[index].Triangles.VertexCount = submesh.VBuffer.GetNumVertices();
				geometry_descs[index].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				geometry_descs[index].Triangles.VertexBuffer.StartAddress = submesh.VBuffer.GetVertexBufferViews()[0].BufferLocation;
				geometry_descs[index].Triangles.VertexBuffer.StrideInBytes = submesh.VBuffer.GetVertexBufferViews()[0].StrideInBytes;
				geometry_descs[index].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

				total_geometry_vertex_buffer_size += submesh.VBuffer.GetD3D12Resource()->GetDesc().Width;
				total_geometry_index_buffer_size += submesh.IBuffer.GetD3D12Resource()->GetDesc().Width;
				
				index++;
			}
		}

		command_list->FlushResourceBarriers();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

		// Top level 
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC top_level_build_desc = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& top_level_inputs = top_level_build_desc.Inputs;
		top_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		top_level_inputs.Flags = build_flags;
		top_level_inputs.NumDescs = 1;
		top_level_inputs.pGeometryDescs = nullptr;
		top_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		// Bottom level
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottom_level_build_desc = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottom_level_inputs = bottom_level_build_desc.Inputs;
		bottom_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bottom_level_inputs.Flags = build_flags;
		bottom_level_inputs.NumDescs = scene_.GetTotalMeshes();
		bottom_level_inputs.pGeometryDescs = geometry_descs.data();
		bottom_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

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


		// Generate mesh shader data for raytracing.
		{
			auto vertex_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(total_geometry_vertex_buffer_size);
			auto index_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(total_geometry_index_buffer_size);
			
			global_vertices_ = ByteAddressBuffer(vertex_buffer_desc, total_geometry_vertex_buffer_size, "Global Vertex Buffer Byte Address Buffer");
			global_indices_ = ByteAddressBuffer(index_buffer_desc, total_geometry_index_buffer_size, "Global Index Buffer Byte Address Buffer");

			UINT64 vertex_offset = 0;
			UINT64 index_offset = 0;
			
			UINT attribute_offset = 0;
			
			for (auto& geometry : scene_.GetMeshes())
			{
				for (auto& submesh : geometry.GetSubMeshes())
				{
					MeshInfo info;
					info.IndicesOffset = index_offset;	// Start address of current mesh's index buffer in contiguous index buffer.
					
					info.PositionAttributeOffset = attribute_offset;
					info.PositionStride = submesh.VBuffer.GetVertexBufferViews()[0].StrideInBytes;
					attribute_offset += submesh.VBuffer.GetVertexBufferViews()[0].SizeInBytes; // Position attribute offset to normal.
					
					info.NormalAttributeOffset = attribute_offset;
					info.NormalStride = submesh.VBuffer.GetVertexBufferViews()[1].StrideInBytes;
					attribute_offset += submesh.VBuffer.GetVertexBufferViews()[1].SizeInBytes; // Normal attribute offset to tangent or uv.
					
					if(submesh.HasTangents)
					{
						info.HasTangents = true;
						info.TangentAttributeOffset = attribute_offset;
						info.TangentStride = submesh.VBuffer.GetVertexBufferViews()[2].StrideInBytes;
						attribute_offset += submesh.VBuffer.GetVertexBufferViews()[2].SizeInBytes; // Tangent attribute offset to uv.
					}
					else
					{
						info.HasTangents = false;
						info.TangentAttributeOffset = attribute_offset;
					}

					info.UvAttributeOffset = attribute_offset;
					info.UvStride = submesh.VBuffer.GetVertexBufferViews()[3].StrideInBytes;
					attribute_offset += submesh.VBuffer.GetVertexBufferViews()[3].SizeInBytes; // UV attribute offset to end of buffer / begin next buffer.

					info.MaterialId = submesh.MaterialCB.MaterialIndex;
									
					mesh_infos_.emplace_back(info);

					// Copy sumesh's buffers to the global contiguous buffers used for raytracing.
					command_list->CopyBufferRegion(global_vertices_, vertex_offset, submesh.VBuffer, 0, submesh.VBuffer.GetD3D12Resource()->GetDesc().Width);
					command_list->CopyBufferRegion(global_indices_, index_offset, submesh.IBuffer, 0, submesh.IBuffer.GetD3D12Resource()->GetDesc().Width);
					
					vertex_offset += submesh.VBuffer.GetD3D12Resource()->GetDesc().Width;
					index_offset += submesh.IBuffer.GetD3D12Resource()->GetDesc().Width;
				}
			}
		}	
	}
	
	// Create raytracing Shader Tables.
	{
		void* raygen_shader_id;
		void* miss_shader_id;
		void* hitgroup_shader_id;

		auto  GetShaderIdentifiersShadow = [&](auto* state_object_properties)
		{
			raygen_shader_id		= state_object_properties->GetShaderIdentifier(c_raygen_shader_);
			hitgroup_shader_id		= state_object_properties->GetShaderIdentifier(c_hitgroup_);
			miss_shader_id			= state_object_properties->GetShaderIdentifier(c_miss_shader_);
		};
		
		Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> state_object_properties;
		ThrowIfFailed(raytracing_pass_state_object_.As(&state_object_properties));
		GetShaderIdentifiersShadow(state_object_properties.Get());

		UINT shader_id_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		// RayGen Shader Table
		{
			UINT shader_record_size = shader_id_size;		
			raygen_shader_table_.push_back(ShaderRecord(raygen_shader_id, shader_id_size));

			// Copy shader table to GPU memory.
			command_list->CopyShaderTable(raygen_shader_table_, shader_record_size, "Raygen ShaderTable");
		}

		// Hit Group Shader Table
		{
			struct RootArguments
			{
				MeshInfoIndex MeshInfo;
			};
			
			UINT shader_record_size = shader_id_size + sizeof(RootArguments);
			std::vector<RootArguments> arguments(scene_.GetTotalMeshes());
	
			int index = 0;
			for (auto& geometry : scene_.GetMeshes())
			{
				for (auto& submesh : geometry.GetSubMeshes())
				{	
					arguments[index].MeshInfo.MeshId = index;
					
					hitgroup_shader_table_.push_back(ShaderRecord(hitgroup_shader_id, shader_id_size, &arguments[index], sizeof(RootArguments)));

					index++;
				}
			}
			
			// Copy shader table to GPU memory.
			command_list->CopyShaderTable(hitgroup_shader_table_, shader_record_size, "Hitgroup ShaderTable");
		}

		// Miss Shader Table
		{
			UINT shader_record_size = shader_id_size;
			miss_shader_table_.push_back(ShaderRecord(miss_shader_id, shader_id_size));

			// Copy shader table to GPU memory.
			command_list->CopyShaderTable(miss_shader_table_, shader_record_size, "Miss ShaderTable");
		}
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
		/*for (int i = 0; i < num_point_lights; ++i)
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
			l.Range = 3.0f;
		}*/

		spot_lights_.resize(num_spot_lights);
		/*for (int i = 0; i < num_spot_lights; ++i)
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
		}*/

		directional_lights_.resize(num_directional_lights);
		for (int i = 0; i < num_directional_lights; ++i)
		{
			DirectionalLight& l = directional_lights_[i];

			XMVECTOR direction_ws = { -0.16f, 0.86f, 0.12f, 0.0 };
			XMVECTOR direction_vs = XMVector3Normalize(XMVector3TransformNormal(direction_ws, view_matrix));

			XMStoreFloat4(&l.DirectionWS, direction_ws);
			XMStoreFloat4(&l.DirectionVS, direction_vs);

			l.Color = XMFLOAT4(15.0, 15.0, 15.0, 1.0);
		}
	}

	// Update scene constants.
	{
		XMMATRIX view_proj = camera.GetViewMatrix() * camera.GetProjectionMatrix();

		scene_buffer_.InverseViewProj	= XMMatrixInverse(nullptr, view_proj);
		scene_buffer_.CamPos			= camera.GetTranslation();
		scene_buffer_.VFOV				= camera.GetFoV();
		scene_buffer_.PixelHeight		= height_;
	}

	// Update viewport constants.
	g_output_mode.NearPlane = camera.GetNearClip();
	g_output_mode.FarPlane  = camera.GetFarClip();	

}

void ReflectionsDemo::OnRender(RenderEventArgs& e)
{
	Game::OnRender(e);

	auto command_queue = NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto command_list = command_queue->GetCommandList();

	// Geometry render pass.
	{	
		command_list->BeginRenderPass(geometry_pass_render_target_);

		command_list->SetViewport(geometry_pass_render_target_.GetViewport());
		command_list->SetScissorRect(scissor_rect_);
		command_list->SetGraphicsRootSignature(geometry_pass_root_signature_);
		command_list->SetPipelineState(geometry_pass_pipeline_state_);

		command_list->SetGraphicsDynamicStructuredBuffer(GeometryPassRootSignatureParams::Materials, scene_.GetMaterialData());

		// Bind all textures.
		for(int i = 0; i < scene_.GetTextures().size(); ++i)
		{
			command_list->SetShaderResourceView(GeometryPassRootSignatureParams::Textures, i, scene_.GetTextures()[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		// Loop over all instances of meshes in the scene and render.
		for (auto& instance : scene_.GetInstances())
		{
			Mesh& mesh = scene_.GetMeshes()[instance.MeshIndex];
			mesh.SetBaseTransform(instance.Transform);
			mesh.Render(*command_list);
		}

#if DEBUG_
		// Render light sources.
		//if (visualise_lights_ && scene_.BasicGeometryLoaded())
		//{
		//	DirectX::XMMATRIX translation_matrix	= DirectX::XMMatrixIdentity();
		//	DirectX::XMMATRIX rotation_matrix		= DirectX::XMMatrixIdentity();
		//	DirectX::XMMATRIX scaling_matrix		= DirectX::XMMatrixIdentity();

		//	DirectX::XMMATRIX world_matrix = DirectX::XMMatrixIdentity();

		//	// Render geometry for point light sources
		//	for (const auto& l : point_lights_)
		//	{
		//		DirectX::XMVECTOR light_position = DirectX::XMLoadFloat4(&l.PositionWS);
		//		DirectX::XMVECTOR light_scaling{ 0.1f, 0.1f, 0.1f };

		//		translation_matrix	= DirectX::XMMatrixTranslationFromVector(light_position);
		//		scaling_matrix		= DirectX::XMMatrixScalingFromVector(light_scaling);

		//		world_matrix = scaling_matrix * rotation_matrix * translation_matrix;

		//		scene_.SphereMesh->SetEmissive(DirectX::XMFLOAT3(l.Color.x, l.Color.y, l.Color.z));

		//		scene_.SphereMesh->SetWorldMatrix(world_matrix);
		//		scene_.SphereMesh->Render(render_context);
		//	}
		//}
#endif	
		command_list->EndRenderPass();
	}

	// Raytraced shadows render pass.
	{
		// Bind the heaps, acceleration strucutre and dispatch rays.
		D3D12_DISPATCH_RAYS_DESC dispatch_desc = {};

		dispatch_desc.RayGenerationShaderRecord.StartAddress = raygen_shader_table_.GetD3D12Resource()->GetGPUVirtualAddress();
		dispatch_desc.RayGenerationShaderRecord.SizeInBytes  = raygen_shader_table_.GetShaderRecordSize();

		dispatch_desc.HitGroupTable.StartAddress	= hitgroup_shader_table_.GetD3D12Resource()->GetGPUVirtualAddress();
		dispatch_desc.HitGroupTable.SizeInBytes		= hitgroup_shader_table_.GetD3D12Resource()->GetDesc().Width;
		dispatch_desc.HitGroupTable.StrideInBytes	= hitgroup_shader_table_.GetShaderRecordSize();
		
		dispatch_desc.MissShaderTable.StartAddress	= miss_shader_table_.GetD3D12Resource()->GetGPUVirtualAddress();
		dispatch_desc.MissShaderTable.SizeInBytes	= miss_shader_table_.GetD3D12Resource()->GetDesc().Width;
		dispatch_desc.MissShaderTable.StrideInBytes = miss_shader_table_.GetShaderRecordSize();
			
		dispatch_desc.Width		= width_;
		dispatch_desc.Height	= height_;
		dispatch_desc.Depth		= 1;

		command_list->SetComputeRootSignature(raytracing_global_root_signature_);
		command_list->SetStateObject(raytracing_pass_state_object_);

		// Upload lights
		SceneLightProperties light_props;
		light_props.NumPointLights			= static_cast<uint32_t>(point_lights_.size());
		light_props.NumSpotLights			= static_cast<uint32_t>(spot_lights_.size());
		light_props.NumDirectionalLights	= static_cast<uint32_t>(directional_lights_.size());
	
		command_list->SetComputeDynamicConstantBuffer(RtGlobalRootSignatureParams::SceneConstantData, scene_buffer_);
		command_list->SetCompute32BitConstants(RtGlobalRootSignatureParams::LightPropertiesCb, light_props);
		
		command_list->SetComputeDynamicStructuredBuffer(RtGlobalRootSignatureParams::PointLights, point_lights_);
		command_list->SetComputeDynamicStructuredBuffer(RtGlobalRootSignatureParams::SpotLights, spot_lights_);
		command_list->SetComputeDynamicStructuredBuffer(RtGlobalRootSignatureParams::DirectionalLights, directional_lights_);
		command_list->SetComputeDynamicStructuredBuffer(RtGlobalRootSignatureParams::Materials, scene_.GetMaterialData());
		command_list->SetComputeDynamicStructuredBuffer(RtGlobalRootSignatureParams::MeshInfo, mesh_infos_);

		command_list->SetComputeByteAddressBuffer(RtGlobalRootSignatureParams::Attributes, global_vertices_.GetD3D12Resource()->GetGPUVirtualAddress());
		command_list->SetComputeByteAddressBuffer(RtGlobalRootSignatureParams::Indices, global_indices_.GetD3D12Resource()->GetGPUVirtualAddress());

		command_list->SetShaderResourceView(RtGlobalRootSignatureParams::GBuffer, 0, geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor0));	// Bind albedo.
		command_list->SetShaderResourceView(RtGlobalRootSignatureParams::GBuffer, 1, geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor1));	// Bind normal.
		command_list->SetShaderResourceView(RtGlobalRootSignatureParams::GBuffer, 2, geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor2));	// Bind metal-rough.
		command_list->SetShaderResourceView(RtGlobalRootSignatureParams::GBuffer, 3, geometry_pass_render_target_.GetTexture(AttachmentPoint::kDepthStencil), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, &depth_buffer_view_);	// Bind depth.

		// Bind all textures.
		for (int i = 0; i < scene_.GetTextures().size(); ++i)
		{
			command_list->SetShaderResourceView(RtGlobalRootSignatureParams::Textures, i, scene_.GetTextures()[i]);
		}
		
		command_list->SetComputeAccelerationStructure(RtGlobalRootSignatureParams::AccelerationStructure, top_level_acceleration_structure_.GetD3D12Resource()->GetGPUVirtualAddress());
		command_list->SetUnorderedAccessView(RtGlobalRootSignatureParams::RenderTarget, 0, raytracing_output_texture_);
		
		command_list->DispatchRays(dispatch_desc);

		// Make sure to finish writes to render target.
		command_list->UAVBarrier(raytracing_output_texture_, true);
	}
	
	// Light accumulation render pass.
	{
		command_list->BeginRenderPass(light_accumulation_pass_render_target_);

		command_list->SetViewport(light_accumulation_pass_render_target_.GetViewport());
		command_list->SetScissorRect(scissor_rect_);
		command_list->SetGraphicsRootSignature(light_accumulation_pass_root_signature_);
		command_list->SetPipelineState(light_accumulation_pass_pipeline_state_);
		command_list->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Upload lights
		SceneLightProperties light_props;
		light_props.NumPointLights			= static_cast<uint32_t>(point_lights_.size());
		light_props.NumSpotLights			= static_cast<uint32_t>(spot_lights_.size());
		light_props.NumDirectionalLights	= static_cast<uint32_t>(directional_lights_.size());

		command_list->SetGraphicsDynamicConstantBuffer(LightAccumulationPassRootSignatureParams::SceneConstantData, scene_buffer_);
		command_list->SetGraphics32BitConstants(LightAccumulationPassRootSignatureParams::LightPropertiesCb, light_props);
		command_list->SetGraphicsDynamicStructuredBuffer(LightAccumulationPassRootSignatureParams::PointLights, point_lights_);
		command_list->SetGraphicsDynamicStructuredBuffer(LightAccumulationPassRootSignatureParams::SpotLights, spot_lights_);
		command_list->SetGraphicsDynamicStructuredBuffer(LightAccumulationPassRootSignatureParams::DirectionalLights, directional_lights_);

		command_list->SetShaderResourceView(LightAccumulationPassRootSignatureParams::GBuffer, 0, geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);	// Bind albedo.
		command_list->SetShaderResourceView(LightAccumulationPassRootSignatureParams::GBuffer, 1, geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor1), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);	// Bind normal.
		command_list->SetShaderResourceView(LightAccumulationPassRootSignatureParams::GBuffer, 2, geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor2), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);	// Bind metal-rough.
		command_list->SetShaderResourceView(LightAccumulationPassRootSignatureParams::GBuffer, 3, geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor3), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);	// Bind emissive-occlusion.
		command_list->SetShaderResourceView(LightAccumulationPassRootSignatureParams::GBuffer, 4, geometry_pass_render_target_.GetTexture(AttachmentPoint::kDepthStencil), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &depth_buffer_view_);	// Bind depth.
		command_list->SetShaderResourceView(LightAccumulationPassRootSignatureParams::GBuffer, 5, raytracing_output_texture_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		
		command_list->Draw(3);
		
		command_list->EndRenderPass();
	}

	// Composite render pass.
	{
		command_list->BeginRenderPass(p_window_->GetRenderTarget());
		
		command_list->SetViewport(p_window_->GetRenderTarget().GetViewport());
		command_list->SetScissorRect(scissor_rect_);
		command_list->SetGraphicsRootSignature(composite_pass_root_signature_);
		command_list->SetPipelineState(composite_pass_pipeline_state_);
		command_list->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		command_list->SetGraphics32BitConstants(CompositePassRootSignatureParams::TonemapProperties, g_tonemap_parameters);
		command_list->SetGraphics32BitConstants(CompositePassRootSignatureParams::OutputMode, g_output_mode);
		command_list->SetShaderResourceView(CompositePassRootSignatureParams::OutputTexture, 0, *current_display_texture_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, g_output_mode.Texture == OutputTexture::DepthTexture ? &depth_buffer_view_ : nullptr);

		command_list->Draw(3);

		command_list->EndRenderPass();
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
		case KeyCode::D1:
			current_display_texture_ = &light_accumulation_pass_render_target_.GetTexture(AttachmentPoint::kColor0);
			g_output_mode.Texture = OutputTexture::HDRTexture;
			break;
		case KeyCode::D2:
			current_display_texture_ = &geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor0);
			g_output_mode.Texture = OutputTexture::AlbedoTexture;
			break;
		case KeyCode::D3:
			current_display_texture_ = &geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor1);
			g_output_mode.Texture = OutputTexture::NormalTexture;
			break;
		case KeyCode::D4:
			current_display_texture_ = &geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor2);
			g_output_mode.Texture = OutputTexture::MetalTexture;
			break;
		case KeyCode::D5:
			current_display_texture_ = &geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor2);
			g_output_mode.Texture = OutputTexture::RoughTexture;
			break;
		case KeyCode::D6:
			current_display_texture_ = &geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor3);
			g_output_mode.Texture = OutputTexture::EmissiveTexture;
			break;
		case KeyCode::D7:
			current_display_texture_ = &geometry_pass_render_target_.GetTexture(AttachmentPoint::kColor3);
			g_output_mode.Texture = OutputTexture::OcclusionTexture;
			break;
		case KeyCode::D8:
			current_display_texture_ = &geometry_pass_render_target_.GetTexture(AttachmentPoint::kDepthStencil);
			g_output_mode.Texture = OutputTexture::DepthTexture;
			break;
		case KeyCode::D9:
			current_display_texture_ = &raytracing_output_texture_;
			g_output_mode.Texture = OutputTexture::RayTracedShadowsTexture;
			break;
		case KeyCode::D0:
			current_display_texture_ = &raytracing_output_texture_;
			g_output_mode.Texture = OutputTexture::RayTracedReflectionsTexture;
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

		RescaleRenderTargets(render_scale_);
	}
}

void ReflectionsDemo::RescaleRenderTargets(float scale)
{
	uint32_t width  = static_cast<uint32_t>(width_ * scale);
	uint32_t height = static_cast<uint32_t>(height_ * scale);

	width  = clamp<uint32_t>(width, 1, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);
	height = clamp<uint32_t>(height, 1, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);

	geometry_pass_render_target_.Resize(width, height);
	raytracing_output_texture_.Resize(width, height);
	light_accumulation_pass_render_target_.Resize(width, height);
}
