#include <FrameworkPCH.h>

#include <Mesh.h>
#include <Application.h>

#include <cstdlib>

using namespace DirectX;
using namespace Microsoft::WRL;

const D3D12_INPUT_ELEMENT_DESC VertexData::InputElements[] =
{
	{ "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

Mesh::Mesh()
	: m_IndexCount(0)
{}

Mesh::~Mesh()
{
	// Allocated resources will be cleaned automatically when the pointers go out of scope.
}

void Mesh::Render(ID3D12GraphicsCommandList2& commandList, uint32_t instanceCount, uint32_t firstInstance)
{
	commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList.IASetVertexBuffers(0, 1, &m_VertexBuffer.GetVertexBufferView());
	
	if (m_IndexCount > 0)
	{
		commandList.IASetIndexBuffer(&m_IndexBuffer.GetIndexBufferView());
		commandList.DrawIndexedInstanced(m_IndexCount, instanceCount, 0, 0, firstInstance);
	}
	else
	{
		commandList.DrawInstanced(m_VertexBuffer.GetNumVertices(), instanceCount, 0, firstInstance);
	}
}

std::unique_ptr<Mesh> Mesh::CreateCube(ID3D12GraphicsCommandList2& commandList, float size, bool rhcoords)
{
	// A cube has six faces, each one pointing in a different direction.
	const int FaceCount = 6;

	static const XMVECTORF32 faceNormals[FaceCount] =
	{
		{ 0,  0,  1 },
		{ 0,  0, -1 },
		{ 1,  0,  0 },
		{ -1,  0,  0 },
		{ 0,  1,  0 },
		{ 0, -1,  0 },
	};

	static const XMVECTORF32 textureCoordinates[4] =
	{
		{ 1, 0 },
		{ 1, 1 },
		{ 0, 1 },
		{ 0, 0 },
	};

	VertexCollection vertices;
	IndexCollection  indices;

	size /= 2;

	// Create each face in turn.
	for (int i = 0; i < FaceCount; i++)
	{
		XMVECTOR normal = faceNormals[i];

		// Get two vectors perpendicular both to the face normal and to each other.
		XMVECTOR basis = (i >= 4) ? g_XMIdentityR2 : g_XMIdentityR1;

		XMVECTOR side1 = XMVector3Cross(normal, basis);
		XMVECTOR side2 = XMVector3Cross(normal, side1);

		// Six indices (two triangles) per face.
		size_t vbase = vertices.size();
		indices.push_back(static_cast<uint16_t>(vbase + 0));
		indices.push_back(static_cast<uint16_t>(vbase + 1));
		indices.push_back(static_cast<uint16_t>(vbase + 2));

		indices.push_back(static_cast<uint16_t>(vbase + 0));
		indices.push_back(static_cast<uint16_t>(vbase + 2));
		indices.push_back(static_cast<uint16_t>(vbase + 3));

		float r = rand() % 100 - 10;
		float g = rand() % 100 - 10;
		float b = rand() % 100 - 10;

	
		FXMVECTOR color = XMVectorSet(r, g, b, 0.0f);

		// Four vertices per face.
		vertices.push_back(VertexData((normal - side1 - side2) * size, normal, color, textureCoordinates[0]));
		vertices.push_back(VertexData((normal - side1 + side2) * size, normal, color, textureCoordinates[1]));
		vertices.push_back(VertexData((normal + side1 + side2) * size, normal, color, textureCoordinates[2]));
		vertices.push_back(VertexData((normal + side1 - side2) * size, normal, color, textureCoordinates[3]));
	}

	// Create the primitive object.
	std::unique_ptr<Mesh> mesh(new Mesh());

	mesh->Initialize(commandList, vertices, indices, rhcoords);

	return mesh;
}

// Helper for flipping winding of geometric primitives for LH vs. RH coords
static void ReverseWinding(IndexCollection& indices, VertexCollection& vertices)
{
	assert((indices.size() % 3) == 0);
	for (auto it = indices.begin(); it != indices.end(); it += 3)
	{
		std::swap(*it, *(it + 2));
	}

	for (auto it = vertices.begin(); it != vertices.end(); ++it)
	{
		it->textureCoordinate.x = (1.f - it->textureCoordinate.x);
	}
}

void Mesh::Initialize(ID3D12GraphicsCommandList2& commandList, VertexCollection& vertices, IndexCollection& indices, bool rhcoords)
{
	if (vertices.size() >= USHRT_MAX)
		throw std::exception("Too many vertices for 16-bit index buffer");

	if (!rhcoords)
		ReverseWinding(indices, vertices);
	
	CopyBuffer(commandList, m_VertexBuffer, vertices.size(), sizeof(vertices[0]), vertices.data());

	assert(sizeof(indices[0]) == 2 || sizeof(indices[0]) == 4);

	DXGI_FORMAT indexFormat = (sizeof(indices[0]) == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	size_t indexSizeInBytes = indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;

	CopyBuffer(commandList, m_IndexBuffer, indices.size(), indexSizeInBytes, indices.data());

	m_IndexCount = static_cast<UINT>(indices.size());
}

void Mesh::CopyBuffer(ID3D12GraphicsCommandList2& commandList, Buffer& buffer, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
{
	auto device = Application::Get().GetDevice();

	size_t bufferSize = numElements * elementSize;

	ComPtr<ID3D12Resource> d3d12Resource;

	if (bufferSize == 0)
	{
		// This will result in a NULL resource (which may be desired to define a default null resource).
	}
	else
	{
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&d3d12Resource)));

		if (bufferData != nullptr)
		{
			// Create an upload resource to use as an intermediate buffer to copy the buffer resource 
			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&buffer.m_uploadResource)));

			D3D12_SUBRESOURCE_DATA subresourceData = {};
			subresourceData.pData		= bufferData;
			subresourceData.RowPitch	= bufferSize;
			subresourceData.SlicePitch	= subresourceData.RowPitch;

			UpdateSubresources(&commandList, d3d12Resource.Get(), buffer.m_uploadResource.Get(), 0, 0, 1, &subresourceData);
		}
	}

	buffer.SetD3D12Resource(d3d12Resource);
	buffer.CreateViews(numElements, elementSize);
}