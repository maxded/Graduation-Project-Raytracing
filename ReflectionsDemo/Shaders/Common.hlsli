//=============================================================================
// Global variables.
//=============================================================================


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
	float4 CameraPosition;
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

