#include "reflections_demo.h"

#include "neel_engine.h"
#include "commandqueue.h"
#include "window.h"
#include "material.h"
#include "camera.h"
#include "helpers.h"

#include "sponza_scene.h"
#include "default_scene.h"
#include "raytracing_scene.h"

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

ReflectionsDemo::ReflectionsDemo(const std::wstring& name, int width, int height, bool v_sync)
	: Game(name, width, height, v_sync)
	  , shift_(false)
	  , width_(width)
	  , height_(height)
	  , render_scale_(1.0f)
	  , scissor_rect_(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
{
}

ReflectionsDemo::~ReflectionsDemo()
{
	
}

bool ReflectionsDemo::LoadContent()
{
	auto device = NeelEngine::Get().GetDevice();
	auto command_queue = NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto command_list = command_queue->GetCommandList();

	current_scene_ = std::make_unique<RayTracingScene>();
	current_scene_->Load("Assets/BasicGeometry/Cube.gltf");

	// Create the SDR Root Signature
	{
		// Check root signature highest version support.
		D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
		{
			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

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

	current_scene_->PrepareRender(*command_list);

	current_scene_->Render(*command_list);

	// Perform HDR -> SDR tonemapping directly to the Window's render target.
	command_list->SetRenderTarget(p_window_->GetRenderTarget());
	command_list->SetViewport(p_window_->GetRenderTarget().GetViewport());
	command_list->SetScissorRect(scissor_rect_);
	command_list->SetPipelineState(sdr_pipeline_state_);
	command_list->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->SetGraphicsRootSignature(sdr_root_signature_);
	command_list->SetGraphics32BitConstants(0, g_tonemap_parameters);
	command_list->SetShaderResourceView(1, 0, current_scene_->GetRenderTarget().GetTexture(kColor0),
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
			current_scene_ = std::make_unique<SponzaScene>();
			current_scene_->Load("Assets/Sponza/Sponza.gltf");
			break;
		}
		case KeyCode::D2:
		{
			current_scene_->Unload();
			current_scene_ = std::make_unique<DefaultScene>();
			current_scene_->Load("Assets/SciFiHelmet/SciFiHelmet.gltf");
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

	current_scene_->GetRenderTarget().Resize(width, height);
}
