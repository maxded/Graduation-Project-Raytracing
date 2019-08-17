#include <FrameworkPCH.h>

#include <Mesh.h>
#include <Application.h>

using namespace DirectX;
using namespace Microsoft::WRL;

const D3D12_INPUT_ELEMENT_DESC VertexData::InputElements[] =
{
	{ "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

Mesh::Mesh()
	: m_IndexCount(0)
{}

Mesh::~Mesh()
{
	// Allocated resources will be cleaned automatically when the pointers go out of scope.
}

//std::unique_ptr<Mesh> Mesh::CreateCube(CommandList& commandList, float size, bool rhcoords)
//{
//	// A cube has six faces, each one pointing in a different direction.
//	const int FaceCount = 6;
//
//	static const XMVECTORF32 faceNormals[FaceCount] =
//	{
//		{ 0,  0,  1 },
//		{ 0,  0, -1 },
//		{ 1,  0,  0 },
//		{ -1,  0,  0 },
//		{ 0,  1,  0 },
//		{ 0, -1,  0 },
//	};
//
//	static const XMVECTORF32 textureCoordinates[4] =
//	{
//		{ 1, 0 },
//		{ 1, 1 },
//		{ 0, 1 },
//		{ 0, 0 },
//	};
//
//	VertexCollection vertices;
//	IndexCollection  indices;
//
//	size /= 2;
//
//	// Create each face in turn.
//	for (int i = 0; i < FaceCount; i++)
//	{
//		XMVECTOR normal = faceNormals[i];
//
//		// Get two vectors perpendicular both to the face normal and to each other.
//		XMVECTOR basis = (i >= 4) ? g_XMIdentityR2 : g_XMIdentityR1;
//
//		XMVECTOR side1 = XMVector3Cross(normal, basis);
//		XMVECTOR side2 = XMVector3Cross(normal, side1);
//
//		// Six indices (two triangles) per face.
//		size_t vbase = vertices.size();
//		indices.push_back(static_cast<uint16_t>(vbase + 0));
//		indices.push_back(static_cast<uint16_t>(vbase + 1));
//		indices.push_back(static_cast<uint16_t>(vbase + 2));
//
//		indices.push_back(static_cast<uint16_t>(vbase + 0));
//		indices.push_back(static_cast<uint16_t>(vbase + 2));
//		indices.push_back(static_cast<uint16_t>(vbase + 3));
//
//		// Four vertices per face.
//		vertices.push_back(VertexData((normal - side1 - side2) * size, normal, textureCoordinates[0]));
//		vertices.push_back(VertexData((normal - side1 + side2) * size, normal, textureCoordinates[1]));
//		vertices.push_back(VertexData((normal + side1 + side2) * size, normal, textureCoordinates[2]));
//		vertices.push_back(VertexData((normal + side1 - side2) * size, normal, textureCoordinates[3]));
//	}
//
//	// Create the primitive object.
//	std::unique_ptr<Mesh> mesh(new Mesh());
//
//	mesh->Initialize(commandList, vertices, indices, rhcoords);
//
//	return mesh;
//}
//
//// Helper for flipping winding of geometric primitives for LH vs. RH coords
//static void ReverseWinding(IndexCollection& indices, VertexCollection& vertices)
//{
//	assert((indices.size() % 3) == 0);
//	for (auto it = indices.begin(); it != indices.end(); it += 3)
//	{
//		std::swap(*it, *(it + 2));
//	}
//
//	for (auto it = vertices.begin(); it != vertices.end(); ++it)
//	{
//		it->textureCoordinate.x = (1.f - it->textureCoordinate.x);
//	}
//}
//
//void Mesh::Initialize(CommandList& commandList, VertexCollection& vertices, IndexCollection& indices, bool rhcoords)
//{
//	if (vertices.size() >= USHRT_MAX)
//		throw std::exception("Too many vertices for 16-bit index buffer");
//
//	if (!rhcoords)
//		ReverseWinding(indices, vertices);
//
//	commandList.CopyVertexBuffer(m_VertexBuffer, vertices);
//	commandList.CopyIndexBuffer(m_IndexBuffer, indices);
//
//	m_IndexCount = static_cast<UINT>(indices.size());
//}