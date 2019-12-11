struct MeshConstantData
{
	float4x4 ModelMatrix;
	//----------------------------------- (64 byte boundary)
	float4x4 ModelViewMatrix;
	//----------------------------------- (64 byte boundary)
	float4x4 InverseTransposeModelViewMatrix;
	//----------------------------------- (64 byte boundary)
	float4x4 ModelViewProjectionMatrix;
	//----------------------------------- (64 byte boundary)

	int MaterialIndex;
	float3 Padding;
	//----------------------------------- (16 byte boundary)
	float4 CameraPosition;
	//----------------------------------- (16 byte boundary)
	// Total:                              64 * 3 + 16 = 224 bytes
};

struct MaterialData
{
	float4 MeshAutoColor;
	//----------------------------------- (16 byte boundary)
	float4 BaseColorFactor;
	//----------------------------------- (16 byte boundary)
	int BaseColorIndex;
	int NormalIndex;
	float NormalScale;
	int MetalRoughIndex;
	//----------------------------------- (16 byte boundary)
	float RoughnessFactor;
	float MetallicFactor;
	int AoIndex;
	float AoStrength;
	//----------------------------------- (16 byte boundary)
	int EmissiveIndex;
	float3 EmissiveFactor;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 5 = 80 bytes 
};

struct PointLight
{
	float4 PositionWS; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	float4 PositionVS; // Light position in view space.
	//----------------------------------- (16 byte boundary)
	float4 Color;
	//----------------------------------- (16 byte boundary)
	float       Intensity;
	float       Attenuation;
	float2      Padding;                // Pad to 16 bytes
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 4 = 64 bytes
};

struct SpotLight
{
	float4 PositionWS; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	float4 PositionVS; // Light position in view space.
	//----------------------------------- (16 byte boundary)
	float4 DirectionWS; // Light direction in world space.
	//----------------------------------- (16 byte boundary)
	float4 DirectionVS; // Light direction in view space.
	//----------------------------------- (16 byte boundary)
	float4 Color;
	//----------------------------------- (16 byte boundary)
	float       Intensity;
	float       SpotAngle;
	float       Attenuation;
	float       Padding;                // Pad to 16 bytes.
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 6 = 96 bytes
};

struct DirectionalLight
{
	float4 DirectionWS; // Light direction in world space.
	//----------------------------------- (16 byte boundary)
	float4 DirectionVS; // Light direction in view space.
	//----------------------------------- (16 byte boundary)
	float4 Color;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 3 = 48 bytes 
};

struct LightProperties
{
	uint NumPointLights;
	uint NumSpotLights;
};

struct LightResult
{
	float4 Diffuse;
	float4 Specular;
};

ConstantBuffer<MeshConstantData>		MeshCB				: register(b0);
ConstantBuffer<LightProperties>			LightPropertiesCB	: register(b1);

StructuredBuffer<MaterialData>			Materials			: register(t0);
StructuredBuffer<PointLight>			PointLights			: register(t1);
StructuredBuffer<SpotLight>				SpotLights			: register(t2);
StructuredBuffer<DirectionalLight>		DirectionalLights	: register(t3);

Texture2D Textures[73]				: register(t4);

SamplerState LinearRepeatSampler    : register(s0);

static const float M_PI = 3.141592653589793f;

float3 LinearToSRGB(float3 x)
{
	// This is exactly the sRGB curve
	//return x < 0.0031308 ? 12.92 * x : 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055;

	// This is cheaper but nearly equivalent
	return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt(abs(x - 0.00228)) - 0.13448 * x + 0.005719;
}

float4 SRGBtoLINEAR(float4 srgbColor)
{
#if MANUAL_SRGB
	return float4(pow(srgbColor.xyz, 2.2), srgbColor.w);
#else
	return srgbColor;
#endif
}

float3 Diffuse_Lambert(float3 diffuseColor)
{
	return diffuseColor / M_PI;
}

float3 F_Schlick(float3 r0, float3 f90, float LdH)
{
	return r0 + (f90 - r0) * pow(clamp(1.0 - LdH, 0.0, 1.0), 5.0);
}

float G_Smith(float NdL, float NdV, float alphaRoughness)
{
	float a2 = alphaRoughness * alphaRoughness;

	float gl = NdL + sqrt(a2 + (1.0 - a2) * (NdL * NdL));
	float gv = NdV + sqrt(a2 + (1.0 - a2) * (NdV * NdV));
	return 1.0f / (gl * gv); // The division by (4.0 * NdL * NdV) is unneeded with this form
}

float D_GGX(float NdH, float alphaRoughness)
{
	float a2 = alphaRoughness * alphaRoughness;
	float f = (NdH * a2 - NdH) * NdH + 1.0;
	return a2 / (M_PI * f * f);
}
