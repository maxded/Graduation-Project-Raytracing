#pragma once

#include <CommandList.h>
#include <VertexBuffer.h>
#include <IndexBuffer.h>

#include <DirectXMath.h>
#include <d3d12.h>

#include <wrl.h>

#include <memory> // For std::unique_ptr
#include <vector>

struct VertexData
{
	VertexData()
		: position(DirectX::XMFLOAT3())
		, normal(DirectX::XMFLOAT3())
		, textureCoordinate(DirectX::XMFLOAT2())
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

class StaticMesh
{
public:
	void Render(CommandList& commandList, uint32_t instanceCount = 1, uint32_t firstInstance = 0);

	static std::unique_ptr<StaticMesh> CreateCube(CommandList& commandList, float size = 1, bool rhcoords = false);
	static std::unique_ptr<StaticMesh> CreateSphere(CommandList& commandList, float diameter = 1, size_t tessellation = 16, bool rhcoords = false);
	static std::unique_ptr<StaticMesh> CreateCone(CommandList& commandList, float diameter = 1, float height = 1, size_t tessellation = 32, bool rhcoords = false);
	static std::unique_ptr<StaticMesh> CreateTorus(CommandList& commandList, float diameter = 1, float thickness = 0.333f, size_t tessellation = 32, bool rhcoords = false);
	static std::unique_ptr<StaticMesh> CreatePlane(CommandList& commandList, float width = 1, float height = 1, bool rhcoords = false);

protected:

private:
	friend struct std::default_delete<StaticMesh>;

	StaticMesh();
	StaticMesh(const StaticMesh& copy) = delete;
	virtual ~StaticMesh();

	void Initialize(CommandList& commandList, VertexCollection& vertices, IndexCollection& indices, bool rhcoords);

	VertexBuffer m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

	UINT m_IndexCount;
};