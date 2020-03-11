#include "Common.hlsli"

//=============================================================================
// Bindings.
//=============================================================================


ConstantBuffer<SceneData>			g_SceneData				: register(b0);
ConstantBuffer<LightProperties>		g_LightProperties		: register(b1);

StructuredBuffer<PointLight>			g_PointLights		: register(t0);
StructuredBuffer<SpotLight>				g_SpotLights		: register(t1);
StructuredBuffer<DirectionalLight>		g_DirectionalLights	: register(t2);

RaytracingAccelerationStructure g_Scene						: register(t3);

Texture2D g_GBuffer[4]										: register(t4);

RWTexture2D<float4> g_RenderTarget							: register(u0);

SamplerState PointClampSampler								: register(s0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
	float3 color;
	bool miss;
};


//=============================================================================
// Shader code.
//=============================================================================

[shader("raygeneration")]
void RaygenShader()
{
	uint2 launchindex	= DispatchRaysIndex().xy;
	uint2 launchdim		= DispatchRaysDimensions().xy;
	float2 pixelpos		= (launchindex + 0.5) / launchdim; 

	//========================= Sample Geometry Buffer ===================================\\

	float4 albedo_sample		= g_GBuffer[0].SampleLevel(PointClampSampler, pixelpos, 0);
	float3 normal_sample		= g_GBuffer[1].SampleLevel(PointClampSampler, pixelpos, 0).rgb;
	float2 metal_rough_sample	= g_GBuffer[2].SampleLevel(PointClampSampler, pixelpos, 0).rg;
	float  depth_sample			= g_GBuffer[3].SampleLevel(PointClampSampler, pixelpos, 0).r;

	//============================== Compute ray origin ===================================\\

	float2 uvpos = pixelpos * 2.0 - 1.0;
	uvpos.y = -uvpos.y;

	float4 clipspaceposition	= float4(uvpos, depth_sample, 1.0);
	float4 worldspaceposition	= mul(g_SceneData.InverseViewProj, clipspaceposition);

	float3 world = worldspaceposition.xyz / worldspaceposition.w;
	float3 rayorigin = OffsetRay(world, normal_sample);

	// Initialize the payload: assume that we have hit something.
	RayPayload payload;
	payload.miss = false;

	//======================= Check if point is in shadows ===================================\\

	RayDesc ray;

	for (uint i = 0; i < g_LightProperties.NumDirectionalLights; ++i)
	{
		// Prepare a shadow ray.
		ray.Origin		= rayorigin;
		ray.Direction	= normalize(g_DirectionalLights[i].DirectionWS.xyz);
		ray.TMin		= 0.001;
		ray.TMax		= 1000.0;

		// Lauch a ray.
		TraceRay(g_Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 0, 0, ray, payload);

		if (payload.miss)
			break;			
	}

	//======================= If not in shadows, do reflection ===============================\\

	if (payload.miss)
	{
		float3 viewdir			= normalize(world - g_SceneData.CameraPosition.xyz);
		float3 reflectiondir	= reflect(viewdir, normal_sample.xyz);

		// Prepare a reflection ray.
		ray.Origin		= rayorigin;
		ray.Direction	= normalize(reflectiondir);
		ray.TMin = 0.001;
		ray.TMax = 1000.0;

		// Lauch a ray.
		//TraceRay(g_Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 0, 0, ray, payload);
	}

	float4 output = float4(0.0, 0.0, 0.0, 0.0);
	output.w = payload.miss ? 1.0 : 0.0;

	g_RenderTarget[launchindex] = output;
}

[shader("closesthit")]
void ClosesthitShader(inout RayPayload payload, in MyAttributes attr)
{
	
}

[shader("miss")]
void MissShader(inout RayPayload payload)
{
	payload.miss = true;
}

