#include "Common.hlsli"

//=============================================================================
// Bindings.
//=============================================================================

ConstantBuffer<SceneData>				g_SceneDataCB			: register(b0);
ConstantBuffer<LightProperties>			g_LightPropertiesCB		: register(b1);

StructuredBuffer<PointLight>			g_PointLights			: register(t0);
StructuredBuffer<SpotLight>				g_SpotLights			: register(t1);
StructuredBuffer<DirectionalLight>		g_DirectionalLights		: register(t2);

Texture2D g_GBuffer[6]											: register(t3);

SamplerState PointClampSampler									: register(s0);

//=============================================================================
// Shader code.
//=============================================================================

float4 main(float2 TexCoord : TEXCOORD) : SV_Target0
{
	//========================= Sample Geometry Buffer ===================================\\

	float4 albedo_sample			 = g_GBuffer[0].SampleLevel(PointClampSampler, TexCoord, 0);	
	float4 normal_sample			 = g_GBuffer[1].SampleLevel(PointClampSampler, TexCoord, 0);
	float2 metal_rough_sample		 = g_GBuffer[2].SampleLevel(PointClampSampler, TexCoord, 0).rg;
	float4 emissive_occlusion_sample = g_GBuffer[3].SampleLevel(PointClampSampler, TexCoord, 0);
	float  depth_sample				 = g_GBuffer[4].SampleLevel(PointClampSampler, TexCoord, 0).r;
	float4 raytraced_sample			 = g_GBuffer[5].SampleLevel(PointClampSampler, TexCoord, 0);

	// Hack.
	if (depth_sample == 1.0)
		return float4(0.4, 0.6, 0.9, 1.0);

	float perceptual_roughness = clamp(metal_rough_sample.r, 0.0, 1.0);
	float metallic = metal_rough_sample.g;

	float3 f0 = 0.04;
	float3 specular_color = lerp(f0, albedo_sample.rgb, metallic);

	//=========================== Compute world position ===================================\\

	float2 uvpos = TexCoord * 2.0 - 1.0;
	uvpos.y = -uvpos.y;

	float4 clip_space_position	= float4(uvpos, depth_sample, 1.0);
	float4 world_space_position = mul(g_SceneDataCB.InverseViewProj, clip_space_position);

	world_space_position.xyz /= world_space_position.w;

	//=================================== Lighting ========================================\\

	float3 V = normalize(g_SceneDataCB.CameraPosition.xyz - world_space_position.xyz );
	float3 N = normal_sample.xyz;
	float3 L = normalize(g_DirectionalLights[0].DirectionWS.rgb);	// Vector from surface point to light
	float3 H = normalize(V + L);									// Half vector between both l and v

	float3 radiance = g_DirectionalLights[0].Color.rgb;

	// Cook-Torrence BRDF
	float NDF	= DistributionGGX(N, H, perceptual_roughness);
	float G		= GeometrySmith(N, V, L, perceptual_roughness);
	float3 F	= fresnelSchlick(max(dot(H, V), 0.0), specular_color);

	float3 kS = F;
	float3 kD = float3(1.0, 1.0, 1.0) - kS;
	kD *= 1.0 - metallic;

	float3 nominator	= NDF * G * F;
	float denominator	= 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
	float3 specular		= nominator / denominator;

	// add to outgoing radiance Lo
	float NdotL = max(dot(N, L),0.0);
	float3 Lo = (kD * albedo_sample.rgb / M_PI + specular) * radiance * NdotL;

	float3 ambient = float3(0.03, 0.03, 0.03) * albedo_sample.rgb;
	float3 color = ambient + Lo * raytraced_sample.a;

	color += raytraced_sample.rgb;
	//color = lerp(color, color * ao, Material.AoStrength);
	color += emissive_occlusion_sample.rgb;

	return float4(color, albedo_sample.a);
}