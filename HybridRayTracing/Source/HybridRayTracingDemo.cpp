#include <HybridRayTracingDemo.h>

#include <Application.h>
#include <CommandQueue.h>
#include <CommandList.h>
#include <Helpers.h>
#include <Window.h>
#include <StaticMesh.h>
#include <Material.h>

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

struct Mat
{
	XMMATRIX ModelMatrix;
	XMMATRIX ModelViewMatrix;
	XMMATRIX InverseTransposeModelViewMatrix;
	XMMATRIX ModelViewProjectionMatrix;
};

struct LightProperties
{
	uint32_t NumPointLights;
	uint32_t NumSpotLights;
};

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

// Clamp a value between a min and max range.
template<typename T>
constexpr const T& clamp(const T& val, const T& min = T(0), const T& max = T(1))
{
	return val < min ? min : val > max ? max : val;
}

// Builds a look-at (world) matrix from a point, up and direction vectors.
XMMATRIX XM_CALLCONV LookAtMatrix(FXMVECTOR Position, FXMVECTOR Direction, FXMVECTOR Up)
{
	assert(!XMVector3Equal(Direction, XMVectorZero()));
	assert(!XMVector3IsInfinite(Direction));
	assert(!XMVector3Equal(Up, XMVectorZero()));
	assert(!XMVector3IsInfinite(Up));

	XMVECTOR R2 = XMVector3Normalize(Direction);

	XMVECTOR R0 = XMVector3Cross(Up, R2);
	R0 = XMVector3Normalize(R0);

	XMVECTOR R1 = XMVector3Cross(R2, R0);

	XMMATRIX M(R0, R1, R2, Position);

	return M;
}

HybridRayTracingDemo::HybridRayTracingDemo(const std::wstring& name, int width, int height, bool vSync)
	: game(name, width, height, vSync)
	, m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, m_Forward(0)
	, m_Backward(0)
	, m_Left(0)
	, m_Right(0)
	, m_Up(0)
	, m_Down(0)
	, m_Pitch(0)
	, m_Yaw(0)
	, m_AnimateLights(false)
	, m_Shift(false)
	, m_Width(width)
	, m_Height(height)
	, m_RenderScale(1.0f)
{
	XMVECTOR cameraPos		= XMVectorSet(0, 5, -20, 1);
	XMVECTOR cameraTarget	= XMVectorSet(0, 5, 0, 1);
	XMVECTOR cameraUp		= XMVectorSet(0, 1, 0, 0);

	m_Camera.set_LookAt(cameraPos, cameraTarget, cameraUp);
	m_Camera.set_Projection(45.0f, width / (float)height, 0.1f, 100.0f);

	m_pAlignedCameraData = (CameraData*)_aligned_malloc(sizeof(CameraData), 16);

	m_pAlignedCameraData->m_InitialCamPos = m_Camera.get_Translation();
	m_pAlignedCameraData->m_InitialCamRot = m_Camera.get_Rotation();
	m_pAlignedCameraData->m_InitialFov	  = m_Camera.get_FoV();
}

HybridRayTracingDemo::~HybridRayTracingDemo()
{
	_aligned_free(m_pAlignedCameraData);
}

bool HybridRayTracingDemo::LoadContent()
{
	auto device			= Application::Get().GetDevice();
	auto commandQueue	= Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto commandList	= commandQueue->GetCommandList();

	m_CubeMesh		= StaticMesh::CreateCube(*commandList);
	m_SphereMesh	= StaticMesh::CreateSphere(*commandList);
	m_ConeMesh		= StaticMesh::CreateCone(*commandList);
	m_TorusMesh		= StaticMesh::CreateTorus(*commandList);
	m_PlaneMesh		= StaticMesh::CreatePlane(*commandList);

	// Create an HDR intermediate render target.
	DXGI_FORMAT HDRFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

	// Create an off-screen render target with a single color buffer and a depth buffer.
	auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(HDRFormat, m_Width, m_Height);
	colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE colorClearValue;
	colorClearValue.Format = colorDesc.Format;
	colorClearValue.Color[0] = 0.4f;
	colorClearValue.Color[1] = 0.6f;
	colorClearValue.Color[2] = 0.9f;
	colorClearValue.Color[3] = 1.0f;

	Texture HDRTexture = Texture(colorDesc, &colorClearValue,
		TextureUsage::RenderTarget,
		L"HDR Texture");

	// Create a depth buffer for the HDR render target.
	auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat, m_Width, m_Height);
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE depthClearValue;
	depthClearValue.Format = depthDesc.Format;
	depthClearValue.DepthStencil = { 1.0f, 0 };

	Texture depthTexture = Texture(depthDesc, &depthClearValue,
		TextureUsage::Depth,
		L"Depth Render Target");

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
		rootParameters[RootParameters::LightPropertiesCB].InitAsConstants(sizeof(LightProperties) / 4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
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

		hdrPipelineStateStream.pRootSignature			= m_HDRRootSignature.GetRootSignature().Get();
		hdrPipelineStateStream.InputLayout				= { VertexData::InputElements, VertexData::InputElementCount };
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

	game::OnUpdate(e);

	totalTime += e.ElapsedTime;
	frameCount++;

	if (totalTime > 1.0)
	{
		g_FPS = frameCount / totalTime;

		char buffer[512];
		sprintf_s(buffer, "FPS: %f\n", g_FPS);
		OutputDebugStringA(buffer);

		frameCount = 0;
		totalTime = 0.0;
	}

	// Update the camera.
	float speedMultipler = (m_Shift ? 16.0f : 4.0f);

	XMVECTOR cameraTranslate = XMVectorSet(m_Right - m_Left, 0.0f, m_Forward - m_Backward, 1.0f) * speedMultipler * static_cast<float>(e.ElapsedTime);
	XMVECTOR cameraPan = XMVectorSet(0.0f, m_Up - m_Down, 0.0f, 1.0f) * speedMultipler * static_cast<float>(e.ElapsedTime);
	m_Camera.Translate(cameraTranslate, Space::Local);
	m_Camera.Translate(cameraPan, Space::Local);

	XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(m_Pitch), XMConvertToRadians(m_Yaw), 0.0f);
	m_Camera.set_Rotation(cameraRotation);

	XMMATRIX viewMatrix = m_Camera.get_ViewMatrix();

	const int numPointLights = 4;
	const int numSpotLights = 4;

	static const XMVECTORF32 LightColors[] =
	{
		Colors::White, Colors::Orange, Colors::Yellow, Colors::Green, Colors::Blue, Colors::Indigo, Colors::Violet, Colors::White
	};

	static float lightAnimTime = 0.0f;
	if (m_AnimateLights)
	{
		lightAnimTime += static_cast<float>(e.ElapsedTime) * 0.5f * XM_PI;
	}

	const float radius = 8.0f;
	const float offset = 2.0f * XM_PI / numPointLights;
	const float offset2 = offset + (offset / 2.0f);

	// Setup the light buffers.
	m_PointLights.resize(numPointLights);
	for (int i = 0; i < numPointLights; ++i)
	{
		PointLight& l = m_PointLights[i];

		l.PositionWS = {
			static_cast<float>(std::sin(lightAnimTime + offset * i)) * radius,
			9.0f,
			static_cast<float>(std::cos(lightAnimTime + offset * i)) * radius,
			1.0f
		};
		XMVECTOR positionWS = XMLoadFloat4(&l.PositionWS);
		XMVECTOR positionVS = XMVector3TransformCoord(positionWS, viewMatrix);
		XMStoreFloat4(&l.PositionVS, positionVS);

		l.Color = XMFLOAT4(LightColors[i]);
		l.Intensity = 1.0f;
		l.Attenuation = 0.0f;
	}

	m_SpotLights.resize(numSpotLights);
	for (int i = 0; i < numSpotLights; ++i)
	{
		SpotLight& l = m_SpotLights[i];

		l.PositionWS = {
			static_cast<float>(std::sin(lightAnimTime + offset * i + offset2)) * radius,
			9.0f,
			static_cast<float>(std::cos(lightAnimTime + offset * i + offset2)) * radius,
			1.0f
		};
		XMVECTOR positionWS = XMLoadFloat4(&l.PositionWS);
		XMVECTOR positionVS = XMVector3TransformCoord(positionWS, viewMatrix);
		XMStoreFloat4(&l.PositionVS, positionVS);

		XMVECTOR directionWS = XMVector3Normalize(XMVectorSetW(XMVectorNegate(positionWS), 0));
		XMVECTOR directionVS = XMVector3Normalize(XMVector3TransformNormal(directionWS, viewMatrix));
		XMStoreFloat4(&l.DirectionWS, directionWS);
		XMStoreFloat4(&l.DirectionVS, directionVS);

		l.Color = XMFLOAT4(LightColors[numPointLights + i]);
		l.Intensity = 1.0f;
		l.SpotAngle = XMConvertToRadians(45.0f);
		l.Attenuation = 0.0f;
	}
}

void XM_CALLCONV ComputeMatrices(FXMMATRIX model, CXMMATRIX view, CXMMATRIX viewProjection, Mat& mat)
{
	mat.ModelMatrix = model;
	mat.ModelViewMatrix = model * view;
	mat.InverseTransposeModelViewMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, mat.ModelViewMatrix));
	mat.ModelViewProjectionMatrix = model * viewProjection;
}

void HybridRayTracingDemo::OnRender(RenderEventArgs& e)
{
	game::OnRender(e);

	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();

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

	// Upload lights
	LightProperties lightProps;
	lightProps.NumPointLights = static_cast<uint32_t>(m_PointLights.size());
	lightProps.NumSpotLights  = static_cast<uint32_t>(m_SpotLights.size());

	commandList->SetGraphics32BitConstants(RootParameters::LightPropertiesCB, lightProps);
	commandList->SetGraphicsDynamicStructuredBuffer(RootParameters::PointLights, m_PointLights);
	commandList->SetGraphicsDynamicStructuredBuffer(RootParameters::SpotLights, m_SpotLights);

	// Draw the earth sphere
	XMMATRIX translationMatrix	  = XMMatrixTranslation(-4.0f, 2.0f, -4.0f);
	XMMATRIX rotationMatrix		  = XMMatrixIdentity();
	XMMATRIX scaleMatrix		  = XMMatrixScaling(4.0f, 4.0f, 4.0f);
	XMMATRIX worldMatrix		  = scaleMatrix * rotationMatrix * translationMatrix;
	XMMATRIX viewMatrix			  = m_Camera.get_ViewMatrix();
	XMMATRIX viewProjectionMatrix = viewMatrix * m_Camera.get_ProjectionMatrix();

	Mat matrices;
	ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Gold);

	m_SphereMesh->Render(*commandList);

	// Draw a cube
	translationMatrix = XMMatrixTranslation(4.0f, 4.0f, 4.0f);
	rotationMatrix = XMMatrixRotationY(XMConvertToRadians(45.0f));
	scaleMatrix = XMMatrixScaling(4.0f, 8.0f, 4.0f);
	worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

	ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Ruby);

	m_CubeMesh->Render(*commandList);

	// Draw a torus
	translationMatrix = XMMatrixTranslation(4.0f, 0.6f, -4.0f);
	rotationMatrix = XMMatrixRotationY(XMConvertToRadians(45.0f));
	scaleMatrix = XMMatrixScaling(4.0f, 4.0f, 4.0f);
	worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

	ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Silver);

	m_TorusMesh->Render(*commandList);

	// Floor plane.
	float scalePlane = 20.0f;
	float translateOffset = scalePlane / 2.0f;

	translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	rotationMatrix = XMMatrixIdentity();
	scaleMatrix = XMMatrixScaling(scalePlane, 1.0f, scalePlane);
	worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

	ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Magenta);

	m_PlaneMesh->Render(*commandList);

	// Back wall
	translationMatrix = XMMatrixTranslation(0, translateOffset, translateOffset);
	rotationMatrix = XMMatrixRotationX(XMConvertToRadians(-90));
	worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

	ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Blue);

	m_PlaneMesh->Render(*commandList);

	// Ceiling plane
	translationMatrix = XMMatrixTranslation(0, translateOffset * 2.0f, 0);
	rotationMatrix = XMMatrixRotationX(XMConvertToRadians(180));
	worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

	ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Yellow);

	m_PlaneMesh->Render(*commandList);

	// Front wall
	translationMatrix = XMMatrixTranslation(0, translateOffset, -translateOffset);
	rotationMatrix = XMMatrixRotationX(XMConvertToRadians(90));
	worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

	ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Jade);

	m_PlaneMesh->Render(*commandList);

	// Left wall
	translationMatrix = XMMatrixTranslation(-translateOffset, translateOffset, 0);
	rotationMatrix = XMMatrixRotationX(XMConvertToRadians(-90)) * XMMatrixRotationY(XMConvertToRadians(-90));
	worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

	ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Red);

	m_PlaneMesh->Render(*commandList);

	// Right wall
	translationMatrix = XMMatrixTranslation(translateOffset, translateOffset, 0);
	rotationMatrix = XMMatrixRotationX(XMConvertToRadians(-90)) * XMMatrixRotationY(XMConvertToRadians(90));
	worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

	ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
	commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, Material::Green);
	m_PlaneMesh->Render(*commandList);

	// Draw shapes to visualize the position of the lights in the scene.
	Material lightMaterial;
	// No specular
	lightMaterial.Specular = { 0, 0, 0, 1 };
	for (const auto& l : m_PointLights)
	{
		lightMaterial.Emissive = l.Color;
		XMVECTOR lightPos = XMLoadFloat4(&l.PositionWS);
		worldMatrix = XMMatrixTranslationFromVector(lightPos);
		ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

		commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
		commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, lightMaterial);

		m_SphereMesh->Render(*commandList);
	}

	for (const auto& l : m_SpotLights)
	{
		lightMaterial.Emissive = l.Color;
		XMVECTOR lightPos = XMLoadFloat4(&l.PositionWS);
		XMVECTOR lightDir = XMLoadFloat4(&l.DirectionWS);
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);

		// Rotate the cone so it is facing the Z axis.
		rotationMatrix = XMMatrixRotationX(XMConvertToRadians(-90.0f));
		worldMatrix = rotationMatrix * LookAtMatrix(lightPos, lightDir, up);

		ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

		commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
		commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, lightMaterial);

		m_ConeMesh->Render(*commandList);
	}

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
	game::OnKeyPressed(e);

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
	case KeyCode::R:
		// Reset camera transform
		m_Camera.set_Translation(m_pAlignedCameraData->m_InitialCamPos);
		m_Camera.set_Rotation(m_pAlignedCameraData->m_InitialCamRot);
		m_Camera.set_FoV(m_pAlignedCameraData->m_InitialFov);
		m_Pitch = 0.0f;
		m_Yaw = 0.0f;
		break;
	case KeyCode::Up:
	case KeyCode::W:
		m_Forward = 1.0f;
		break;
	case KeyCode::Left:
	case KeyCode::A:
		m_Left = 1.0f;
		break;
	case KeyCode::Down:
	case KeyCode::S:
		m_Backward = 1.0f;
		break;
	case KeyCode::Right:
	case KeyCode::D:
		m_Right = 1.0f;
		break;
	case KeyCode::Q:
		m_Down = 1.0f;
		break;
	case KeyCode::E:
		m_Up = 1.0f;
		break;
	case KeyCode::Space:
		m_AnimateLights = !m_AnimateLights;
		break;
	case KeyCode::ShiftKey:
		m_Shift = true;
		break;
	}	
}

void HybridRayTracingDemo::OnKeyReleased(KeyEventArgs& e)
{
	game::OnKeyReleased(e);

	switch (e.Key)
	{
	case KeyCode::Enter:
	case KeyCode::Up:
	case KeyCode::W:
		m_Forward = 0.0f;
		break;
	case KeyCode::Left:
	case KeyCode::A:
		m_Left = 0.0f;
		break;
	case KeyCode::Down:
	case KeyCode::S:
		m_Backward = 0.0f;
		break;
	case KeyCode::Right:
	case KeyCode::D:
		m_Right = 0.0f;
		break;
	case KeyCode::Q:
		m_Down = 0.0f;
		break;
	case KeyCode::E:
		m_Up = 0.0f;
		break;
	case KeyCode::ShiftKey:
		m_Shift = false;
		break;
	}
}

void HybridRayTracingDemo::OnMouseMoved(MouseMotionEventArgs& e)
{
	game::OnMouseMoved(e);

}

void HybridRayTracingDemo::OnMouseWheel(MouseWheelEventArgs& e)
{
	game::OnMouseWheel(e);
}


void HybridRayTracingDemo::OnResize(ResizeEventArgs& e)
{
	game::OnResize(e);

	if (m_Width != e.Width || m_Height != e.Height)
	{
		m_Width  = std::max(1, e.Width);
		m_Height = std::max(1, e.Height);

		float fov = m_Camera.get_FoV();
		float aspectRatio = m_Width / (float)m_Height;
		m_Camera.set_Projection(fov, aspectRatio, 0.1f, 100.0f);

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

