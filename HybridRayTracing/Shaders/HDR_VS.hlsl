struct Mat
{
	matrix ModelMatrix;
	matrix ModelViewMatrix;
	matrix InverseTransposeModelViewMatrix;
	matrix ModelViewProjectionMatrix;
};

ConstantBuffer<Mat> MatCB : register(b0, space0);

struct VS_INPUT
{
    float3 Position     : POSITION;
    float3 Normal		: NORMAL;
    float4 Tangent		: TANGENT;
    float2 TexCoord     : TEXCOORD;
};

struct VertexShaderOutput
{
	float4 PositionVS : POSITION;
	float3 NormalVS   : NORMAL;
	float2 TexCoord   : TEXCOORD;
	float4 Position   : SV_Position;
};

VertexShaderOutput main(VS_INPUT IN)
{
	VertexShaderOutput OUT;

	OUT.Position = mul(MatCB.ModelViewProjectionMatrix, float4(IN.Position, 1.0f));
	OUT.PositionVS = mul(MatCB.ModelViewMatrix, float4(IN.Position, 1.0f));
	OUT.NormalVS = mul((float3x3)MatCB.InverseTransposeModelViewMatrix, IN.Normal);
	OUT.TexCoord = IN.TexCoord;

	return OUT;
}