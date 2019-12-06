#include "Common.hlsli"

struct VertexPositionNormalTexture
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
	float2 TexCoord : TEXCOORD;
};

struct VertexShaderOutput
{
	float4 PositionVS : POSITION;
	float3 NormalVS   : NORMAL;
	float2 TexCoord   : TEXCOORD;
	float4 Position   : SV_Position;
};

VertexShaderOutput main(VertexPositionNormalTexture IN)
{
	VertexShaderOutput OUT;

	OUT.Position	= mul(MeshCB.ModelViewProjectionMatrix, float4(IN.Position, 1.0f));
	OUT.PositionVS	= mul(MeshCB.ModelViewMatrix, float4(IN.Position, 1.0f));
	OUT.NormalVS	= mul((float3x3)MeshCB.InverseTransposeModelViewMatrix, IN.Normal);
	OUT.TexCoord	= IN.TexCoord;

	return OUT;
}