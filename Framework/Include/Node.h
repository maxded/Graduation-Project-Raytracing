#pragma once

#include <string>
#include <vector>
#include <DirectXMath.h>

#include <gltf.h>

class Node
{
	friend class Scenes;
public:
	Node();
	virtual ~Node();
protected:
	
	void Load(fx::gltf::Document const& doc, uint32_t nodeIndex, const DirectX::XMMATRIX& parentTransform, std::vector<Node>& nodes);

	int32_t MeshIndex() const { return m_MeshIndex; }
	DirectX::XMMATRIX Transform() const { return m_LocalTransform; }
private:
	std::string			  m_Name;
	DirectX::XMMATRIX	  m_LocalTransform;
	int32_t				  m_MeshIndex;
};
