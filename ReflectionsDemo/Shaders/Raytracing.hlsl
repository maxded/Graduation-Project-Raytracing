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

SamplerState PointClampSampler									: register(s0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
	bool SkipShading;
	float RayHitT;
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

float2 GetUVAttribute(uint byteOffset)
{
	return asfloat(g_Attributes.Load2(byteOffset));
}

float3 RayPlaneIntersection(float3 planeOrigin, float3 planeNormal, float3 rayOrigin, float3 rayDirection)
{
	float t = dot(-planeNormal, rayOrigin - planeOrigin) / dot(planeNormal, rayDirection);
	return rayOrigin + rayDirection * t;
}

bool Inverse2x2(float2x2 mat, out float2x2 inverse)
{
	float determinant = mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1];

	float rcpDeterminant = rcp(determinant);
	inverse[0][0] = mat[1][1];
	inverse[1][1] = mat[0][0];
	inverse[1][0] = -mat[0][1];
	inverse[0][1] = -mat[1][0];
	inverse = rcpDeterminant * inverse;

	return abs(determinant) > 0.00000001;
}

void CalculateTrianglePartialDerivatives(float2 uv0, float2 uv1, float2 uv2, float3 p0, float3 p1, float3 p2, out float3 dpdu, out float3 dpdv)
{
	float2x2 linearEquation;
	linearEquation[0] = uv0 - uv2;
	linearEquation[1] = uv1 - uv2;

	float2x3 pointVector;
	pointVector[0] = p0 - p2;
	pointVector[1] = p1 - p2;
	float2x2 inverse;
	Inverse2x2(linearEquation, inverse);
	dpdu = pointVector[0] * inverse[0][0] + pointVector[1] * inverse[0][1];
	dpdv = pointVector[0] * inverse[1][0] + pointVector[1] * inverse[1][1];
}

void CalculateUVDerivatives(float3 normal, float3 dpdu, float3 dpdv, float3 p, float3 pX, float3 pY, out float2 ddX, out float2 ddY)
{
	int2 indices;
	float3 absNormal = abs(normal);
	if (absNormal.x > absNormal.y && absNormal.x > absNormal.z)
	{
		indices = int2(1, 2);
	}
	else if (absNormal.y > absNormal.z)
	{
		indices = int2(0, 2);
	}
	else
	{
		indices = int2(0, 1);
	}

	float2x2 linearEquation;
	linearEquation[0] = float2(dpdu[indices.x], dpdv[indices.x]);
	linearEquation[1] = float2(dpdu[indices.y], dpdv[indices.y]);

	float2x2 inverse;
	Inverse2x2(linearEquation, inverse);
	float2 pointOffset = float2(pX[indices.x] - p[indices.x], pX[indices.y] - p[indices.y]);
	ddX = abs(mul(inverse, pointOffset));

	pointOffset = float2(pY[indices.x] - p[indices.x], pY[indices.y] - p[indices.y]);
	ddY = abs(mul(inverse, pointOffset));
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

	// Test.

	float3 origin;
	float3 direction;

	GenerateCameraRay(launchindex, origin, direction);

	RayPayload testpayload;
	testpayload.SkipShading = false;
	testpayload.RayHitT = FLT_MAX;

	// Prepare a shadow ray.
	RayDesc testray;
	testray.Origin		= origin;
	testray.Direction	= direction;
	testray.TMin		= 0.01;
	testray.TMax		= FLT_MAX;

	// Lauch a shadow ray.
	TraceRay(g_Accel, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, testray, testpayload);

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

	// Lauch a shadow ray.
	TraceRay(g_Accel, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, ~0, 0, 0, 0, shadowray, shadowpayload);
	if (shadowpayload.RayHitT < FLT_MAX)
	{
		g_RenderTarget[launchindex].w = 0.0;
		return;
	}
	else
	{
		g_RenderTarget[launchindex].w = 1.0;
	}
		
	//======================= If not in shadows, do reflection ===============================\\

	//float3 viewdir			= normalize(world - g_SceneData.CameraPosition.xyz);
	//float3 reflectiondir	= reflect(viewdir, normal_sample.xyz);

	//// Initialize the reflection payload.
	//RayPayload reflectionpayload;
	//reflectionpayload.SkipShading = false;
	//reflectionpayload.RayHitT = FLT_MAX;

	//// Prepare a reflection ray.
	//RayDesc reflectionray;
	//reflectionray.Origin	= rayorigin;
	//reflectionray.Direction	= normalize(reflectiondir);
	//reflectionray.TMin		= 0.01;
	//reflectionray.TMax		= FLT_MAX;

	//// Lauch a ray.
	//TraceRay(g_Accel, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, reflectionray, reflectionpayload);
}


[shader("closesthit")]
void ClosesthitShader(inout RayPayload payload, in MyAttributes attr)
{
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
	const float2 uv0 = GetUVAttribute(info.UvAttributeOffset + ii.x * info.UvStride);
	const float2 uv1 = GetUVAttribute(info.UvAttributeOffset + ii.y * info.UvStride);
	const float2 uv2 = GetUVAttribute(info.UvAttributeOffset + ii.z * info.UvStride);
	float2 uv = bary.x * uv0 + bary.y * uv1 + bary.z * uv2;

	// Calculate normal.
	const float3 normal0 = asfloat(g_Attributes.Load3(info.NormalAttributeOffset + ii.x * info.NormalStride));
	const float3 normal1 = asfloat(g_Attributes.Load3(info.NormalAttributeOffset + ii.y * info.NormalStride));
	const float3 normal2 = asfloat(g_Attributes.Load3(info.NormalAttributeOffset + ii.z * info.NormalStride));
	float3 vsNormal = normalize(normal0 * bary.x + normal1 * bary.y + normal2 * bary.z);

	// Calculate tangent.
	const float4 tangent0 = asfloat(g_Attributes.Load4(info.TangentAttributeOffset + ii.x * info.TangentStride));
	const float4 tangent1 = asfloat(g_Attributes.Load4(info.TangentAttributeOffset + ii.y * info.TangentStride));
	const float4 tangent2 = asfloat(g_Attributes.Load4(info.TangentAttributeOffset + ii.z * info.TangentStride));
	float4 vsTangent = normalize(tangent0 * bary.x + tangent1 * bary.y + tangent2 * bary.z);

	// Calculate bitangent.
	float3 vsBitangent = cross(vsNormal, vsTangent.xyz) * vsTangent.w;

	// Get positions for uv partial derivatives.
	const float3 p0 = asfloat(g_Attributes.Load3(info.PositionAttributeOffset + ii.x * info.PositionStride));
	const float3 p1 = asfloat(g_Attributes.Load3(info.PositionAttributeOffset + ii.y * info.PositionStride));
	const float3 p2 = asfloat(g_Attributes.Load3(info.PositionAttributeOffset + ii.z * info.PositionStride));

	float3 worldPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

	uint2 threadID = DispatchRaysIndex().xy;
	float3 ddxOrigin, ddxDir, ddyOrigin, ddyDir;

	GenerateCameraRay(uint2(threadID.x + 1, threadID.y), ddxOrigin, ddxDir);
	GenerateCameraRay(uint2(threadID.x, threadID.y + 1), ddyOrigin, ddyDir);

	float3 triangleNormal = normalize(cross(p2 - p0, p1 - p0));
	float3 xOffsetPoint = RayPlaneIntersection(worldPosition, triangleNormal, ddxOrigin, ddxDir);
	float3 yOffsetPoint = RayPlaneIntersection(worldPosition, triangleNormal, ddyOrigin, ddyDir);

	float3 dpdu, dpdv;
	CalculateTrianglePartialDerivatives(uv0, uv1, uv2, p0, p1, p2, dpdu, dpdv);
	float2 ddx, ddy;
	CalculateUVDerivatives(triangleNormal, dpdu, dpdv, worldPosition, xOffsetPoint, yOffsetPoint, ddx, ddy);

	//============================== Material Sampling ===================================\\

	uint materialid = info.MaterialId;
	MaterialData material = g_Materials[materialid];

	float3 diffuse_sample = g_Textures[material.BaseColorIndex].SampleGrad(PointClampSampler, uv, ddx, ddy).rgb;

	float2 metal_rough_sample = g_Textures[material.MetalRoughIndex].SampleGrad(PointClampSampler, uv, ddx, ddy).gb;
	metal_rough_sample.x *= material.RoughnessFactor;
	metal_rough_sample.y *= material.MetallicFactor;

	float3x3 TBN = float3x3(vsTangent.xyz, vsBitangent, vsNormal);

	float3 normal_map_sample = g_Textures[material.NormalIndex].SampleGrad(PointClampSampler, uv, ddx, ddy).rgb;
	normal_map_sample.g = 1.0 - normal_map_sample.g;

	float3 normal = (2.0 * normal_map_sample.rgb - 1.0) * float3(material.NormalScale, material.NormalScale, 1.0);
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

		// Lauch a ray.	
		//TraceRay(g_Accel, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, ~0, 0, 0, 0, shadowray, shadowpayload);	
		if (shadowpayload.RayHitT < FLT_MAX)
		{
			shadow = 0.0;
		}
	}	

	g_RenderTarget[DispatchRaysIndex().xy].xyz = diffuse_sample;
}

[shader("miss")]
void MissShader(inout RayPayload payload)
{
	if (payload.SkipShading)
	{
		g_RenderTarget[DispatchRaysIndex().xy].w = 1.0;
	}
	// else lookup
}

