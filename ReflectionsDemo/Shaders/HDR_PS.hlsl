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

	IN.NormalW = normalize(IN.NormalW);

	float4 final_color;

#if USE_AUTO_COLOR
	final_color = material.MeshAutoColor;
	return final_color;
#else

#if HAS_BASECOLORMAP
	float4 base_color = SRGBtoLINEAR(Textures[material.BaseColorIndex].SampleLevel(LinearRepeatSampler, IN.TexCoord, 0)) * material.BaseColorFactor;
#else
	float4 base_color = material.BaseColorFactor;
#endif

#if HAS_METALROUGHNESSMAP
	float4 metal_rough_sample		= Textures[material.MetalRoughIndex].SampleLevel(LinearRepeatSampler, IN.TexCoord, 0);
	float perceptual_roughness		= metal_rough_sample.g * material.RoughnessFactor;
	float metallic					= metal_rough_sample.b * material.MetallicFactor;
#else
	float perceptual_roughness		= material.RoughnessFactor;
	float metallic					= material.MetallicFactor;
#endif

	perceptual_roughness	= clamp(perceptual_roughness, 0.04, 1.0);
	metallic				= clamp(metallic, 0.0, 1.0);

	float alpha_roughness = perceptual_roughness * perceptual_roughness;

	float3 f0				= 0.04;
	float3 diffuse_color	= base_color.rgb * (1.0 - f0) * (1.0 - metallic);
	float3 specular_color	= lerp(f0, base_color.rgb, metallic);

	float reflectance = max(max(specular_color.r, specular_color.g), specular_color.b);

	// For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
	// For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
	float reflectance_90			= clamp(reflectance * 25.0, 0.0, 1.0);
	float3 specular_environment_R0	= specular_color.rgb;
	float3 specular_environment_R90 = reflectance_90;

#if HAS_TANGENTS
	float3x3 TBN = float3x3(normalize(IN.TangentW), normalize(IN.BinormalW), IN.NormalW);
#else
	float3 pos_dx = ddx(IN.PositionW);
	float3 pos_dy = ddy(IN.PositionW);
	float3 tex_dx = ddx(float3(IN.TexCoord, 0.0));
	float3 tex_dy = ddy(float3(IN.TexCoord, 0.0));
	float3 t = (tex_dy.y * pos_dx - tex_dx.y * pos_dy) / (tex_dx.x * tex_dy.y - tex_dy.x * tex_dx.y);

	float3 n = IN.NormalW;
	t = normalize(t - n * dot(n, t));
	float3 b = normalize(cross(n, t));

	float3x3 TBN = float3x3(t, b, n);
#endif

#if HAS_NORMALMAP
	float4 normal_map_sample = Textures[material.NormalIndex].Sample(LinearRepeatSampler, IN.TexCoord);
	normal_map_sample.g = 1.0f - normal_map_sample.g;

	float3 normal = (2.0f * normal_map_sample.rgb - 1.0f) * float3(material.NormalScale, material.NormalScale, 1.0f);
	float3 N = normalize(mul(normal, TBN));
#else
	float3 N = TBN[2].xyz;
#endif
	
	float3 V = normalize(MeshCB.CameraPosition.xyz - IN.PositionW); // Vector from surface point to camera
	float3 L = normalize(DirectionalLights[0].DirectionWS);			// Vector from surface point to light
	float3 H = normalize(L + V);									// Half vector between both l and v
	float3 reflection = -normalize(reflect(V, N));

	float NdL = clamp(dot(N, L), 0.001, 1.0);
	float NdV = clamp(abs(dot(N, V)), 0.001, 1.0);
	float NdH = clamp(dot(N, H), 0.0, 1.0);
	float LdH = clamp(dot(L, H), 0.0, 1.0);

	// Calculate the shading terms for the microfacet specular shading model
	float3 F	= F_Schlick(specular_environment_R0, specular_environment_R90, LdH);
	float G		= G_Smith(NdL, NdV, alpha_roughness);
	float D		= D_GGX(NdH, alpha_roughness);

	// Calculation of analytical lighting contribution
	float3 diffuse_contrib = (1.0 - F) * Diffuse_Lambert(diffuse_color);
	float3 spec_contrib = F * (G * D);
	float3 color = NdL * (diffuse_contrib + spec_contrib);

#if HAS_OCCLUSIONMAP
#endif
#if HAS_OCCLUSIONMAP_COMBINED
	
#endif

#if HAS_EMISSIVEMAP
#endif
	final_color = float4(LinearToSRGB(color), base_color.a);
#endif

	return final_color;
}