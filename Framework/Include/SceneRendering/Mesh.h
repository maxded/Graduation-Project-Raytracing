#pragma once

#include <VertexBuffer.h>
#include <IndexBuffer.h>
#include <gltf.h>
#include <CommandList.h>
#include <DirectXMath.h>

class Mesh
{
	friend class Scenes;
public:
	Mesh();
	~Mesh();

	static std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements;

protected:
	void Load(const fx::gltf::Document& doc, std::size_t meshIndex, CommandList& commandList);

	void Render(CommandList& commandlist);

	void SetModelMatrix(DirectX::XMMATRIX modelMatrix);
private:
	std::string m_Name;

	struct SubMesh
	{	
		SubMesh()
			: m_IndexCount(0)
			, m_Topology(D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
		{}

		VertexBuffer m_VertexBuffer;
		IndexBuffer  m_IndexBuffer;

		UINT m_IndexCount;
		D3D_PRIMITIVE_TOPOLOGY m_Topology;
	};

	std::vector<SubMesh> m_SubMeshes;

	struct ConstantData
	{
		ConstantData()
			: m_ModelMatrix{}
			, m_ModelViewMatrix{}
			, m_InverseTransposeModelViewMatrix{}
			, m_ModelViewProjectionMatrix{}
		{}

		DirectX::XMMATRIX m_ModelMatrix;
		DirectX::XMMATRIX m_ModelViewMatrix;
		DirectX::XMMATRIX m_InverseTransposeModelViewMatrix;
		DirectX::XMMATRIX m_ModelViewProjectionMatrix;
	};

	ConstantData m_Data;
};