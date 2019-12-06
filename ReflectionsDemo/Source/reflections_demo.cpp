#include "reflections_demo.h"

#include "neel_engine.h"
#include "commandqueue.h"
#include "window.h"
#include "material.h"
#include "camera.h"
#include "helpers.h"

#include "sponza_scene.h"
#include "default_scene.h"

using namespace Microsoft::WRL;
using namespace DirectX;

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include <d3dcompiler.h>


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

// An enum for root signature parameters.
// I'm not using scoped enums to avoid the explicit cast that would be required
// to use these as root indices in the root signature.
enum RootParameters
{
	kMeshConstantBuffer,		// ConstantBuffer<Mat> MatCB : register(b0);
	kMeshMaterialBuffer,		// ConstantBuffer<MaterialFake> MaterialCB : register( b0, space1 );   
	kLightPropertiesCb,			// ConstantBuffer<LightProperties> LightPropertiesCB : register( b1 );
	kPointLights,				// StructuredBuffer<PointLight> PointLights : register( t0 );
	kSpotLights,				// StructuredBuffer<SpotLight> SpotLights : register( t1 );
	kTextures,					// Texture2D DiffuseTexture : register( t2 );
	kNumRootParameters
};

ReflectionsDemo::ReflectionsDemo(const std::wstring& name, int width, int height, bool v_sync)
	: Game(name, width, height, v_sync)
	  , scissor_rect_(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	  , shift_(false)
	  , width_(width)
	  , height_(height)
	  , render_scale_(1.0f)
{
}

ReflectionsDemo::~ReflectionsDemo()
{
}

bool ReflectionsDemo::LoadContent()
{
	auto device = NeelEngine::Get().GetDevice();
	auto command_queue = NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto command_list = command_queue->GetCommandList();

	// Create an HDR intermediate render target.
	DXGI_FORMAT hdr_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	DXGI_FORMAT depth_buffer_format = DXGI_FORMAT_D32_FLOAT;

	// Create an off-screen render target with a single color buffer and a depth buffer.
	auto color_desc = CD3DX12_RESOURCE_DESC::Tex2D(hdr_format, width_, height_);
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
	auto depth_desc = CD3DX12_RESOURCE_DESC::Tex2D(depth_buffer_format, width_, height_);
	depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE depth_clear_value;
	depth_clear_value.Format = depth_desc.Format;
	depth_clear_value.DepthStencil = {1.0f, 0};

	Texture depth_texture = Texture(depth_desc, &depth_clear_value,
	                               TextureUsage::kDepth,
	                               "Depth Render Target");

	// Attach the HDR texture to the HDR render target.
	hdr_render_target_.AttachTexture(AttachmentPoint::kColor0, hdr_texture);
	hdr_render_target_.AttachTexture(AttachmentPoint::kDepthStencil, depth_texture);

	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
	feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
	{
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Create a root signature for the HDR pipeline.
	{
		// Load the HDR shaders.
		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> ps;
		ThrowIfFailed(D3DReadFileToBlob(L"HDR_VS.cso", &vs));
		ThrowIfFailed(D3DReadFileToBlob(L"HDR_PS.cso", &ps));

		// Allow input layout and deny unnecessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 descriptor_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

		CD3DX12_ROOT_PARAMETER1 root_parameters[RootParameters::kNumRootParameters];
		root_parameters[RootParameters::kMeshConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
		root_parameters[RootParameters::kMeshMaterialBuffer].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[RootParameters::kLightPropertiesCb].InitAsConstants(sizeof(Scene::SceneLightProperties) / 4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[RootParameters::kPointLights].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[RootParameters::kSpotLights].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[RootParameters::kTextures].InitAsDescriptorTable(1, &descriptor_range, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC linear_repeat_sampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);
		
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
		root_signature_description.Init_1_1(RootParameters::kNumRootParameters, root_parameters, 1, &linear_repeat_sampler, root_signature_flags);
		
		hdr_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);

		// Setup the HDR pipeline State.
		struct HDRPipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE PRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		} hdr_pipeline_state_stream;

		std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout =
		{
			{
				"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, Mesh::vertex_slot_, 0,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
			},
			{
				"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, Mesh::normal_slot_, 0,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
			},
			{
				"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, Mesh::tangent_slot_, 0,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
			},
			{
				"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, Mesh::texcoord0_slot_, 0,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
			}
		};

		hdr_pipeline_state_stream.PRootSignature = hdr_root_signature_.GetRootSignature().Get();
		hdr_pipeline_state_stream.InputLayout = {&input_layout[0], static_cast<UINT>(input_layout.size())};
		hdr_pipeline_state_stream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		hdr_pipeline_state_stream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		hdr_pipeline_state_stream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		hdr_pipeline_state_stream.DSVFormat = hdr_render_target_.GetDepthStencilFormat();
		hdr_pipeline_state_stream.RTVFormats = hdr_render_target_.GetRenderTargetFormats();

		D3D12_PIPELINE_STATE_STREAM_DESC hdr_pipeline_state_stream_desc = {
			sizeof(HDRPipelineStateStream), &hdr_pipeline_state_stream
		};
		ThrowIfFailed(device->CreatePipelineState(&hdr_pipeline_state_stream_desc, IID_PPV_ARGS(&hdr_pipeline_state_)));
	}

	// Create the SDR Root Signature
	{
		CD3DX12_DESCRIPTOR_RANGE1 descriptor_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 root_parameters[2];
		root_parameters[0].InitAsConstants(sizeof(TonemapParameters) / 4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[1].InitAsDescriptorTable(1, &descriptor_range, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC linear_clamps_sampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
		                                                D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		                                                D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		                                                D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
		root_signature_description.Init_1_1(2, root_parameters, 1, &linear_clamps_sampler);

		sdr_root_signature_.SetRootSignatureDesc(root_signature_description.Desc_1_1, feature_data.HighestVersion);

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

	SponzaScene sponza_scene;
	sponza_scene.Load("C:\\Users\\mdans\\Documents\\NeelEngine\\ReflectionsDemo\\Assets\\Sponza\\Sponza.gltf");

	current_scene_ = std::make_unique<SponzaScene>(sponza_scene);

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

		char buffer[512];
		sprintf_s(buffer, "FPS: %f\n", g_fps);
		OutputDebugStringA(buffer);

		frame_count = 0;
		total_time = 0.0;
	}

	current_scene_->Update(e);
}

void ReflectionsDemo::OnRender(RenderEventArgs& e)
{
	Game::OnRender(e);

	auto command_queue = NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto command_list = command_queue->GetCommandList();

	Camera& camera = Camera::Get();

	// Clear the render targets.
	{
		FLOAT clear_color[] = {0.4f, 0.6f, 0.9f, 1.0f};

		command_list->ClearTexture(hdr_render_target_.GetTexture(AttachmentPoint::kColor0), clear_color);
		command_list->ClearDepthStencilTexture(hdr_render_target_.GetTexture(AttachmentPoint::kDepthStencil),
		                                      D3D12_CLEAR_FLAG_DEPTH);
	}

	command_list->SetRenderTarget(hdr_render_target_);
	command_list->SetViewport(hdr_render_target_.GetViewport());
	command_list->SetScissorRect(scissor_rect_);

	command_list->SetPipelineState(hdr_pipeline_state_);
	command_list->SetGraphicsRootSignature(hdr_root_signature_);

	command_list->SetGraphicsDynamicConstantBuffer(RootParameters::kMeshMaterialBuffer, MaterialFake::Ruby);

	current_scene_->Render(*command_list);

	// Perform HDR -> SDR tonemapping directly to the Window's render target.
	command_list->SetRenderTarget(p_window_->GetRenderTarget());
	command_list->SetViewport(p_window_->GetRenderTarget().GetViewport());
	command_list->SetPipelineState(sdr_pipeline_state_);
	command_list->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->SetGraphicsRootSignature(sdr_root_signature_);
	command_list->SetGraphics32BitConstants(0, g_tonemap_parameters);
	command_list->SetShaderResourceView(1, 0, hdr_render_target_.GetTexture(kColor0),
	                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	command_list->Draw(3);

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
			//animate_lights_ = !animate_lights_;
			break;
		case KeyCode::ShiftKey:
			shift_ = true;
			break;
		case KeyCode::D1:
		{		
			current_scene_->Unload();
			current_scene_ = nullptr;
			
			SponzaScene sponza_scene;
			sponza_scene.Load("C:\\Users\\mdans\\Documents\\NeelEngine\\ReflectionsDemo\\Assets\\Sponza\\Sponza.gltf");

			current_scene_ = std::make_unique<SponzaScene>(sponza_scene);
			break;
		}
		case KeyCode::D2:
		{
			current_scene_->Unload();
			current_scene_ = nullptr;

			DefaultScene scifi_helmet_scene;
			scifi_helmet_scene.Load("C:\\Users\\mdans\\Documents\\NeelEngine\\ReflectionsDemo\\Assets\\SciFiHelmet\\SciFiHelmet.gltf");

			current_scene_ = std::make_unique<DefaultScene>(scifi_helmet_scene);
			break;
		}
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
