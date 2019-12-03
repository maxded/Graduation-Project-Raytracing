#pragma once

#include <string>
#include <vector>
#include <DirectXMath.h>

#include <gltf.h>

class Node
{
	friend class Scenes;
	friend class Scene;
public:
	Node();
	~Node();
protected:

	static void Load(const fx::gltf::Document& doc, uint32_t node_index, const DirectX::XMMATRIX& parent_transform,
	                 std::vector<Node>& nodes);

	const int32_t MeshIndex() const { return mesh_index_; }
	const DirectX::XMMATRIX& Transform() const { return local_transform_; }
private:
	std::string name_;
	DirectX::XMMATRIX local_transform_;
	int32_t mesh_index_;
};
