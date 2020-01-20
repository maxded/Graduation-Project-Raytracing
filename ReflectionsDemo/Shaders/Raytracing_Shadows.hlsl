#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

struct SceneConstantBuffer
{
	float4x4 InverseViewProj;
	float4 cameraPosition;
};

ConstantBuffer<SceneConstantBuffer> sceneCB		: register(b0);

RaytracingAccelerationStructure Scene			: register(t0);
Texture2D DepthMap								: register(t1);

RWTexture2D<uint1> RenderTarget					: register(u0);

SamplerState LinearClampSampler					: register(s0);

struct ShadowPayload
{
	bool miss;
};

[shader("raygeneration")]
void ShadowPassRaygenShader()
{
	uint2 launchindex	= DispatchRaysIndex().xy;
	uint2 launchdim		= DispatchRaysDimensions();
	float2 pixelpos		= (lauchindex + 0.5f) / lauchdim * 2.0f - 1.0f;

	const float depth = DepthMap.SampleLevel(LinearClampSampler, pixelpos, 0);

	// Skip sky rays.
	if (depth == 0.0)
	{
		RenderTarget[launchindex] = 0.0;
		return;
	}

	// Compute position from depth buffer.

}

[shader("miss")]
void ShadowPassMissShader(inout ShadowPayload payload)
{
	payload.miss = true;
}

#endif // RAYTRACING_HLSL