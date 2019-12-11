#pragma once

#include <DirectXMath.h>

struct PointLight
{
	PointLight()
		: PositionWS(0.0f, 0.0f, 0.0f, 1.0f)
		  , PositionVS(0.0f, 0.0f, 0.0f, 1.0f)
		  , Color(1.0f, 1.0f, 1.0f, 1.0f)
		  , Intensity(1.0f)
		  , Attenuation(0.0f)
		  , Padding{0.0f, 0.0f}
	{}

	DirectX::XMFLOAT4 PositionWS; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 PositionVS; // Light position in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 Color;
	//----------------------------------- (16 byte boundary)
	float Intensity;
	float Attenuation;
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
		  , InverseTransposeModelViewMatrix{DirectX::XMMatrixIdentity()}
		  , ModelViewProjectionMatrix{DirectX::XMMatrixIdentity()}
		  , MaterialIndex(0)
		  , Padding{}
	{}

	DirectX::XMMATRIX ModelMatrix;
	//----------------------------------- (64 byte boundary)
	DirectX::XMMATRIX ModelViewMatrix;
	//----------------------------------- (64 byte boundary)
	DirectX::XMMATRIX InverseTransposeModelViewMatrix;
	//----------------------------------- (64 byte boundary)
	DirectX::XMMATRIX ModelViewProjectionMatrix;
	//----------------------------------- (64 byte boundary)
	int MaterialIndex;
	float Padding[3]; // Pad to 16 bytes. 
	//----------------------------------- (16 byte boundary)
	DirectX::XMVECTOR CameraPosition;
	//----------------------------------- (16 byte boundary)
	// Total:                              64 * 3 + 32 = 224 bytes
};

struct MeshMaterialData
{
	MeshMaterialData()
		: MeshAutoColor(0.0f, 0.0f, 0.0f, 0.0f)
		  , BaseColorFactor(0.0f, 0.0f, 0.0f, 0.0f)
		  , BaseColorIndex(0)
		  , NormalIndex(0)
		  , NormalScale(0.0f)
		  , MetalRoughIndex(0)
		  , RoughnessFactor(0.0f)
		  , MetallicFactor(0.0f)
		  , AoIndex(0)
		  , AoStrength(0.0f)
		  , EmissiveIndex(0)
		  , EmissiveFactor(0.0f, 0.0f, 0.0f)
	{}

	DirectX::XMFLOAT4 MeshAutoColor;
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 BaseColorFactor;
	//----------------------------------- (16 byte boundary)
	int BaseColorIndex;
	int NormalIndex;
	float NormalScale;
	int MetalRoughIndex;
	//----------------------------------- (16 byte boundary)
	float RoughnessFactor;
	float MetallicFactor;
	int AoIndex;
	float AoStrength;
	//----------------------------------- (16 byte boundary)
	int EmissiveIndex;
	DirectX::XMFLOAT3 EmissiveFactor;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 5 = 80 bytes 
};
