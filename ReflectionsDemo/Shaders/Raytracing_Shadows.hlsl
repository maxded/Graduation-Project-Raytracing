//=============================================================================
// CPU data.
//=============================================================================


struct SceneData
{
	float4x4 InverseView;
	float4x4 InverseProj;
	float4x4 InverseViewProj;
	float4 CameraPosition;
	float4 LightDirection;
};

struct PointLight
{
	float4 PositionWS; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	float4 PositionVS; // Light position in view space.
	//----------------------------------- (16 byte boundary)
	float4 Color;
	//----------------------------------- (16 byte boundary)
	float       Intensity;
	float       Range;
	float2      Padding;                // Pad to 16 bytes
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 4 = 64 bytes
};

struct SpotLight
{
	float4 PositionWS; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	float4 PositionVS; // Light position in view space.
	//----------------------------------- (16 byte boundary)
	float4 DirectionWS; // Light direction in world space.
	//----------------------------------- (16 byte boundary)
	float4 DirectionVS; // Light direction in view space.
	//----------------------------------- (16 byte boundary)
	float4 Color;
	//----------------------------------- (16 byte boundary)
	float       Intensity;
	float       SpotAngle;
	float       Attenuation;
	float       Padding;                // Pad to 16 bytes.
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 6 = 96 bytes
};

struct DirectionalLight
{
	float4 DirectionWS; // Light direction in world space.
	//----------------------------------- (16 byte boundary)
	float4 DirectionVS; // Light direction in view space.
	//----------------------------------- (16 byte boundary)
	float4 Color;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 3 = 48 bytes 
};

struct LightProperties
{
	uint NumPointLights;
	uint NumSpotLights;
	uint NumDirectionalLights;
};


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


//=============================================================================
// Functions.
//=============================================================================

const float origin() { return 1.0 / 32.0; }
const float float_scale() { return 1.0 / 65536.0; }
const float int_scale() { return 256.0; }

float Attenuation(float light_range, float d)
{
	return 1.0f - smoothstep(light_range * 0.75f, light_range, d);
}


float3 OffsetRay(const float3 p, const float3 n)
{
	int3 of_i = float3(int_scale() * n.x, int_scale() * n.y, int_scale() * n.z);

	float3 p_i; 

	p_i.x = asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x));
	p_i.y = asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y));
	p_i.z = asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z));

	float3 o;

	o.x = abs(p.x) < origin() ? p.x + float_scale() * n.x : p_i.x;
	o.y = abs(p.y) < origin() ? p.y + float_scale() * n.y : p_i.y;
	o.z = abs(p.z) < origin() ? p.z + float_scale() * n.z : p_i.z;

	return o;
}

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

	// Initialize the payload: assume that we have hit something.
	ShadowPayload payload;
	payload.miss = false;

	// Offset ray origin for floating point inprecision.
	// Nvidia's "Adaptive Offsetting Along The Geometric Normal" proposal -> Ray Tracing Gems pg.82 (book)
	float3 rayorigin	= OffsetRay(world, worldnormal);

	// Find closest light source.
	for (uint i = 0; i < LightPropertiesCB.NumPointLights; ++i)
	{
		float3 lightorigin = PointLights[i].PositionWS.xyz;

		float d = distance(rayorigin, lightorigin);
		float attenuation = Attenuation(PointLights[i].Range, d);

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

