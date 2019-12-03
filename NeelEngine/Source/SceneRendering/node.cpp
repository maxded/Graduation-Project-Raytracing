#include "neel_engine_pch.h"

#include "node.h"

Node::Node()
	: name_("Node")
	  , local_transform_(XMMatrixIdentity())
	  , mesh_index_(-1)
{
}

Node::~Node()
{
}

void Node::Load(fx::gltf::Document const& doc, uint32_t node_index, const XMMATRIX& parent_transform,
                std::vector<Node>& nodes)
{
	const fx::gltf::Node& node = doc.nodes[node_index];

	Node& graph_node = nodes[node_index];

	graph_node.name_ = node.name;
	graph_node.mesh_index_ = node.mesh;
	graph_node.local_transform_ = parent_transform;

	if (node.matrix != fx::gltf::defaults::IdentityMatrix)
	{
		const XMFLOAT4X4 transform(node.matrix.data());
		graph_node.local_transform_ = XMLoadFloat4x4(&transform) * graph_node.local_transform_;
	}
	else
	{
		if (node.translation != fx::gltf::defaults::NullVec3)
		{
			const XMFLOAT3 translation(node.translation.data());
			graph_node.local_transform_ = XMMatrixTranslationFromVector(XMLoadFloat3(&translation)) * graph_node.
				local_transform_;
		}

		if (node.scale != fx::gltf::defaults::IdentityVec3)
		{
			const XMFLOAT3 scale(node.scale.data());
			graph_node.local_transform_ = XMMatrixScalingFromVector(XMLoadFloat3(&scale)) * graph_node.
				local_transform_;
		}

		if (node.rotation != fx::gltf::defaults::IdentityRotation)
		{
			const DirectX::XMFLOAT4 rotation(node.rotation.data());
			graph_node.local_transform_ = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation)) *
				graph_node.local_transform_;
		}
	}

	if (node.camera >= 0)
	{
	}
	else
	{
		for (auto index : node.children)
		{
			Load(doc, index, graph_node.local_transform_, nodes);
		}
	}
}
