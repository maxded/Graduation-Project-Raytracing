struct MeshConstantBuffer
{
	matrix ModelMatrix;
	//----------------------------------- (64 byte boundary)
	matrix ModelViewMatrix;
	//----------------------------------- (64 byte boundary)
	matrix InverseTransposeModelViewMatrix;
	//----------------------------------- (64 byte boundary)
	matrix ModelViewProjectionMatrix;
	//----------------------------------- (64 byte boundary)

	int MaterialIndex;
	float3 Padding;
	//----------------------------------- (16 byte boundary)
	// Total:                              64 * 3 + 16 = 208 bytes
};

struct Material
{
	float4 Emissive;
	//----------------------------------- (16 byte boundary)
	float4 Ambient;
	//----------------------------------- (16 byte boundary)
	float4 Diffuse;
	//----------------------------------- (16 byte boundary)
	float4 Specular;
	//----------------------------------- (16 byte boundary)
	float  SpecularPower;
	float3 Padding;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 5 = 80 bytes
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
	float       Attenuation;
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

struct LightProperties
{
	uint NumPointLights;
	uint NumSpotLights;
};

struct LightResult
{
	float4 Diffuse;
	float4 Specular;
};

ConstantBuffer<MeshConstantBuffer>	MeshCB : register(b0);
ConstantBuffer<Material>			MaterialCB : register(b0, space1);
ConstantBuffer<LightProperties>		LightPropertiesCB : register(b1);

StructuredBuffer<PointLight> PointLights : register(t0);
StructuredBuffer<SpotLight> SpotLights : register(t1);