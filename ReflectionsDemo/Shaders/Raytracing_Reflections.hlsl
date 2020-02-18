#include "Common.hlsli"

//=============================================================================
// Bindings.
//=============================================================================

ConstantBuffer<SceneData>	g_SceneData			: register(b0);

RWTexture2D<float4>			g_RenderTarget		: register(u0);

Texture2D					g_GBuffer[4]		: register(t0);

SamplerState				g_PointClampSampler	: register(s0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
	float4 color;
};

//=============================================================================
// Shader code.
//=============================================================================

[shader("raygeneration")]
void ReflectionPassRaygenShader()
{
	uint2 launchindex	= DispatchRaysIndex().xy;
	uint2 launchdim		= DispatchRaysDimensions().xy;
	float2 pixelpos		= (launchindex + 0.5) / launchdim;

	float4 albedo_sample		= g_GBuffer[0].SampleLevel(g_PointClampSampler, pixelpos, 0);
	float3 normal_sample		= g_GBuffer[1].SampleLevel(g_PointClampSampler, pixelpos, 0).rgb;
	float2 metal_rough_sample	= g_GBuffer[2].SampleLevel(g_PointClampSampler, pixelpos, 0).rg;
	float depth_sample			= g_GBuffer[3].SampleLevel(g_PointClampSampler, pixelpos, 0).r;

	// Compute position from depth buffer.
	float2 uvpos = pixelpos * 2.0 - 1.0;

	// Invert Y for DirectX-style coordinates.
	uvpos.y = -uvpos.y;

	float4 clipspaceposition  = float4(uvpos, depth_sample, 1.0);
	float4 worldspaceposition = mul(g_SceneData.InverseViewProj, clipspaceposition);

	// Perspective division.
	float3 world = worldspaceposition.xyz / worldspaceposition.w;
	float3 rayorigin = OffsetRay(world, normal_sample);

	// Calculate reflection ray direction.



	g_RenderTarget[launchindex] = float4(0.2, 0.5, 0.7, 1.0);
}

[shader("closesthit")]
void ReflectionPassClosesthitShader(inout RayPayload payload, in MyAttributes attr)
{

}

[shader("miss")]
void ReflectionPassMissShader(inout RayPayload payload)
{

}