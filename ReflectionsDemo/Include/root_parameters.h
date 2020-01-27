#pragma once

namespace GeometryPassRootSignatureParams
{
	enum
	{
		Materials = 0,			// ConstantBuffer<MaterialData> Materials				: register( b0 );
		MeshConstantBuffer,		// ConstantBuffer<Mat> MatCB							: register( b1 );			
		Textures,				// Texture2D textures[5]								: register( t0 );
		NumRootParameters
	};
}

namespace LightAccumulationPassRootSignatureParams
{
	enum
	{
		SceneConstantData = 0,	// ConstantBuffer<SceneConstantData> SceneDataCB		: register( b0 );
		LightPropertiesCb,		// ConstantBuffer<LightProperties> LightPropertiesCB	: register( b1 );
		PointLights,			// StructuredBuffer<PointLight> PointLights				: register( t0 );
		SpotLights,				// StructuredBuffer<SpotLight> SpotLights				: register( t1 );
		DirectionalLights,		// StructuredBuffer<DirectionalLight> DirectionalLights : register( t2 );
		GBuffer,				// Texture2D GBuffer[8]									: register( t3 );
		NumRootParameters
	};
}

namespace CompositePassRootSignatureParams
{
	enum
	{
		TonemapProperties = 0,	// ConstantBuffer<TonemapParameters> TonemapParametersCB	: register( b0 );
		OutputMode,				// ConstantBuffer<OutputMode> OutputModeCB					: register( b1 );
		OutputTexture,			// Texture2D HDRTexture										: register( t0 );
		NumRootParameters
	};
}

namespace RtShadowsPassGlobalRootSignatureParams
{
	enum
	{
		RenderTarget = 0,		// RWTexture2D<float> RenderTarget						: register( u0 );
		SceneConstantData,		// ConstantBuffer<SceneData> SceneCB					: register( b0 );
		LightPropertiesCb,		// ConstantBuffer<LightProperties> LightPropertiesCB	: register( b1 );
		PointLights,			// StructuredBuffer<PointLight> PointLights				: register( t0 );
		SpotLights,				// StructuredBuffer<SpotLight> SpotLights				: register( t1 );
		DirectionalLights,		// StructuredBuffer<DirectionalLight> DirectionalLights : register( t2 );
		AccelerationStructure,	// RaytracingAccelerationStructure Scene				: register( t3 );
		Textures,				// Texture2D Textures[2]								: register( t4 );
		NumRootParameters
	};
}
