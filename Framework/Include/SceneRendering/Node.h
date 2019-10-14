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
	virtual ~Node();
protected:
	
	static void Load(const fx::gltf::Document& doc, uint32_t nodeIndex, const DirectX::XMMATRIX& parentTransform, std::vector<Node>& nodes);

	const int32_t MeshIndex() const { return m_MeshIndex; }
	const DirectX::XMMATRIX& Transform() const { return m_LocalTransform; }
private:
	std::string			  m_Name;
	DirectX::XMMATRIX	  m_LocalTransform;
	int32_t				  m_MeshIndex;
};
