//=============================================================================
// Global variables.
//=============================================================================

#define FLT_MAX 3.402823466e+38

static const float M_PI = 3.141592653589793f;

static const float M_RMIN = 0.01; // 1 cm.
static const float M_RMAX = 5.0;  // 10 m.
static const float M_INF = 9999.9;

static const float origin = 1.0 / 32.0;
static const float float_scale = 1.0 / 65536.0;
static const float int_scale = 256.0;


//=============================================================================
// Data structures.
//=============================================================================

struct SceneData
{
	float4x4 InverseViewProj;
	float4x4 ViewProj;
	float4 CameraPosition;
	float VFOV;
	float PixelHeight;
	int RayBounces;
	float Padding;
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
// Global functions.
//=============================================================================


// Inverse-square equation.
float InverseSquare(float r)
{
	return 1.0 / (pow(max(r, M_RMIN), 2.0));
}

// Windowing functions used by Unreal Engine & Frostbite Engine.
float Windowing(float r)
{
	float result = 1.0 - (pow(r / M_RMAX, 4));

	return pow(clamp(result, 0.0, M_INF), 2.0);
}

// Offset ray origin for floating point inprecision.
// Nvidia's "Adaptive Offsetting Along The Geometric Normal" proposal -> Ray Tracing Gems pg.82 (book)
float3 OffsetRay(const float3 p, const float3 n)
{
	int3 of_i = float3(int_scale * n.x, int_scale * n.y, int_scale * n.z);

	float3 p_i;

	p_i.x = asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x));
	p_i.y = asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y));
	p_i.z = asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z));

	float3 o;

	o.x = abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x;
	o.y = abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y;
	o.z = abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z;

	return o;
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float num = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = M_PI * denom * denom;

	return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float num = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}


