#pragma once

#include <DirectXMath.h>

struct MeshInstance
{
	DirectX::XMMATRIX Transform;
	int32_t MeshIndex;
};
