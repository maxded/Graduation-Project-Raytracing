#include <FrameworkPCH.h>

#include <Node.h>

using namespace DirectX;

Node::Node()
	: m_MeshIndex(-1)
	, m_LocalTransform(XMMatrixIdentity())
	, m_Name("Node")
{

}

Node::~Node()
{

}

void Node::Load(fx::gltf::Document const& doc, uint32_t nodeIndex, const DirectX::XMMATRIX& parentTransform, std::vector<Node>& nodes)
{
	const fx::gltf::Node& node = doc.nodes[nodeIndex];

	m_Name			 = node.name;
	m_MeshIndex		 = node.mesh;
	m_LocalTransform = parentTransform;

	if (node.matrix != fx::gltf::defaults::IdentityMatrix)
	{
		const XMFLOAT4X4 transform(node.matrix.data());
		m_LocalTransform = XMLoadFloat4x4(&transform) * m_LocalTransform;
	}
	else
	{
		if (node.translation != fx::gltf::defaults::NullVec3)
		{
			const XMFLOAT3 translation(node.translation.data());
			m_LocalTransform = XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&translation)) * m_LocalTransform;
		}

		if (node.rotation != fx::gltf::defaults::IdentityVec4)
		{
			const XMFLOAT4 rotation(node.rotation.data());
			m_LocalTransform = XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation)) * m_LocalTransform;
		}

		if (node.scale != fx::gltf::defaults::IdentityVec3)
		{
			const XMFLOAT3 scale(node.scale.data());
			m_LocalTransform = DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&scale)) * m_LocalTransform;
		}	
	}

	for (auto& child : node.children)
	{
		nodes[child].Load(doc, child, m_LocalTransform, nodes);
	}
}
