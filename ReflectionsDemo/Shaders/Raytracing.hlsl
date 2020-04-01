#include "Common.hlsli"

struct MeshIndex
{
	int MeshId;
};

struct MeshInfo
{
	uint IndicesOffset;
	uint PositionAttributeOffset;
	uint NormalAttributeOffset;
	uint TangentAttributeOffset;
	//----------------------------------- (16 byte boundary)
	uint UvAttributeOffset;
	uint PositionStride;
	uint NormalStride;
	uint TangentStride;
	//----------------------------------- (16 byte boundary)
	uint UvStride;

	bool HasTangents;
	int MaterialId;
};

struct MaterialData
{
	float4 MeshAutoColor;
	//----------------------------------- (16 byte boundary)
	float4 BaseColorFactor;
	//----------------------------------- (16 byte boundary)
	int BaseColorIndex;
	int NormalIndex;
	int MetalRoughIndex;
	int OcclusionIndex;
	//----------------------------------- (16 byte boundary)
	int EmissiveIndex;
	float NormalScale;
	float RoughnessFactor;
	float MetallicFactor;
	//----------------------------------- (16 byte boundary)
	float AoStrength;
	float3 EmissiveFactor;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 5 = 80 bytes 
};


//=============================================================================
// Bindings.
//=============================================================================

RWTexture2D<float4> g_RenderTarget								: register(u0);

ConstantBuffer<SceneData>				g_SceneData				: register(b0);
ConstantBuffer<LightProperties>			g_LightProperties		: register(b1);
ConstantBuffer<MeshIndex>				g_MeshIndex				: register(b2);

StructuredBuffer<PointLight>			g_PointLights			: register(t0);
StructuredBuffer<SpotLight>				g_SpotLights			: register(t1);
StructuredBuffer<DirectionalLight>		g_DirectionalLights		: register(t2);
StructuredBuffer<MaterialData>			g_Materials				: register(t3);
StructuredBuffer<MeshInfo>				g_MeshInfo				: register(t5);

ByteAddressBuffer						g_Attributes			: register(t6);
ByteAddressBuffer						g_Indices				: register(t7);

RaytracingAccelerationStructure g_Accel							: register(t4);

Texture2D g_GBuffer[4]											: register(t8);
Texture2D g_Textures[69]										: register(t12);

SamplerState DefaultSampler										: register(s0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayCone
{
	float Width;
	float SpreadAngle;
};


struct RayPayload
{
	bool SkipShading;
	float RayHitT;
	int Bounce;
};


//=============================================================================
// Functions.
//=============================================================================

void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
	float2 xy = index + 0.5; // center in the middle of the pixel
	float2 screenpos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

	// Invert Y for DirectX-style coordinates
	screenpos.y = -screenpos.y;

	// Unproject into a ray
	float4 unprojected = mul(g_SceneData.InverseViewProj, float4(screenpos, 0, 1));
	float3 world = unprojected.xyz / unprojected.w;
	origin = g_SceneData.CameraPosition.xyz;
	direction = normalize(world - origin);
} 

uint3 Load3x16BitIndices(uint offsetBytes)
{
	const uint dwordAlignedOffset = offsetBytes & ~3;

	const uint2 four16BitIndices = g_Indices.Load2(dwordAlignedOffset);

	uint3 indices;

	if (dwordAlignedOffset == offsetBytes)
	{
		indices.x = four16BitIndices.x & 0xffff;
		indices.y = (four16BitIndices.x >> 16) & 0xffff;
		indices.z = four16BitIndices.y & 0xffff;
	}
	else
	{
		indices.x = (four16BitIndices.x >> 16) & 0xffff;
		indices.y = four16BitIndices.y & 0xffff;
		indices.z = (four16BitIndices.y >> 16) & 0xffff;
	}

	return indices;
}

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

	float4 albedo_sample		= g_GBuffer[0].SampleLevel(DefaultSampler, pixelpos, 0);
	float3 normal_sample		= g_GBuffer[1].SampleLevel(DefaultSampler, pixelpos, 0).rgb;
	float2 metal_rough_sample	= g_GBuffer[2].SampleLevel(DefaultSampler, pixelpos, 0).rg;
	float  depth_sample			= g_GBuffer[3].SampleLevel(DefaultSampler, pixelpos, 0).r;

	//============================== Compute ray origin ===================================\\

	float2 uvpos = pixelpos * 2.0 - 1.0;
	uvpos.y = -uvpos.y;

	float4 clipspaceposition	= float4(uvpos, depth_sample, 1.0);
	float4 worldspaceposition	= mul(g_SceneData.InverseViewProj, clipspaceposition);

	float3 world = worldspaceposition.xyz / worldspaceposition.w;
	float3 rayorigin = OffsetRay(world, normal_sample);

	//============================== RayCone test ===================================\\

	//GenerateCameraRay(launchindex, origin, direction);
	//
	//RayCone cone;
	//cone.Width = 0;
	//cone.SpreadAngle = atan((2.0 * tan(g_SceneData.VFOV / 2.0)) / g_SceneData.PixelHeight);

	//// Initialize the test payload.
	//RayPayload testpayload;
	//testpayload.SkipShading = false;
	//testpayload.RayHitT		= FLT_MAX;

	//// Prepare a test ray.
	//RayDesc testray;
	//testray.Origin		= origin;
	//testray.Direction	= direction;
	//testray.TMin		= 0.01;
	//testray.TMax		= FLT_MAX;

	//TraceRay(g_Accel, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, testray, testpayload);

	//======================= Check if point is in shadows ===================================\\

	// Initialize the shadow payload.
	RayPayload shadowpayload;
	shadowpayload.SkipShading = true;
	shadowpayload.RayHitT = FLT_MAX;

	// Prepare a shadow ray.
	RayDesc shadowray;
	shadowray.Origin	= rayorigin;
	shadowray.Direction	= normalize(g_DirectionalLights[0].DirectionWS.xyz);
	shadowray.TMin		= 0.01;
	shadowray.TMax		= FLT_MAX;

	g_RenderTarget[launchindex] = float4(0.0,0.0,0.0,1.0);

	// Lauch a shadow ray for primary visibility check.
	TraceRay(g_Accel, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, ~0, 0, 0, 0, shadowray, shadowpayload);
	if (shadowpayload.RayHitT < FLT_MAX)
	{
		g_RenderTarget[launchindex].w = 0.0;
	}
	
	//======================= If not in shadows, do reflection ===============================\\

	// Is reflection?
	if (!metal_rough_sample.r < 0.3)
	{
		g_RenderTarget[launchindex].rgb = 0.0;
		return;
	}
		
	float3 viewdir			= normalize(world - g_SceneData.CameraPosition.xyz);
	float3 reflectiondir	= reflect(viewdir, normal_sample.xyz);

	// Initialize the reflection payload.
	RayPayload reflectionpayload;
	reflectionpayload.SkipShading = false;
	reflectionpayload.RayHitT = FLT_MAX;
	reflectionpayload.Bounce = 0;

	// Prepare a reflection ray.
	RayDesc reflectionray;
	reflectionray.Origin	= rayorigin;
	reflectionray.Direction	= normalize(reflectiondir);
	reflectionray.TMin		= 0.01;
	reflectionray.TMax		= FLT_MAX;

	// Lauch a ray.
	TraceRay(g_Accel, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, reflectionray, reflectionpayload);
}


[shader("closesthit")]
void ClosesthitShader(inout RayPayload payload, in MyAttributes attr)
{
	payload.Bounce++;
	payload.RayHitT = RayTCurrent();
	if (payload.SkipShading)
	{
		return;
	}

	int meshid		= g_MeshIndex.MeshId;
	MeshInfo info	= g_MeshInfo[meshid];

	//============================== Calculate primitve data ===================================\\

	float3 bary = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

	// Calculate uv.
	const uint3 ii = Load3x16BitIndices(info.IndicesOffset + PrimitiveIndex() * 3 * 2);
	const float2 uv0 = asfloat(g_Attributes.Load2(info.UvAttributeOffset + ii.x * info.UvStride));
	const float2 uv1 = asfloat(g_Attributes.Load2(info.UvAttributeOffset + ii.y * info.UvStride));
	const float2 uv2 = asfloat(g_Attributes.Load2(info.UvAttributeOffset + ii.z * info.UvStride));
	float2 uv = bary.x * uv0 + bary.y * uv1 + bary.z * uv2;

	// Calculate normal.
	const float3 normal0 = asfloat(g_Attributes.Load3(info.NormalAttributeOffset + ii.x * info.NormalStride));
	const float3 normal1 = asfloat(g_Attributes.Load3(info.NormalAttributeOffset + ii.y * info.NormalStride));
	const float3 normal2 = asfloat(g_Attributes.Load3(info.NormalAttributeOffset + ii.z * info.NormalStride));

	// Blender export uses +z as up.
	const float3 correctnormal0 = float3(normal0.x, -normal0.z, normal0.y);
	const float3 correctnormal1 = float3(normal1.x, -normal1.z, normal1.y);
	const float3 correctnormal2 = float3(normal2.x, -normal2.z, normal2.y);

	float3 wsNormal = normalize(correctnormal0 * bary.x + correctnormal1 * bary.y + correctnormal2 * bary.z);

	// Calculate tangent.
	const float4 tangent0 = asfloat(g_Attributes.Load4(info.TangentAttributeOffset + ii.x * info.TangentStride));
	const float4 tangent1 = asfloat(g_Attributes.Load4(info.TangentAttributeOffset + ii.y * info.TangentStride));
	const float4 tangent2 = asfloat(g_Attributes.Load4(info.TangentAttributeOffset + ii.z * info.TangentStride));

	const float4 correcttangent0 = float4(tangent0.x, -tangent0.z, tangent0.y, tangent0.w);
	const float4 correcttangent1 = float4(tangent1.x, -tangent1.z, tangent1.y, tangent1.w);
	const float4 correcttangent2 = float4(tangent2.x, -tangent2.z, tangent2.y, tangent2.w);

	float4 wsTangent = normalize(correcttangent0 * bary.x + correcttangent1 * bary.y + correcttangent2 * bary.z);

	// Calculate bitangent.
	float3 wsBitangent = normalize(cross(wsNormal, wsTangent.xyz)) * wsTangent.w;

	float3 worldPosition  = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

	//============================== Material Sampling ===================================\\

	uint materialid = info.MaterialId;
	MaterialData material = g_Materials[materialid];

	float3 diffuse_sample = g_Textures[material.BaseColorIndex].SampleLevel(DefaultSampler, uv, 0).rgb;

	float2 metal_rough_sample = g_Textures[material.MetalRoughIndex].SampleLevel(DefaultSampler, uv, 0).gb;
	metal_rough_sample.x *= material.RoughnessFactor;
	metal_rough_sample.y *= material.MetallicFactor;

	float3x3 TBN = float3x3(wsTangent.xyz, wsBitangent, wsNormal);

	float3 normal_map_sample = g_Textures[material.NormalIndex].SampleLevel(DefaultSampler, uv, 0).rgb;
	normal_map_sample.g = 1.0 - normal_map_sample.g;

	float3 normal = (2.0 * normal_map_sample.rgb - 1.0) * material.NormalScale;
	float3 world_normal = normalize(mul(normal, TBN));

	//============================== Lighting ===================================\\

	float shadow = 1.0;
	{
		// Initialize the payload.
		RayPayload shadowpayload;
		shadowpayload.SkipShading = true;
		shadowpayload.RayHitT = FLT_MAX;

		float3 rayorigin = OffsetRay(worldPosition, world_normal);

		// Prepare a shadow ray.
		RayDesc shadowray;
		shadowray.Origin	= rayorigin;
		shadowray.Direction = normalize(g_DirectionalLights[0].DirectionWS.xyz);
		shadowray.TMin		= 0.01;
		shadowray.TMax		= FLT_MAX;

		// Lauch a shadow ray.	
		TraceRay(g_Accel, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, ~0, 0, 0, 0, shadowray, shadowpayload);	
		if (shadowpayload.RayHitT < FLT_MAX)
		{
			shadow = 0.0;
		}
	}	

	if (!metal_rough_sample.x < 0.1)
	{
		float3 N = world_normal;
		float3 V = normalize(-WorldRayDirection());
		float3 L = normalize(g_DirectionalLights[0].DirectionWS.xyz);
		float3 H = normalize(V + L);

		float3 radiance = g_DirectionalLights[0].Color.rgb;

		float perceptual_roughness = clamp(metal_rough_sample.x, 0.04, 1.0);
		float metallic = clamp(metal_rough_sample.y, 0.0, 1.0);

		float3 f0 = 0.04;
		float3 specular_color = lerp(f0, diffuse_sample, metallic);

		// Cook-Torrence BRDF.
		float NDF = DistributionGGX(N, H, perceptual_roughness);
		float G = GeometrySmith(N, V, L, perceptual_roughness);
		float3 F = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), specular_color);

		float3 kS = F;
		float3 kD = float3(1.0, 1.0, 1.0) - kS;
		kD *= 1.0 - metallic;

		float3 numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
		float3 specular = numerator / max(denominator, 0.001);

		// add to outgoing radiance Lo.
		float NdotL = max(dot(N, L), 0.0);
		float3 Lo = (1.0 * diffuse_sample / M_PI + specular) * radiance * NdotL;

		float3 ambient = float3(0.03, 0.03, 0.03) * diffuse_sample;
		float3 outcolor = ambient + (Lo * shadow);

		g_RenderTarget[DispatchRaysIndex().xy].xyz = outcolor;
	}

	// Is reflection? Recursion.
	if (metal_rough_sample.x < 0.1 && payload.Bounce < g_SceneData.RayBounces)
	{
		float3 rayorigin = OffsetRay(worldPosition, world_normal);
		float3 reflectiondir = reflect(WorldRayDirection(), world_normal);

		// Prepare a reflection ray.
		RayDesc reflectionray;
		reflectionray.Origin	= rayorigin;
		reflectionray.Direction = normalize(reflectiondir);
		reflectionray.TMin = 0.01;
		reflectionray.TMax = FLT_MAX;

		TraceRay(g_Accel, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, reflectionray, payload);
	}
}

[shader("miss")]
void MissShader(inout RayPayload payload)
{
	if (payload.SkipShading)
	{
		//g_RenderTarget[DispatchRaysIndex().xy].w = 1.0;
	}
	else
	{
		g_RenderTarget[DispatchRaysIndex().xy].xyz = 0.0;
	}
}

