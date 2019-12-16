#include "default_scene.h"
#include "neel_engine.h"
#include "commandqueue.h"
#include "window.h"
#include "material.h"

#include "helpers.h"

#include <d3dcompiler.h>

DefaultScene::DefaultScene()
	: scissor_rect_(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
{
}

DefaultScene::~DefaultScene()
{
}

void DefaultScene::Load(const std::string& filename)
{
	auto device = NeelEngine::Get().GetDevice();
	auto command_queue = NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto command_list = command_queue->GetCommandList();

	LoadFromFile(filename, *command_list);

	// Create an HDR intermediate render target.
	{
		DXGI_FORMAT hdr_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT depth_buffer_format = DXGI_FORMAT_D32_FLOAT;

		// Create an off-screen render target with a single color buffer and a depth buffer.
		int width = NeelEngine::Get().GetWindow()->GetClientWidth();
		int height = NeelEngine::Get().GetWindow()->GetClientHeight();

		auto color_desc = CD3DX12_RESOURCE_DESC::Tex2D(hdr_format, width, height);
		color_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE color_clear_value;
		color_clear_value.Format = color_desc.Format;
		color_clear_value.Color[0] = 0.4f;
		color_clear_value.Color[1] = 0.6f;
		color_clear_value.Color[2] = 0.9f;
		color_clear_value.Color[3] = 1.0f;

		Texture hdr_texture = Texture(color_desc, &color_clear_value,
			TextureUsage::kRenderTarget,
			"HDR Texture");

		// Create a depth buffer for the HDR render target.
		auto depth_desc = CD3DX12_RESOURCE_DESC::Tex2D(depth_buffer_format, width, height);
		depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depth_clear_value;
		depth_clear_value.Format = depth_desc.Format;
		depth_clear_value.DepthStencil = { 1.0f, 0 };

		Texture depth_texture = Texture(depth_desc, &depth_clear_value,
			TextureUsage::kDepth,
			"Depth Render Target");

		// Attach the HDR texture to the HDR render target.
		hdr_render_target_.AttachTexture(AttachmentPoint::kColor0, hdr_texture);
		hdr_render_target_.AttachTexture(AttachmentPoint::kDepthStencil, depth_texture);
	}

	// Create a root signature for the default scene.
	{
		// Check root signature highest version support.
		D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
		{
			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		// Allow input layout and deny unnecessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 descriptor_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, document_data_.Textures.size(), 4);

		CD3DX12_ROOT_PARAMETER1 root_parameters[RootParameters::NumRootParameters];
		root_parameters[RootParameters::MeshConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
		root_parameters[RootParameters::LightPropertiesCb].InitAsConstants(sizeof(Scene::SceneLightProperties) / 4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[RootParameters::Materials].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[RootParameters::PointLights].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[RootParameters::SpotLights].InitAsShaderResourceView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[RootParameters::DirectionalLights].InitAsShaderResourceView(3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[RootParameters::Textures].InitAsDescriptorTable(1, &descriptor_range, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC linear_repeat_sampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);

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
		root_signature_description.Init_1_1(RootParameters::NumRootParameters, root_parameters, 1, &linear_repeat_sampler, root_signature_flags);

		default_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);
	}

	// Create pipeline state object with shader permutations for default scene.
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

		hdr_pipeline_state_stream.PRootSignature = default_root_signature_.GetRootSignature().Get();
		hdr_pipeline_state_stream.InputLayout = { &input_layout[0], static_cast<UINT>(input_layout.size()) };
		hdr_pipeline_state_stream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);;
		hdr_pipeline_state_stream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		hdr_pipeline_state_stream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		hdr_pipeline_state_stream.DSVFormat = hdr_render_target_.GetDepthStencilFormat();
		hdr_pipeline_state_stream.RTVFormats = hdr_render_target_.GetRenderTargetFormats();

		Microsoft::WRL::ComPtr<ID3DBlob> pixel_blob = CompileShaderPerumutation("main", ShaderOptions::USE_AUTO_COLOR);
		hdr_pipeline_state_stream.PS = CD3DX12_SHADER_BYTECODE(pixel_blob.Get());

		D3D12_PIPELINE_STATE_STREAM_DESC hdr_pipeline_state_stream_desc = { sizeof(HDRPipelineStateStream), &hdr_pipeline_state_stream };
		ThrowIfFailed(device->CreatePipelineState(&hdr_pipeline_state_stream_desc, IID_PPV_ARGS(&default_pipeline_state_map_[ShaderOptions::USE_AUTO_COLOR])));

		// Compile shader permutations for all required options for meshes.
		for (const auto& mesh : document_data_.Meshes)
		{
			std::vector<ShaderOptions> required_options = mesh.RequiredShaderOptions();

			for (const ShaderOptions options : required_options)
			{
				if (default_pipeline_state_map_[options] == nullptr)
				{
					Microsoft::WRL::ComPtr<ID3DBlob> ps_blob = CompileShaderPerumutation("main", options);
					hdr_pipeline_state_stream.PS = CD3DX12_SHADER_BYTECODE(ps_blob.Get());

					D3D12_PIPELINE_STATE_STREAM_DESC state_stream_desc = { sizeof(HDRPipelineStateStream), &hdr_pipeline_state_stream };
					ThrowIfFailed(device->CreatePipelineState(&state_stream_desc, IID_PPV_ARGS(&default_pipeline_state_map_[options])));
				}
			}
		}
	}

	auto fence_value = command_queue->ExecuteCommandList(command_list);
	command_queue->WaitForFenceValue(fence_value);
}

void DefaultScene::Update(UpdateEventArgs& e)
{
	Scene::Update(e);
}

void DefaultScene::PrepareRender(CommandList& command_list)
{
	// Clear the render targets.
	{
		FLOAT clear_color[] = { 0.4f, 0.6f, 0.9f, 1.0f };

		command_list.ClearTexture(hdr_render_target_.GetTexture(AttachmentPoint::kColor0), clear_color);
		command_list.ClearDepthStencilTexture(hdr_render_target_.GetTexture(AttachmentPoint::kDepthStencil),
			D3D12_CLEAR_FLAG_DEPTH);
	}

	command_list.SetRenderTarget(hdr_render_target_);
	command_list.SetViewport(hdr_render_target_.GetViewport());
	command_list.SetScissorRect(scissor_rect_);

	command_list.SetGraphicsRootSignature(default_root_signature_);
}

void DefaultScene::Render(CommandList& command_list)
{
	RenderContext render_context
	{
		command_list,
		ShaderOptions::None,
		ShaderOptions::None,
		default_pipeline_state_map_
	};

	// Upload lights
	SceneLightProperties light_props;
	light_props.NumPointLights = static_cast<uint32_t>(point_lights_.size());
	light_props.NumSpotLights = static_cast<uint32_t>(spot_lights_.size());
	light_props.NumDirectionalLights = static_cast<uint32_t>(directional_lights_.size());

	command_list.SetGraphics32BitConstants(RootParameters::LightPropertiesCb, light_props);
	command_list.SetGraphicsDynamicStructuredBuffer(RootParameters::PointLights, point_lights_);
	command_list.SetGraphicsDynamicStructuredBuffer(RootParameters::SpotLights, spot_lights_);
	command_list.SetGraphicsDynamicStructuredBuffer(RootParameters::DirectionalLights, directional_lights_);

	// Stage all descriptors for textures.
	for (int i = 0; i < document_data_.Textures.size(); i++)
	{
		command_list.SetShaderResourceView(RootParameters::Textures, i, document_data_.Textures[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	// Loop over all instances of scene and render
	for (auto& instance : mesh_instances_)
	{
		Mesh& mesh = document_data_.Meshes[instance.MeshIndex];
		mesh.SetBaseTransform(instance.Transform);
		mesh.Render(render_context);
	}
}

RenderTarget& DefaultScene::GetRenderTarget()
{
	return hdr_render_target_;
}
