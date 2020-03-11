#pragma once

#include <DirectXMath.h>

struct PointLight
{
	PointLight()
		: PositionWS(0.0f, 0.0f, 0.0f, 1.0f)
		  , PositionVS(0.0f, 0.0f, 0.0f, 1.0f)
		  , Color(1.0f, 1.0f, 1.0f, 1.0f)
		  , Intensity(1.0f)
		  , Range(0.0f)
		  , Padding{0.0f, 0.0f}
	{}

	DirectX::XMFLOAT4 PositionWS; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 PositionVS; // Light position in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 Color;
	//----------------------------------- (16 byte boundary)
	float Intensity;
	float Range;
	float Padding[2]; // Pad to 16 bytes.
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 4 = 64 bytes
};

struct SpotLight
{
	SpotLight()
		: PositionWS(0.0f, 0.0f, 0.0f, 1.0f)
		  , PositionVS(0.0f, 0.0f, 0.0f, 1.0f)
		  , DirectionWS(0.0f, 0.0f, 1.0f, 0.0f)
		  , DirectionVS(0.0f, 0.0f, 1.0f, 0.0f)
		  , Color(1.0f, 1.0f, 1.0f, 1.0f)
		  , Intensity(1.0f)
		  , SpotAngle(DirectX::XM_PIDIV2)
		  , Attenuation(0.0f)
		  , Padding(0.0f)
	{}

	DirectX::XMFLOAT4 PositionWS; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 PositionVS; // Light position in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 DirectionWS; // Light direction in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 DirectionVS; // Light direction in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 Color;
	//----------------------------------- (16 byte boundary)
	float Intensity;
	float SpotAngle;
	float Attenuation;
	float Padding; // Pad to 16 bytes.
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 6 = 96 bytes
};

struct DirectionalLight
{
	DirectionalLight()
		: DirectionWS(0.0f, 0.0f, 1.0f, 0.0f)
		  , DirectionVS(0.0f, 0.0f, 1.0f, 0.0f)
		  , Color(1.0f, 1.0f, 1.0f, 1.0f)
	{}

	DirectX::XMFLOAT4 DirectionWS; // Light direction in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 DirectionVS; // Light direction in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 Color;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 3 = 48 bytes 
};

struct MeshConstantData
{
	MeshConstantData()
		: ModelMatrix{DirectX::XMMatrixIdentity()}
		  , ModelViewMatrix{DirectX::XMMatrixIdentity()}
		  , InverseTransposeModelMatrix{DirectX::XMMatrixIdentity()}
		  , ModelViewProjectionMatrix{DirectX::XMMatrixIdentity()}
	{}

	DirectX::XMMATRIX ModelMatrix;
	//----------------------------------- (64 byte boundary)
	DirectX::XMMATRIX ModelViewMatrix;
	//----------------------------------- (64 byte boundary)
	DirectX::XMMATRIX InverseTransposeModelMatrix;
	//----------------------------------- (64 byte boundary)
	DirectX::XMMATRIX ModelViewProjectionMatrix;
	//----------------------------------- (64 byte boundary)
	DirectX::XMVECTOR CameraPosition;
	//----------------------------------- (16 byte boundary)
	// Total:                              64 * 4 + 16 = 272 bytes
};

struct MeshMaterialData
{
	MeshMaterialData()
		: MeshAutoColor(0.0f, 0.0f, 0.0f, 0.0f)
		  , BaseColorFactor(0.0f, 0.0f, 0.0f, 0.0f)
		  , BaseColorIndex(0)
		  , NormalIndex(0)
		  , MetalRoughIndex(0)
		  , OcclusionIndex(0)
		  , EmissiveIndex(0)
		  , NormalScale(0.0f)
		  , RoughnessFactor(0.0f)
		  , MetallicFactor(0.0f)
		  , AoStrength(0.0f)
		  , EmissiveFactor(0.0f, 0.0f, 0.0f)
	{}

	DirectX::XMFLOAT4 MeshAutoColor;
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 BaseColorFactor;
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
	DirectX::XMFLOAT3 EmissiveFactor;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 5 = 80 bytes 
};
