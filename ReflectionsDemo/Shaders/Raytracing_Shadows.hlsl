#include "Common.hlsli"

//=============================================================================
// Bindings.
//=============================================================================


ConstantBuffer<SceneData>			SceneCB					: register(b0);
ConstantBuffer<LightProperties>		LightPropertiesCB		: register(b1);

StructuredBuffer<PointLight>			PointLights			: register(t0);
StructuredBuffer<SpotLight>				SpotLights			: register(t1);
StructuredBuffer<DirectionalLight>		DirectionalLights	: register(t2);

RaytracingAccelerationStructure Scene						: register(t3);

Texture2D Textures[2]										: register(t4);

RWTexture2D<float> RenderTarget								: register(u0);

SamplerState PointClampSampler								: register(s0);


struct ShadowPayload
{
	bool miss;
};


//=============================================================================
// Shader code.
//=============================================================================

[shader("raygeneration")]
void ShadowPassRaygenShader()
{
	uint2 launchindex	= DispatchRaysIndex().xy;
	uint2 launchdim		= DispatchRaysDimensions().xy;
	float2 pixelpos		= (launchindex + 0.5) / launchdim; 

	const float depth		 = Textures[0].SampleLevel(PointClampSampler, pixelpos, 0).r;
	const float3 worldnormal = Textures[1].SampleLevel(PointClampSampler, pixelpos, 0).rgb;

	// Skip sky rays.
	if (depth == 0.0)
	{
		RenderTarget[launchindex] = 0.0;
		return;
	}

	// Compute position from depth buffer.
	float2 uvpos = pixelpos * 2.0 - 1.0;

	// Invert Y for DirectX-style coordinates.
	uvpos.y = -uvpos.y;

	float4 clipspaceposition	= float4(uvpos, depth, 1.0);
	float4 worldspaceposition	= mul(SceneCB.InverseViewProj, clipspaceposition);

	// Perspective division.
	float3 world = worldspaceposition.xyz / worldspaceposition.w;
	float3 rayorigin = OffsetRay(world, worldnormal);


	// Initialize the payload: assume that we have hit something.
	ShadowPayload payload;
	payload.miss = false;

	// Find closest light source.
	for (uint i = 0; i < LightPropertiesCB.NumPointLights; ++i)
	{
		float3 lightorigin = PointLights[i].PositionWS.xyz;

		float d = distance(rayorigin, lightorigin);
		float attenuation = InverseSquare(d) * Windowing(d);

		if (attenuation < 0.01)
			continue;

		// Prepare a shadow ray.
		RayDesc ray;
		ray.Origin		= rayorigin;
		ray.Direction	= normalize(lightorigin - rayorigin);
		ray.TMin		= 0.001;
		ray.TMax		= distance(rayorigin, lightorigin);

		// Lauch a ray.
		TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 0, 0, ray, payload);

		// Payload.miss means that the ray hit no object so it's not in shadow.
		if (payload.miss)
			break;
	}

	// Directional Light
	if (!payload.miss)
	{
		for (uint i = 0; i < LightPropertiesCB.NumDirectionalLights; ++i)
		{
			// Prepare a shadow ray.
			RayDesc ray;
			ray.Origin		= rayorigin;
			ray.Direction	= normalize(DirectionalLights[i].DirectionWS.xyz);
			ray.TMin		= 0.001;
			ray.TMax		= 1000.0;

			// Lauch a ray.
			TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 0, 0, ray, payload);

			if (payload.miss)
				break;
		}	
	}

	RenderTarget[launchindex] = payload.miss ? 1.0 : 0.0;
}

[shader("miss")]
void ShadowPassMissShader(inout ShadowPayload payload)
{
	payload.miss = true;
}

