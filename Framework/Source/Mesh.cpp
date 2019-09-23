#include <FrameworkPCH.h>

#include <Application.h>
#include <Mesh.h>
#include <MeshData.h>
#include <ResourceStateTracker.h>



Mesh::Mesh()
{
	
}

Mesh::~Mesh()
{

}

void Mesh::Load(const fx::gltf::Document& doc, std::size_t meshIndex, CommandList& commandList)
{
	auto device = Application::Get().GetDevice();

	m_SubMeshes.resize(doc.meshes[meshIndex].primitives.size());

	for (std::size_t i = 0; i < doc.meshes[meshIndex].primitives.size(); i++)
	{
		MeshData mesh(doc, meshIndex, i);

		MeshData::BufferInfo const& vBuffer = mesh.VertexBuffer();
		MeshData::BufferInfo const& nBuffer = mesh.NormalBuffer();
		MeshData::BufferInfo const& tBuffer = mesh.TangentBuffer();
		MeshData::BufferInfo const& cBuffer = mesh.TexCoord0Buffer();
		MeshData::BufferInfo const& iBuffer = mesh.IndexBuffer();

		if (!vBuffer.HasData() || !nBuffer.HasData() || !iBuffer.HasData())
		{
			throw std::runtime_error("Only meshes with vertex, normal, and index buffers are supported");
		}

		SubMesh& submesh = m_SubMeshes[i];

		const uint32_t totalBufferSize =
			vBuffer.TotalSize +
			nBuffer.TotalSize +
			tBuffer.TotalSize +
			cBuffer.TotalSize +
			iBuffer.TotalSize;

		uint8_t* CPU = static_cast<uint8_t*>(malloc(static_cast<size_t>(totalBufferSize)));
		uint32_t offset = 0;

		if (!CPU)
			throw std::bad_alloc();

		std::vector<size_t> numElements;
		std::vector<size_t> elementSize;

		// Copy position data to upload buffer...
		std::memcpy(CPU, vBuffer.Data, vBuffer.TotalSize);
		numElements.push_back(vBuffer.Accessor->count);
		elementSize.push_back(vBuffer.DataStride);
		offset += vBuffer.TotalSize;
		
		// Copy normal data to upload buffer...
		std::memcpy(CPU + offset, nBuffer.Data, nBuffer.TotalSize);
		numElements.push_back(vBuffer.Accessor->count);
		elementSize.push_back(vBuffer.DataStride);
		offset += nBuffer.TotalSize;

		if (tBuffer.HasData())
		{
			// Copy tangent data to upload buffer...
			std::memcpy(CPU + offset, tBuffer.Data, tBuffer.TotalSize);
			numElements.push_back(vBuffer.Accessor->count);
			elementSize.push_back(vBuffer.DataStride);
			offset += tBuffer.TotalSize;
		}

		if (cBuffer.HasData())
		{
			// Copy texcoord data to upload buffer...
			std::memcpy(CPU + offset, cBuffer.Data, cBuffer.TotalSize);
			numElements.push_back(vBuffer.Accessor->count);
			elementSize.push_back(vBuffer.DataStride);
			offset += cBuffer.TotalSize;
		}
	
		// Copy index data to upload buffer...
		std::memcpy(CPU + offset, iBuffer.Data, iBuffer.TotalSize);
		// TODO: check index buffer format
		submesh.m_IndexCount = iBuffer.Accessor->count;
		DXGI_FORMAT IndexFormat = (iBuffer.DataStride) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

		commandList.CopyBuffer(submesh.m_VertexBuffer, totalBufferSize, CPU);
		commandList.CopyIndexBuffer(submesh.m_IndexBuffer, submesh.m_IndexCount, IndexFormat, CPU + offset);

		// Create views for all individual buffers
		submesh.m_VertexBuffer.CreateViews(numElements, elementSize);
		submesh.m_IndexBuffer.CreateViews(iBuffer.Accessor->count, iBuffer.DataStride);
	}
}

void Mesh::Render(CommandList& commandList)
{
	for (auto& submesh : m_SubMeshes)
	{
		commandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList.SetVertexBuffer(0, submesh.m_VertexBuffer);
		if (submesh.m_IndexCount > 0)
		{
			commandList.SetIndexBuffer(submesh.m_IndexBuffer);
			commandList.DrawIndexed(submesh.m_IndexCount, 1, 0, 0, 0);
		}
		else
		{
			commandList.Draw(submesh.m_VertexBuffer.GetNumVertices(), 1, 0, 0);
		}		
	}
}

