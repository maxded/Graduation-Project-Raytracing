#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

struct SceneConstantBuffer
{
	float4x4 projectionToWorld;
	float4 cameraPosition;
};

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);

ConstantBuffer<SceneConstantBuffer> sceneCB : register(b0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
	float4 color;
};

inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
	float2 xy = index + 0.5f; // center in the middle of the pixel.
	float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

	// Invert Y for DirectX-style coordinates.
	screenPos.y = -screenPos.y;

	// Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), sceneCB.projectionToWorld);

	world.xyz /= world.w;
	origin = sceneCB.cameraPosition.xyz;
	direction = normalize(world.xyz - origin);
}


[shader("raygeneration")]
void MyRaygenShader()
{
	float3 rayDir;
	float3 origin;

	// Trace the ray.
    // Set the ray's extents.
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = rayDir;
	// Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
	// TMin should be kept small to prevent missing geometry at close contact areas.
	ray.TMin = 0.001;
	ray.TMax = 10000.0;
	RayPayload payload = { float4(0, 0, 0, 0) };
	TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

	// Write the raytraced color to the output texture.
	RenderTarget[DispatchRaysIndex().xy] = payload.color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	payload.color = float4(1.0, 0.0, 0.0, 1.0);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = float4(0.0, 0.2, 0.4, 1.0);
}

#endif // RAYTRACING_HLSL