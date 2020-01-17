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

struct ShadowPayload
{
	bool miss;
};

[shader("raygeneration")]
void ShadowPassRaygenShader()
{

}

[shader("miss")]
void ShadowPassMissShader(inout ShadowPayload payload)
{
	payload.miss = true;
}

#endif // RAYTRACING_HLSL