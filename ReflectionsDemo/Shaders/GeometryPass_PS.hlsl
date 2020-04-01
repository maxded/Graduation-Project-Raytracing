//=============================================================================
// CPU data.
//=============================================================================

struct MaterialConstantBuffer
{
	int MaterialIndex;
	float3 Padding;
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

struct MeshConstantData
{
	float4x4 ModelMatrix;
	//----------------------------------- (64 byte boundary)
	float4x4 ModelViewMatrix;
	//----------------------------------- (64 byte boundary)
	float4x4 InverseTransposeModelMatrix;
	//----------------------------------- (64 byte boundary)
	float4x4 ModelViewProjectionMatrix;
	//----------------------------------- (64 byte boundary)
	// Total:                              64 * 4 + 16 = 256 bytes
};


//=============================================================================
// Bindings.
//=============================================================================

ConstantBuffer<MaterialConstantBuffer>  MaterialCB			: register(b0);
ConstantBuffer<MeshConstantData>		MeshCB				: register(b1);

StructuredBuffer<MaterialData> Materials					: register(t0);

Texture2D Textures[69]										: register(t1);

SamplerState LinearRepeatSampler							: register(s0);


//=============================================================================
// Pixel shader input/output.
//=============================================================================


struct PixelShaderInput
{
	float4 Position		: SV_Position;
	float3 PositionW	: POSITION;
	float3 NormalW		: NORMAL;
	float3 TangentW		: TANGENT;
	float3 BinormalW	: BINORMAL;
	float2 TexCoord		: TEXCOORD;
};

struct PixelShaderOutput
{
	float4 AlbedoBuffer				: SV_Target0;
	float4 NormalBuffer				: SV_Target1;
	float2 MetalRoughBuffer			: SV_Target2;
	float4 EmissiveOcclusionBuffer	: SV_Target3;
};


//=============================================================================
// Shader code. (GBuffer fill)
//=============================================================================


PixelShaderOutput main(PixelShaderInput IN) 
{
	PixelShaderOutput output;

	MaterialData material = Materials[MaterialCB.MaterialIndex];

	float4 base_color;
	float2 metal_rough;
	float3 n;
	float3 emissive;
	float ao;

	if (material.BaseColorIndex >= 0)
	{
		base_color = Textures[material.BaseColorIndex].Sample(LinearRepeatSampler, IN.TexCoord);
	}
	else 
	{
		base_color = material.BaseColorFactor;
	}

	if (material.MetalRoughIndex >= 0)
	{
		metal_rough = Textures[material.MetalRoughIndex].Sample(LinearRepeatSampler, IN.TexCoord).gb;

		metal_rough.r *= material.RoughnessFactor;
		metal_rough.g *= material.MetallicFactor;
	}
	else
	{
		metal_rough.r = material.RoughnessFactor;
		metal_rough.g = material.MetallicFactor;
	}

//#if HAS_TANGENTS
	float3x3 TBN = float3x3(normalize(IN.TangentW), normalize(IN.BinormalW), normalize(IN.NormalW));
//#else
	// WIP
	//float3 pos_dx = ddx(IN.PositionW);
	//float3 pos_dy = ddy(IN.PositionW);
	//float2 tex_dx = ddx(IN.TexCoord);
	//float2 tex_dy = ddy(IN.TexCoord);

	//float3 t = tex_dy.y * pos_dx - tex_dx.y * pos_dy;
	//float3 b = tex_dx.x * pos_dy - tex_dy.x * pos_dx;
	//float3 n = normalize(IN.NormalW);

	//float3x3 TBN = float3x3(t, b, n);
//#endif

	if (material.NormalIndex >= 0)
	{
		float4 normal_map_sample = Textures[material.NormalIndex].Sample(LinearRepeatSampler, IN.TexCoord);
		normal_map_sample.g = 1.0 - normal_map_sample.g;

		float3 normal = (2.0 * normal_map_sample.rgb - 1.0) * material.NormalScale;
		n = normalize(mul(normal, TBN));
	}
	else 
	{
		n = TBN[2].xyz;
	}

	if (material.OcclusionIndex >= 0)
	{
		if (material.OcclusionIndex == material.MetalRoughIndex)
		{
			ao = Textures[material.MetalRoughIndex].Sample(LinearRepeatSampler, IN.TexCoord).r;
		}
		else
		{
			//ao = Textures[material.OcclusionIndex].Sample(LinearRepeatSampler, IN.TexCoord).r;
		}
	}
	else
	{
		ao = 0.0;
	}
	
	if (material.EmissiveIndex >= 0)
	{
		emissive = Textures[material.EmissiveIndex].Sample(LinearRepeatSampler, IN.TexCoord).rgb * material.EmissiveFactor;
	}
	else
	{
		emissive = material.EmissiveFactor;
	}

	output.AlbedoBuffer				= base_color;
	output.NormalBuffer				= float4(n, 1.0);
	output.MetalRoughBuffer			= metal_rough;
	output.EmissiveOcclusionBuffer	= float4(emissive, ao);

	return output;
}