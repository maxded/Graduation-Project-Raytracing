#include <HybridRayTracingDemo.h>

#include <Application.h>
#include <CommandQueue.h>
#include <CommandList.h>
#include <Window.h>
#include <Material.h>
#include <Camera.h>
#include <Helpers.h>

#include <SponzaScene.h>
#include <DefaultScene.h>

using namespace Microsoft::WRL;
using namespace DirectX;

#include <algorithm> // For std::min and std::max.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include <d3dx12.h>
#include <d3dcompiler.h>

#include <DirectXColors.h>
#include <DirectXMath.h>

enum TonemapMethod : uint32_t
{
	TM_Linear,
	TM_Reinhard,
	TM_ReinhardSq,
	TM_ACESFilmic,
};

struct TonemapParameters
{
	TonemapParameters()
		: TonemapMethod(TM_Reinhard)
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
	{}

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

TonemapParameters g_TonemapParameters;

// An enum for root signature parameters.
// I'm not using scoped enums to avoid the explicit cast that would be required
// to use these as root indices in the root signature.
enum RootParameters
{
	MatricesCB,         // ConstantBuffer<Mat> MatCB : register(b0);
	MaterialCB,         // ConstantBuffer<Material> MaterialCB : register( b0, space1 );   
	LightPropertiesCB,  // ConstantBuffer<LightProperties> LightPropertiesCB : register( b1 );
	PointLights,        // StructuredBuffer<PointLight> PointLights : register( t0 );
	SpotLights,         // StructuredBuffer<SpotLight> SpotLights : register( t1 );
	NumRootParameters
};

HybridRayTracingDemo::HybridRayTracingDemo(const std::wstring& name, int width, int height, bool vSync)
	: Game(name, width, height, vSync)
	, m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, m_Shift(false)
	, m_Width(width)
	, m_Height(height)
	, m_RenderScale(1.0f)
	, m_Scenes{}
	, m_CurrentSceneIndex(0)
{

}

HybridRayTracingDemo::~HybridRayTracingDemo()
{
	
}

bool HybridRayTracingDemo::LoadContent()
{
	auto device			= Application::Get().GetDevice();
	auto commandQueue	= Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto commandList	= commandQueue->GetCommandList();

	std::shared_ptr<SponzaScene> sponzaScene(new SponzaScene);
	sponzaScene->Load("C:\\Users\\mdans\\Documents\\GPE\\HybridRayTracing\\Assets\\Sponza.gltf", *commandList);

	std::shared_ptr<DefaultScene> buggyScene(new DefaultScene);
	buggyScene->Load("C:\\Users\\mdans\\Documents\\GPE\\HybridRayTracing\\Assets\\Buggy.gltf", *commandList);

	m_Scenes.emplace_back(sponzaScene);
	m_Scenes.emplace_back(buggyScene);

	// Create an HDR intermediate render target.
	DXGI_FORMAT HDRFormat			= DXGI_FORMAT_R16G16B16A16_FLOAT;
	DXGI_FORMAT depthBufferFormat	= DXGI_FORMAT_D32_FLOAT;

	// Create an off-screen render target with a single color buffer and a depth buffer.
	auto colorDesc  = CD3DX12_RESOURCE_DESC::Tex2D(HDRFormat, m_Width, m_Height);
	colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE colorClearValue;
	colorClearValue.Format = colorDesc.Format;
	colorClearValue.Color[0] = 0.4f;
	colorClearValue.Color[1] = 0.6f;
	colorClearValue.Color[2] = 0.9f;
	colorClearValue.Color[3] = 1.0f;

	Texture HDRTexture = Texture(colorDesc, &colorClearValue,
		TextureUsage::RenderTarget,
		"HDR Texture");

	// Create a depth buffer for the HDR render target.
	auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat, m_Width, m_Height);
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE depthClearValue;
	depthClearValue.Format = depthDesc.Format;
	depthClearValue.DepthStencil = { 1.0f, 0 };

	Texture depthTexture = Texture(depthDesc, &depthClearValue,
		TextureUsage::Depth,
		"Depth Render Target");

	// Attach the HDR texture to the HDR render target.
	m_HDRRenderTarget.AttachTexture(AttachmentPoint::Color0, HDRTexture);
	m_HDRRenderTarget.AttachTexture(AttachmentPoint::DepthStencil, depthTexture);

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Create a root signature for the HDR pipeline.
	{
		// Load the HDR shaders.
		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> ps;
		ThrowIfFailed(D3DReadFileToBlob(L"HDR_VS.cso", &vs));
		ThrowIfFailed(D3DReadFileToBlob(L"HDR_PS.cso", &ps));

		// Allow input layout and deny unnecessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
		rootParameters[RootParameters::MatricesCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[RootParameters::MaterialCB].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::LightPropertiesCB].InitAsConstants(sizeof(Scene::SceneLightProperties) / 4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::PointLights].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::SpotLights].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(RootParameters::NumRootParameters, rootParameters, 0, nullptr, rootSignatureFlags);

		m_HDRRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

		// Setup the HDR pipeline state.
		struct HDRPipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		} hdrPipelineStateStream;

		std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, Mesh::VertexSlot, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, Mesh::NormalSlot, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, Mesh::TangentSlot, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, Mesh::Texcoord0Slot, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		hdrPipelineStateStream.pRootSignature			= m_HDRRootSignature.GetRootSignature().Get();
		hdrPipelineStateStream.InputLayout				= { &inputLayout[0], static_cast<UINT>(inputLayout.size()) };
		hdrPipelineStateStream.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		hdrPipelineStateStream.VS						= CD3DX12_SHADER_BYTECODE(vs.Get());
		hdrPipelineStateStream.PS						= CD3DX12_SHADER_BYTECODE(ps.Get());
		hdrPipelineStateStream.DSVFormat				= m_HDRRenderTarget.GetDepthStencilFormat();
		hdrPipelineStateStream.RTVFormats				= m_HDRRenderTarget.GetRenderTargetFormats();

		D3D12_PIPELINE_STATE_STREAM_DESC hdrPipelineStateStreamDesc = {
			sizeof(HDRPipelineStateStream), &hdrPipelineStateStream
		};
		ThrowIfFailed(device->CreatePipelineState(&hdrPipelineStateStreamDesc, IID_PPV_ARGS(&m_HDRPipelineState)));
	}

	// Create the SDR Root Signature
	{
		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsConstants(sizeof(TonemapParameters) / 4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC linearClampsSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(2, rootParameters, 1, &linearClampsSampler);

		m_SDRRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

		// Create the SDR PSO
		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> ps;
		ThrowIfFailed(D3DReadFileToBlob(L"HDRtoSDR_VS.cso", &vs));
		ThrowIfFailed(D3DReadFileToBlob(L"HDRtoSDR_PS.cso", &ps));

		CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

		struct SDRPipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		} sdrPipelineStateStream;

		sdrPipelineStateStream.pRootSignature = m_SDRRootSignature.GetRootSignature().Get();
		sdrPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		sdrPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		sdrPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		sdrPipelineStateStream.Rasterizer = rasterizerDesc;
		sdrPipelineStateStream.RTVFormats = m_pWindow->GetRenderTarget().GetRenderTargetFormats();

		D3D12_PIPELINE_STATE_STREAM_DESC sdrPipelineStateStreamDesc = {
			sizeof(SDRPipelineStateStream), &sdrPipelineStateStream
		};
		ThrowIfFailed(device->CreatePipelineState(&sdrPipelineStateStreamDesc, IID_PPV_ARGS(&m_SDRPipelineState)));
	}

	auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);

	return true;
}

void HybridRayTracingDemo::UnloadContent()
{
	
}

static double g_FPS = 0.0;

void HybridRayTracingDemo::OnUpdate(UpdateEventArgs& e)
{
	static uint64_t frameCount = 0;
	static double totalTime = 0.0;

	totalTime += e.ElapsedTime;
	frameCount++;

	Game::OnUpdate(e);

	if (totalTime > 1.0)
	{
		g_FPS = frameCount / totalTime;

		char buffer[512];
		sprintf_s(buffer, "FPS: %f\n", g_FPS);
		OutputDebugStringA(buffer);

		frameCount = 0;
		totalTime = 0.0;
	}

	m_Scenes[m_CurrentSceneIndex]->Update(e); 
}

void HybridRayTracingDemo::OnRender(RenderEventArgs& e)
{
	Game::OnRender(e);

	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();

	Camera& camera = Camera::Get();

	// Clear the render targets.
	{
		FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

		commandList->ClearTexture(m_HDRRenderTarget.GetTexture(AttachmentPoint::Color0), clearColor);
		commandList->ClearDepthStencilTexture(m_HDRRenderTarget.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
	}

	commandList->SetRenderTarget(m_HDRRenderTarget);
	commandList->SetViewport(m_HDRRenderTarget.GetViewport());
	commandList->SetScissorRect(m_ScissorRect);

	commandList->SetPipelineState(m_HDRPipelineState);
	commandList->SetGraphicsRootSignature(m_HDRRootSignature);

	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Ruby);

	m_Scenes[m_CurrentSceneIndex]->Render(*commandList);

	// Perform HDR -> SDR tonemapping directly to the Window's render target.
	commandList->SetRenderTarget(m_pWindow->GetRenderTarget());
	commandList->SetViewport(m_pWindow->GetRenderTarget().GetViewport());
	commandList->SetPipelineState(m_SDRPipelineState);
	commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootSignature(m_SDRRootSignature);
	commandList->SetGraphics32BitConstants(0, g_TonemapParameters);
	commandList->SetShaderResourceView(1, 0, m_HDRRenderTarget.GetTexture(Color0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->Draw(3);

	commandQueue->ExecuteCommandList(commandList);

	// Present
	m_pWindow->Present();
}

void HybridRayTracingDemo::OnKeyPressed(KeyEventArgs& e)
{
	Game::OnKeyPressed(e);

	switch (e.Key)
	{
	case KeyCode::Escape:
		Application::Get().Quit(0);
		break;
	case KeyCode::Enter:
		if (e.Alt)
		{
	case KeyCode::F11:	
		{
			m_pWindow->ToggleFullscreen();
		}
		break;
		}
	case KeyCode::V:
		m_pWindow->ToggleVSync();
		break;
	case KeyCode::Space:
		//m_AnimateLights = !m_AnimateLights;
		break;
	case KeyCode::ShiftKey:
		m_Shift = true;
		break;
	case KeyCode::Right:
		if ((m_CurrentSceneIndex + 1) < m_Scenes.size())
			m_CurrentSceneIndex++;
		break;
	case KeyCode::Left:
		if ((m_CurrentSceneIndex - 1) > -1)
			m_CurrentSceneIndex--;
		break;
	}	
}

void HybridRayTracingDemo::OnKeyReleased(KeyEventArgs& e)
{
	Game::OnKeyReleased(e);

	switch (e.Key)
	{
	case KeyCode::ShiftKey:
		m_Shift = false;
		break;
	}
}

void HybridRayTracingDemo::OnMouseMoved(MouseMotionEventArgs& e)
{
	Game::OnMouseMoved(e);
}

void HybridRayTracingDemo::OnMouseWheel(MouseWheelEventArgs& e)
{
	Game::OnMouseWheel(e);
}

void HybridRayTracingDemo::OnResize(ResizeEventArgs& e)
{
	Game::OnResize(e);

	Camera& camera = Camera::Get();

	if (m_Width != e.Width || m_Height != e.Height)
	{
		m_Width  = std::max(1, e.Width);
		m_Height = std::max(1, e.Height);

		float fov = camera.get_FoV();
		float aspectRatio = m_Width / (float)m_Height;
		camera.set_Projection(fov, aspectRatio, 0.1f, 100.0f);

		RescaleHDRRenderTarget(m_RenderScale);
	}
}

void HybridRayTracingDemo::RescaleHDRRenderTarget(float scale)
{
	uint32_t width = static_cast<uint32_t>(m_Width * scale);
	uint32_t height = static_cast<uint32_t>(m_Height * scale);

	width = clamp<uint32_t>(width, 1, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);
	height = clamp<uint32_t>(height, 1, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);

	m_HDRRenderTarget.Resize(width, height);
}

