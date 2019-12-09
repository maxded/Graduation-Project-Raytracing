#include "Common.hlsli"

struct PixelShaderInput
{
	float4 PositionVS : POSITION;
	float3 NormalVS   : NORMAL;
	float2 TexCoord   : TEXCOORD;
};

float4 main(PixelShaderInput IN) : SV_Target
{
	LightResult lit = DoLighting(IN.PositionVS.xyz, normalize(IN.NormalVS));

	float4 emissive = MaterialCB.Emissive;
	float4 ambient = MaterialCB.Ambient;
	float4 diffuse = MaterialCB.Diffuse * lit.Diffuse;
	float4 specular = MaterialCB.Specular * lit.Specular;

#if USE_AUTO_COLOR
	return (emissive + ambient + diffuse + specular);
#endif

#if HAS_BASECOLORMAP	
	return (emissive + ambient + diffuse + specular);
#endif

#if HAS_METALROUGHNESSMAP
	return (emissive + ambient + diffuse + specular);
#endif

#if HAS_TANGENTS
	return (emissive + ambient + diffuse + specular);
#endif

#if HAS_NORMALMAP
	return (emissive + ambient + diffuse + specular);
#endif
	
#if HAS_OCCLUSIONMAP
	return (emissive + ambient + diffuse + specular);
#endif
#if HAS_OCCLUSIONMAP_COMBINED
	return (emissive + ambient + diffuse + specular);
#endif

#if HAS_EMISSIVEMAP
	return (emissive + ambient + diffuse + specular);
#endif

#if MANUAL_SRGB
	return (emissive + ambient + diffuse + specular);
#endif

	return (emissive + ambient + diffuse + specular);
}