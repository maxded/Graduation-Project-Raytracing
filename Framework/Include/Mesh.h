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
	//Mesh(const Mesh& copy) = delete;
	virtual ~Mesh();

	static std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements;

protected:
	void Load(const fx::gltf::Document& doc, std::size_t meshIndex, CommandList& commandList);

	void Render(CommandList& commandlist);
private:
	struct SubMesh
	{	
		VertexBuffer m_VertexBuffer;
		IndexBuffer  m_IndexBuffer;

		UINT m_IndexCount;
	};

	std::vector<SubMesh> m_SubMeshes;
};