#include <FrameworkPCH.h>

#include <Application.h>
#include <Mesh.h>
#include <MeshData.h>
#include <ResourceStateTracker.h>



Mesh::Mesh()
	: m_SubMeshes{}
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

		const MeshData::BufferInfo& vBuffer = mesh.VertexBuffer();
		const MeshData::BufferInfo& nBuffer = mesh.NormalBuffer();
		const MeshData::BufferInfo& tBuffer = mesh.TangentBuffer();
		const MeshData::BufferInfo& cBuffer = mesh.TexCoord0Buffer();
		const MeshData::BufferInfo& iBuffer = mesh.IndexBuffer();

		if (!vBuffer.HasData() || !nBuffer.HasData() || !iBuffer.HasData())
		{
			throw std::runtime_error("Only meshes with vertex, normal, and index buffers are supported");
		}

		const fx::gltf::Primitive& primitive = doc.meshes[meshIndex].primitives[i];

		SubMesh& submesh = m_SubMeshes[i];

		// Get submesh primitive topology for rendering
		switch (primitive.mode)
		{
		case fx::gltf::Primitive::Mode::Points:
			submesh.m_Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
			break;
		case fx::gltf::Primitive::Mode::Lines:
			submesh.m_Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
			break;
		case fx::gltf::Primitive::Mode::LineLoop:
			submesh.m_Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST;
			break;
		case fx::gltf::Primitive::Mode::LineStrip:
			submesh.m_Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
			break;
		case fx::gltf::Primitive::Mode::Triangles:
			submesh.m_Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
		case fx::gltf::Primitive::Mode::TriangleStrip:
			submesh.m_Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			break;
		case fx::gltf::Primitive::Mode::TriangleFan:
			submesh.m_Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			break;
		}

		const uint32_t totalBufferSize =
			vBuffer.TotalSize +
			nBuffer.TotalSize +
			tBuffer.TotalSize +
			cBuffer.TotalSize +
			iBuffer.TotalSize;

		void* pBuffer = new char[totalBufferSize];
		uint8_t* CPU = static_cast<uint8_t*>(pBuffer);
		uint32_t offset = 0;

		if (!CPU)
			throw std::bad_alloc();

		std::vector<uint32_t> numElements;
		std::vector<uint32_t> elementSize;

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

		delete[] pBuffer;
	}
}

void Mesh::Render(CommandList& commandList)
{
	for (auto& submesh : m_SubMeshes)
	{
		commandList.SetPrimitiveTopology(submesh.m_Topology);
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

