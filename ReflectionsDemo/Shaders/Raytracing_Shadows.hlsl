#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

struct SceneConstantBuffer
{
	float4x4 InverseViewProj;
	float4 CameraPosition;
	float4 LightDirection;
};

ConstantBuffer<SceneConstantBuffer> SceneCB		: register(b0);

RaytracingAccelerationStructure Scene			: register(t0);
Texture2D DepthMap								: register(t1);

RWTexture2D<float4> RenderTarget				: register(u0);

SamplerState LinearClampSampler					: register(s0);

struct ShadowPayload
{
	bool miss;
};

[shader("raygeneration")]
void ShadowPassRaygenShader()
{
	uint2 launchindex	= DispatchRaysIndex().xy;
	uint2 launchdim		= DispatchRaysDimensions().xy;
	float2 pixelpos		= launchindex / launchdim; 

	pixelpos.y = -pixelpos.y;

	const float depth = DepthMap.SampleLevel(LinearClampSampler, pixelpos, 0).r;

	// Skip sky rays.
	if (depth == 0.0)
	{
		RenderTarget[launchindex] = 0.0;
		return;
	}

	// Compute position from depth buffer.
	float2 uvpos = (launchindex + 0.5f) / launchdim * 2.0 - 1.0;

	// Invert Y for DirectX-style coordinates.
	uvpos.y = -uvpos.y;

	float4 clipspaceposition	= float4(uvpos, depth * 2.0 - 1.0, 1.0);
	float4 worldspaceposition	= mul(SceneCB.InverseViewProj, clipspaceposition);

	// Perspective division.
	float3 position = worldspaceposition.xyz /= worldspaceposition.w;

	// Initialize the payload: assume that we have hit something.
	ShadowPayload payload;
	payload.miss = false;

	// Prepare a shadow ray.
	RayDesc ray;
	ray.Origin		= position;
	ray.Direction	= -SceneCB.LightDirection.xyz;
	ray.TMin		= 0.001;
	ray.TMax		= 10000.0;

	// Lauch a ray.
	TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 0, ray, payload);

	float shadow = payload.miss ? 1.0 : 0.0;

	RenderTarget[launchindex] = float4(depth, depth, depth, 1.0);
}

[shader("miss")]
void ShadowPassMissShader(inout ShadowPayload payload)
{
	payload.miss = true;
}

#endif // RAYTRACING_HLSL