#pragma once

namespace HDRRootSignatureParams
{
	enum
	{
		Materials = 0,			// ConstantBuffer<MaterialData> Materials				: register( b0 );
		MeshConstantBuffer,		// ConstantBuffer<Mat> MatCB							: register( b1 );		
		LightPropertiesCb,		// ConstantBuffer<LightProperties> LightPropertiesCB	: register( b2 );
		PointLights,			// StructuredBuffer<PointLight> PointLights				: register( t0 );
		SpotLights,				// StructuredBuffer<SpotLight> SpotLights				: register( t1 );
		DirectionalLights,		// StructuredBuffer<DirectionalLight> DirectionalLights : register( t2 );
		Textures,				// Texture2D textures[5]								: register( t3 );
		NumRootParameters
	};
}

namespace SDRRootSignatureParams
{
	enum
	{
		TonemapProperties = 0,	// ConstantBuffer<TonemapParameters> TonemapParametersCB	: register( b0 );
		HDRTexture ,			// Texture2D HDRTexture										: register( t0 );
		NumRootParameters
	};
}

namespace GlobalRootSignatureParams
{
	enum
	{
		RenderTarget = 0,
		SceneConstantBuffer,
		DepthMap,
		AccelerationStructure,
		NumRootParameters
	};
}
