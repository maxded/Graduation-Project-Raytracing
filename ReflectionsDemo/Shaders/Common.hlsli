struct MeshConstantData
{
	float4x4 ModelMatrix;
	//----------------------------------- (64 byte boundary)
	float4x4 ModelViewMatrix;
	//----------------------------------- (64 byte boundary)
	float4x4 InverseTransposeModelMatrix;
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
	uint NumDirectionalLights;
};

ConstantBuffer<MeshConstantData>		MeshCB				: register(b0);
ConstantBuffer<LightProperties>			LightPropertiesCB	: register(b1);

StructuredBuffer<MaterialData>			Materials			: register(t0);
StructuredBuffer<PointLight>			PointLights			: register(t1);
StructuredBuffer<SpotLight>				SpotLights			: register(t2);
StructuredBuffer<DirectionalLight>		DirectionalLights	: register(t3);

Texture2D Textures[69]				: register(t4);

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

//---------------------------------------------------------------------//

float DistributionGGX(float3 N, float3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float num = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = M_PI * denom * denom;

	return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float num = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return num / denom;
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}