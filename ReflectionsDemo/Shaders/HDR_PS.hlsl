#include "Common.hlsli"

struct PixelShaderInput
{
	float4 Position		: SV_Position;
	float3 PositionW	: POSITION;
	float3 NormalW		: NORMAL;
	float3 TangentW		: TANGENT;
	float3 BinormalW	: BINORMAL;
	float2 TexCoord		: TEXCOORD;
};

float4 main(PixelShaderInput IN) : SV_Target
{
	MaterialData material = Materials[MeshCB.MaterialIndex];
	IN.NormalW	= normalize(IN.NormalW);

	float4 final_color;

#if USE_AUTO_COLOR
	final_color = material.MeshAutoColor;
	return final_color;
#else

#if HAS_BASECOLORMAP
	float4 base_color = Textures[material.BaseColorIndex].Sample(LinearRepeatSampler, IN.TexCoord) * material.BaseColorFactor;
#else
	float4 base_color = material.BaseColorFactor;
#endif

#if HAS_METALROUGHNESSMAP
	float4 metal_rough_sample		= Textures[material.MetalRoughIndex].Sample(LinearRepeatSampler, IN.TexCoord);
	float perceptual_roughness		= metal_rough_sample.g * material.RoughnessFactor;
	float metallic					= metal_rough_sample.b * material.MetallicFactor;
#else
	float perceptual_roughness		= material.RoughnessFactor;
	float metallic					= material.MetallicFactor;
#endif

	perceptual_roughness	= clamp(perceptual_roughness, 0.04, 1.0);
	metallic				= clamp(metallic, 0.0, 1.0);

	float alpha_roughness	= perceptual_roughness * perceptual_roughness;

	float3 f0				= 0.04;
	float3 specular_color	= lerp(f0, base_color.rgb, metallic);

#if HAS_TANGENTS
	float3x3 TBN = float3x3(normalize(IN.TangentW), normalize(IN.BinormalW), IN.NormalW);
#else
	// WIP
	float3 pos_dx = ddx(IN.PositionW);
	float3 pos_dy = ddy(IN.PositionW);
	float2 tex_dx = ddx(IN.TexCoord);
	float2 tex_dy = ddy(IN.TexCoord);

	float3 t = tex_dy.y * pos_dx - tex_dx.y * pos_dy;
	float3 b = tex_dx.x * pos_dy - tex_dy.x * pos_dx;
	float3 n = IN.NormalW;

	float3x3 TBN = float3x3(t, b, n);
#endif

#if HAS_NORMALMAP
	float4 normal_map_sample = Textures[material.NormalIndex].Sample(LinearRepeatSampler, IN.TexCoord);
	normal_map_sample.g = 1.0 - normal_map_sample.g;

	float3 normal = (2.0 * normal_map_sample.rgb - 1.0) * float3(material.NormalScale, material.NormalScale, 1.0);
	float3 N = normalize(mul(normal, TBN));
#else
	float3 N = TBN[2].xyz;
#endif	

	float3 V = normalize(MeshCB.CameraPosition.xyz - IN.PositionW);		// Vector from surface point to camera

	// Do Pointlights
	float3 Lo = float3(0.0, 0.0, 0.0);
	for (int i = 0; i < LightPropertiesCB.NumPointLights; i++)
	{
		float3 L = normalize(PointLights[i].PositionWS.rgb - IN.PositionW); // Vector from surface point to light
		float3 H = normalize(V + L);										// Half vector between both l and v

		float distance		= length(PointLights[i].PositionWS.rgb - IN.PositionW);
		float attenuation	= 1.0 / (distance * distance);
		float3 radiance		= PointLights[i].Color.rgb * attenuation;

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
		Lo += (kD * base_color.rgb / M_PI + specular) * radiance * NdotL;
	}

	// Do DirectionalLights
	for (uint i = 0; i < LightPropertiesCB.NumDirectionalLights; ++i)
	{
		float3 L = normalize(DirectionalLights[i].DirectionWS.rgb);	// Vector from surface point to light
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
		Lo += (kD * base_color.rgb / M_PI + specular) * radiance * NdotL;
	}

	float3 ambient = float3(0.01, 0.01, 0.01) * base_color.rgb;
	float3 color   = ambient + Lo;

#if HAS_OCCLUSIONMAP
#if HAS_OCCLUSIONMAP_COMBINED
	float ao = metalRoughSample.r;
#else
	float ao = Textures[material.AoIndex].Sample(LinearRepeatSampler, IN.TexCoord);
#endif
	color = lerp(color, color * ao, material.AoStrength);
#endif

#if HAS_EMISSIVEMAP
	float3 emissive = Textures[material.EmissiveIndex].Sample(LinearRepeatSampler, IN.TexCoord).rgb * material.EmissiveFactor;
	color += emissive;
#else
	color += material.EmissiveFactor;
#endif

	final_color = float4(color, base_color.a);
#endif
	return final_color;
}