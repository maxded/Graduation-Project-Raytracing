//=============================================================================
// CPU data.
//=============================================================================

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

ConstantBuffer<MeshConstantData>		MeshCB				: register(b1);

//=============================================================================
// Vertex shader input/output.
//=============================================================================

struct VertexShaderInput
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

//=============================================================================
// Shader code.
//=============================================================================

VertexShaderOutput main(VertexShaderInput IN)
{
	VertexShaderOutput OUT;

	OUT.Position	= mul(MeshCB.ModelViewProjectionMatrix, float4(IN.Position, 1.0f));

	OUT.PositionW	= mul((float3x3)MeshCB.ModelMatrix, IN.Position);
	OUT.NormalW		= mul((float3x3)MeshCB.InverseTransposeModelMatrix, IN.Normal);
	OUT.TangentW	= mul((float3x3)MeshCB.ModelMatrix, IN.Tangent.xyz);
	OUT.BinormalW	= cross(OUT.NormalW, OUT.TangentW.xyz) * IN.Tangent.w;
	OUT.TexCoord	= IN.TexCoord;

	return OUT;
}