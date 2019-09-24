#pragma once

#include <Model.h>
#include <VertexBuffer.h>
#include <IndexBuffer.h>

class UploadBuffer;

class Mesh
{
	friend class Model;
public:
	Mesh();
	~Mesh();

	static std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements;

protected:

	//Mesh(const Mesh& copy) = delete;

	void Load(const fx::gltf::Document& doc, std::size_t meshIndex, CommandList& commandList);

	void Render(CommandList& commandlist);
private:
	struct SubMesh
	{	
		VertexBuffer m_VertexBuffer;
		IndexBuffer  m_IndexBuffer;

		UINT m_IndexCount = 0;

		D3D_PRIMITIVE_TOPOLOGY m_Topology;
	};

	std::vector<SubMesh> m_SubMeshes;
};