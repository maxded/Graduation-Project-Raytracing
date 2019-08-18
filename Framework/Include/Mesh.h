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
		: position(DirectX::XMFLOAT3())
		, normal(DirectX::XMFLOAT3())
		, color(DirectX::XMFLOAT3())
		, textureCoordinate(DirectX::XMFLOAT2())
	{}

	VertexData(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT3& color, const DirectX::XMFLOAT2& textureCoordinate)
		: position(position)
		, normal(normal)
		, color(color)
		, textureCoordinate(textureCoordinate)
	{}

	VertexData(DirectX::FXMVECTOR position, DirectX::FXMVECTOR normal, DirectX::FXMVECTOR color, DirectX::FXMVECTOR textureCoordinate)
	{
		XMStoreFloat3(&this->position, position);
		XMStoreFloat3(&this->normal, normal);
		XMStoreFloat3(&this->color, color);
		XMStoreFloat2(&this->textureCoordinate, textureCoordinate);
	}

	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT3 color;
	DirectX::XMFLOAT2 textureCoordinate;

	static const int InputElementCount = 4;
	static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
};

using VertexCollection = std::vector<VertexData>;
using IndexCollection  = std::vector<uint16_t>;

class Mesh
{
public:
	void Render(ID3D12GraphicsCommandList2& commandList, uint32_t instanceCount = 1, uint32_t firstInstance = 0);

	static std::unique_ptr<Mesh> CreateCube(ID3D12GraphicsCommandList2& commandList, float size = 1, bool rhcoords = false);

protected:
private:
	friend struct std::default_delete<Mesh>;

	Mesh();
	Mesh(const Mesh& copy) = delete;
	virtual ~Mesh();

	void Initialize(ID3D12GraphicsCommandList2& commandList, VertexCollection& vertices, IndexCollection& indices, bool rhcoords);

	void CopyBuffer(ID3D12GraphicsCommandList2& commandList, Buffer& buffer, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	VertexBuffer m_VertexBuffer;
	IndexBuffer  m_IndexBuffer;

	UINT m_IndexCount;
};