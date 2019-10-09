#include <FrameworkPCH.h>

#include <Node.h>

Node::Node()
	: m_MeshIndex(-1)
	, m_LocalTransform(XMMatrixIdentity())
	, m_Name("Node")
{

}

Node::~Node()
{

}

void Node::Load(fx::gltf::Document const& doc, uint32_t nodeIndex, const XMMATRIX& parentTransform, std::vector<Node>& nodes)
{
	const fx::gltf::Node& node = doc.nodes[nodeIndex];

	Node& graphNode = nodes[nodeIndex];

	graphNode.m_Name			 = node.name;
	graphNode.m_MeshIndex		 = node.mesh;
	graphNode.m_LocalTransform   = parentTransform;

	if (node.matrix != fx::gltf::defaults::IdentityMatrix)
	{
		const XMFLOAT4X4 transform(node.matrix.data());
		graphNode.m_LocalTransform = XMLoadFloat4x4(&transform) * graphNode.m_LocalTransform;
	}
	else
	{
		if (node.translation != fx::gltf::defaults::NullVec3)
		{
			const XMFLOAT3 translation(node.translation.data());
			graphNode.m_LocalTransform = XMMatrixTranslationFromVector(XMLoadFloat3(&translation)) * graphNode.m_LocalTransform;
		}

		if (node.scale != fx::gltf::defaults::IdentityVec3)
		{
			const XMFLOAT3 scale(node.scale.data());
			graphNode.m_LocalTransform = XMMatrixScalingFromVector(XMLoadFloat3(&scale)) * graphNode.m_LocalTransform;
		}

		if (node.rotation != fx::gltf::defaults::IdentityRotation)
		{
			const DirectX::XMFLOAT4 rotation(node.rotation.data());
			graphNode.m_LocalTransform = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation)) * graphNode.m_LocalTransform;
		}
	}


	char buffer[512];
	sprintf_s(buffer, "Node Index: %i\n", nodeIndex);
	OutputDebugStringA(buffer);

	if (node.camera >= 0)
	{
		
	}
	else
	{
		for (auto index : node.children)
		{
			Load(doc, index, graphNode.m_LocalTransform, nodes);
		}
	}
}
