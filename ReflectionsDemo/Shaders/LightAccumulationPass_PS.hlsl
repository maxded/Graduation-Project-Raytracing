#include "Common.hlsli"

//=============================================================================
// CPU data.
//=============================================================================

struct PointLight
{
	float4 PositionWS; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	float4 PositionVS; // Light position in view space.
	//----------------------------------- (16 byte boundary)
	float4 Color;
	//----------------------------------- (16 byte boundary)
	float       Intensity;
	float       Range;
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

struct SceneData
{
	float4x4 InverseViewProj;
	float4 CameraPosition;
};


//=============================================================================
// Functions.
//=============================================================================


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


//=============================================================================
// Bindings.
//=============================================================================

ConstantBuffer<SceneData>				SceneDataCB			: register(b0);
ConstantBuffer<LightProperties>			LightPropertiesCB	: register(b1);

StructuredBuffer<PointLight>			PointLights			: register(t0);
StructuredBuffer<SpotLight>				SpotLights			: register(t1);
StructuredBuffer<DirectionalLight>		DirectionalLights	: register(t2);

Texture2D GBuffer[6]										: register(t3);

SamplerState PointClampSampler								: register(s0);


//=============================================================================
// Shader code.
//=============================================================================

float4 main(float2 TexCoord : TEXCOORD) : SV_Target0
{
	// Sample G-Buffer.
	float4 albedo_sample			 = GBuffer[0].SampleLevel(PointClampSampler, TexCoord, 0);	
	float4 normal_sample			 = GBuffer[1].SampleLevel(PointClampSampler, TexCoord, 0);
	float2 metal_rough_sample		 = GBuffer[2].SampleLevel(PointClampSampler, TexCoord, 0).rg;
	float4 emissive_occlusion_sample = GBuffer[3].SampleLevel(PointClampSampler, TexCoord, 0);
	float  depth_sample				 = GBuffer[4].SampleLevel(PointClampSampler, TexCoord, 0).r;
	float  shadow_sample			 = GBuffer[5].SampleLevel(PointClampSampler, TexCoord, 0).r;

	float perceptual_roughness = clamp(metal_rough_sample.g, 0.04, 1.0);
	float metallic = clamp(metal_rough_sample.r, 0.0, 1.0);

	float alpha_roughness = perceptual_roughness * perceptual_roughness;

	float3 f0 = 0.04;
	float3 specular_color = lerp(f0, albedo_sample.rgb, metallic);

	// compute world space position.
	float2 uvpos = TexCoord * 2.0 - 1.0;
	uvpos.y = -uvpos.y;

	float4 clip_space_position	= float4(uvpos, depth_sample, 1.0);
	float4 world_space_position = mul(SceneDataCB.InverseViewProj, clip_space_position);

	world_space_position.xyz /= world_space_position.w;

	float3 V = normalize(SceneDataCB.CameraPosition.xyz - world_space_position.xyz);	
	float3 N = normal_sample.xyz;

	// Do Pointlights.
	float3 Lo = float3(0.0, 0.0, 0.0);
	for (int i = 0; i < LightPropertiesCB.NumPointLights; i++)
	{
		float3 L	= normalize(PointLights[i].PositionWS.rgb - world_space_position.xyz);
		float3 H	= normalize(V + L);										

		float distance		= length(PointLights[i].PositionWS.rgb - world_space_position.xyz);
		float attenuation	= InverseSquare(distance) * Windowing(distance);
		float3 radiance		= PointLights[i].Color.rgb * attenuation;

		// Cook-Torrence BRDF
		float NDF	= DistributionGGX(N, H, perceptual_roughness);
		float G		= GeometrySmith(N, V, L, perceptual_roughness);
		float3 F	= fresnelSchlick(max(dot(H, V), 0.0), specular_color);

		float3 kS = F;
		float3 kD = float3(1.0, 1.0, 1.0) - kS;
		kD *= 1.0 - metallic;

		float3 numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
		float3 specular = numerator / max(denominator, 0.001);

		// add to outgoing radiance Lo
		float NdotL = max(dot(N, L), 0.0);
		Lo += (kD * albedo_sample.rgb / M_PI + specular) * radiance * NdotL;
	}

	// Do DirectionalLights
	for (uint i = 0; i < LightPropertiesCB.NumDirectionalLights; ++i)
	{
		float3 L = normalize(DirectionalLights[i].DirectionWS.rgb);		// Vector from surface point to light
		float3 H = normalize(V + L);									// Half vector between both l and v

		float3 radiance = DirectionalLights[i].Color.rgb;

		// Cook-Torrence BRDF
		float NDF	= DistributionGGX(N, H, perceptual_roughness);
		float G		= GeometrySmith(N, V, L, perceptual_roughness);
		float3 F	= fresnelSchlick(max(dot(H, V), 0.0), specular_color);

		float3 kS = F;
		float3 kD = float3(1.0, 1.0, 1.0) - kS;
		kD *= 1.0 - metallic;

		float3 numerator	= NDF * G * F;
		float denominator	= 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
		float3 specular		= numerator / max(denominator, 0.001);

		// add to outgoing radiance Lo
		float NdotL = max(dot(N, L), 0.0);
		Lo += (kD * albedo_sample.rgb / M_PI + specular) * radiance * NdotL;
	}

	float3 ambient = float3(0.03, 0.03, 0.03) * albedo_sample.rgb;
	float3 color = ambient + Lo * shadow_sample;

	//color = lerp(color, color * ao, Material.AoStrength);
	color += emissive_occlusion_sample.xyz;

	return float4(color, albedo_sample.a);
}