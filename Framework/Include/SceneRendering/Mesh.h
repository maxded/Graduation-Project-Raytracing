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

	void SetWorldMatrix(const DirectX::XMFLOAT3& translation, const DirectX::XMVECTOR& quaternion, float scale);
	void SetWorldMatrix(const DirectX::XMMATRIX& worldMatrix);

	static const UINT VertexSlot	= 0;
	static const UINT NormalSlot	= 1;
	static const UINT TangentSlot	= 2;
	static const UINT Texcoord0Slot	= 3;

protected:
	void Load(const fx::gltf::Document& doc, std::size_t meshIndex, CommandList& commandList);

	void Render(CommandList& commandlist);

	void SetBaseTransform(DirectX::XMMATRIX baseTransform);
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
			: m_ModelMatrix{ DirectX::XMMatrixIdentity() }
			, m_ModelViewMatrix{ DirectX::XMMatrixIdentity() }
			, m_InverseTransposeModelViewMatrix{ DirectX::XMMatrixIdentity() }
			, m_ModelViewProjectionMatrix{ DirectX::XMMatrixIdentity() }
		{}

		DirectX::XMMATRIX m_ModelMatrix;
		DirectX::XMMATRIX m_ModelViewMatrix;
		DirectX::XMMATRIX m_InverseTransposeModelViewMatrix;
		DirectX::XMMATRIX m_ModelViewProjectionMatrix;
	};

	ConstantData m_Data;

	DirectX::XMMATRIX m_BaseTransform;
};