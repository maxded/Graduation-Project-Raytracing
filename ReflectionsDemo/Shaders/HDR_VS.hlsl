#include "Common.hlsli"

struct VertexPositionNormalTexture
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
	float4 Tangent	: TANGENT;
	float2 TexCoord : TEXCOORD;
};

struct VertexShaderOutput
{
	float4 Position		: SV_Position;
	float3 PositionW	: POSITION;
	float3 NormalW		: NORMAL;
	float3 TangentW		: TANGENT;
	float3 BinormalW	: BINORMAL;
	float2 TexCoord		: TEXCOORD;
};

VertexShaderOutput main(VertexPositionNormalTexture IN)
{
	VertexShaderOutput OUT;

	OUT.Position	= mul(float4(IN.Position, 1.0f), MeshCB.ModelViewProjectionMatrix);

	OUT.PositionW	= mul(IN.Position, (float3x3)MeshCB.ModelMatrix);
	OUT.NormalW		= mul(IN.Normal, (float3x3)MeshCB.ModelMatrix);
	OUT.TangentW	= mul(IN.Tangent.xyz, (float3x3)MeshCB.ModelMatrix);
	OUT.BinormalW = cross(OUT.NormalW, OUT.TangentW) *1.0;
	OUT.TexCoord	= IN.TexCoord;

	return OUT;
}