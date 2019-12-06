#include "Common.hlsli"

struct PixelShaderInput
{
	float4 PositionVS : POSITION;
	float3 NormalVS   : NORMAL;
	float2 TexCoord   : TEXCOORD;
};

float3 LinearToSRGB(float3 x)
{
	// This is exactly the sRGB curve
	//return x < 0.0031308 ? 12.92 * x : 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055;

	// This is cheaper but nearly equivalent
	return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt(abs(x - 0.00228)) - 0.13448 * x + 0.005719;
}

float DoDiffuse(float3 N, float3 L)
{
	return max(0, dot(N, L));
}

float DoSpecular(float3 V, float3 N, float3 L)
{
	float3 R = normalize(reflect(-L, N));
	float RdotV = max(0, dot(R, V));

	return pow(RdotV, MaterialCB.SpecularPower);
}

float DoAttenuation(float attenuation, float distance)
{
	return 1.0f / (1.0f + attenuation * distance * distance);
}

float DoSpotCone(float3 spotDir, float3 L, float spotAngle)
{
	float minCos = cos(spotAngle);
	float maxCos = (minCos + 1.0f) / 2.0f;
	float cosAngle = dot(spotDir, -L);
	return smoothstep(minCos, maxCos, cosAngle);
}

LightResult DoPointLight(PointLight light, float3 V, float3 P, float3 N)
{
	LightResult result;
	float3 L = (light.PositionVS.xyz - P);
	float d = length(L);
	L = L / d;

	float attenuation = DoAttenuation(light.Attenuation, d);

	result.Diffuse = DoDiffuse(N, L) * attenuation * light.Color * light.Intensity;
	result.Specular = DoSpecular(V, N, L) * attenuation * light.Color * light.Intensity;

	return result;
}

LightResult DoSpotLight(SpotLight light, float3 V, float3 P, float3 N)
{
	LightResult result;
	float3 L = (light.PositionVS.xyz - P);
	float d = length(L);
	L = L / d;

	float attenuation = DoAttenuation(light.Attenuation, d);

	float spotIntensity = DoSpotCone(light.DirectionVS.xyz, L, light.SpotAngle);

	result.Diffuse = DoDiffuse(N, L) * attenuation * spotIntensity * light.Color * light.Intensity;
	result.Specular = DoSpecular(V, N, L) * attenuation * spotIntensity * light.Color * light.Intensity;

	return result;
}

LightResult DoLighting(float3 P, float3 N)
{
	uint i;

	// Lighting is performed in view space.
	float3 V = normalize(-P);

	LightResult totalResult = (LightResult)0;

	for (i = 0; i < LightPropertiesCB.NumPointLights; ++i)
	{
		LightResult result = DoPointLight(PointLights[i], V, P, N);

		totalResult.Diffuse += result.Diffuse;
		totalResult.Specular += result.Specular;
	}

	for (i = 0; i < LightPropertiesCB.NumSpotLights; ++i)
	{
		LightResult result = DoSpotLight(SpotLights[i], V, P, N);

		totalResult.Diffuse += result.Diffuse;
		totalResult.Specular += result.Specular;
	}

	return totalResult;
}

float4 main(PixelShaderInput IN) : SV_Target
{
	LightResult lit = DoLighting(IN.PositionVS.xyz, normalize(IN.NormalVS));

	float4 emissive = MaterialCB.Emissive;
	float4 ambient = MaterialCB.Ambient;
	float4 diffuse = MaterialCB.Diffuse * lit.Diffuse;
	float4 specular = MaterialCB.Specular * lit.Specular;
	
	return (emissive + ambient + diffuse + specular);
}