//=============================================================================
// CPU data.
//=============================================================================


struct MaterialData
{
	float4 MeshAutoColor;
	//----------------------------------- (16 byte boundary)
	float4 BaseColorFactor;
	//----------------------------------- (16 byte boundary)
	float NormalScale;
	float RoughnessFactor;
	float MetallicFactor;
	float AoStrength;
	//----------------------------------- (16 byte boundary)
	float3 EmissiveFactor;
	float  Padding;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 4 = 64 bytes 
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
	float4 CameraPosition;
	//----------------------------------- (16 byte boundary)
	// Total:                              64 * 4 + 16 = 272 bytes
};


//=============================================================================
// Bindings.
//=============================================================================


ConstantBuffer<MaterialData>			Material			: register(b0);
ConstantBuffer<MeshConstantData>		MeshCB				: register(b1);

Texture2D Textures[5]										: register(t0);

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

#if USE_AUTO_COLOR
	// TODO: add auto color functionality.
#else

#if HAS_BASECOLORMAP
	float4 base_color = Textures[0].Sample(LinearRepeatSampler, IN.TexCoord) * Material.BaseColorFactor;
#else
	float4 base_color = Material.BaseColorFactor;
#endif

#if HAS_METALROUGHNESSMAP
	float4 metal_rough_sample		= Textures[2].Sample(LinearRepeatSampler, IN.TexCoord);

	float perceptual_roughness		= metal_rough_sample.g * Material.RoughnessFactor;
	float metallic					= metal_rough_sample.b * Material.MetallicFactor;
#else
	float perceptual_roughness		= Material.RoughnessFactor;
	float metallic					= Material.MetallicFactor;
#endif

#if HAS_TANGENTS
	float3x3 TBN = float3x3(normalize(IN.TangentW), normalize(IN.BinormalW), normalize(IN.NormalW));
#else
	// WIP
	float3 pos_dx = ddx(IN.PositionW);
	float3 pos_dy = ddy(IN.PositionW);
	float2 tex_dx = ddx(IN.TexCoord);
	float2 tex_dy = ddy(IN.TexCoord);

	float3 t = tex_dy.y * pos_dx - tex_dx.y * pos_dy;
	float3 b = tex_dx.x * pos_dy - tex_dy.x * pos_dx;
	float3 n = normalize(IN.NormalW);

	float3x3 TBN = float3x3(t, b, n);
#endif

#if HAS_NORMALMAP
	float4 normal_map_sample = Textures[1].Sample(LinearRepeatSampler, IN.TexCoord);
	normal_map_sample.g = 1.0 - normal_map_sample.g;

	float3 normal = (2.0 * normal_map_sample.rgb - 1.0) * float3(Material.NormalScale, Material.NormalScale, 1.0);
	float3 N = normalize(mul(normal, TBN));
#else
	float3 N = TBN[2].xyz;
#endif	

#if HAS_OCCLUSIONMAP
#if HAS_OCCLUSIONMAP_COMBINED
	float ao = metalRoughSample.r;
#else
	float ao = Textures[3].Sample(LinearRepeatSampler, IN.TexCoord);
#endif
	color = lerp(color, color * ao, Material.AoStrength);
#else
	float ao = 0.0;
#endif

#if HAS_EMISSIVEMAP
	float3 emissive = Textures[4].Sample(LinearRepeatSampler, IN.TexCoord).rgb * Material.EmissiveFactor;
#else
	float3 emissive = Material.EmissiveFactor;	
#endif
	output.AlbedoBuffer				= base_color;
	output.NormalBuffer				= float4(N, 1.0);
	output.MetalRoughBuffer			= float2(metallic, perceptual_roughness);
	output.EmissiveOcclusionBuffer	= float4(emissive, ao);
#endif

	return output;
}