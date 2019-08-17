#pragma once

#include <VertexBuffer.h>
#include <IndexBuffer.h>

#include <DirectXMath.h>
#include <d3d12.h>

#include <memory> 
#include <vector>

struct VertexData
{
	VertexData()
		: position(DirectX::XMFLOAT3(0))
		, normal(DirectX::XMFLOAT3(0))
		, textureCoordinate(DirectX::XMFLOAT2(0))
	{}

	VertexData(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT2& textureCoordinate)
		: position(position)
		, normal(normal)
		, textureCoordinate(textureCoordinate)
	{}

	VertexData(DirectX::FXMVECTOR position, DirectX::FXMVECTOR normal, DirectX::FXMVECTOR textureCoordinate)
	{
		XMStoreFloat3(&this->position, position);
		XMStoreFloat3(&this->normal, normal);
		XMStoreFloat2(&this->textureCoordinate, textureCoordinate);
	}

	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 textureCoordinate;

	static const int InputElementCount = 3;
	static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
};

using VertexCollection = std::vector<VertexData>;
using IndexCollection  = std::vector<uint16_t>;

class Mesh
{
public:
	void Render();

	//static std::unique_ptr<Mesh> CreateCube(CommandList& commandList, float size = 1, bool rhcoords = false);

protected:
private:
	friend struct std::default_delete<Mesh>;

	Mesh();
	Mesh(const Mesh& copy) = delete;
	virtual ~Mesh();

	//void Initialize(CommandList& commandList, VertexCollection& vertices, IndexCollection& indices, bool rhcoords);

	VertexBuffer m_VertexBuffer;
	IndexBuffer  m_IndexBuffer;

	UINT m_IndexCount;
};