#include <FrameworkPCH.h>

#include <Scenes.h>
#include <gltf.h>

using namespace DirectX;

Scenes::Scenes()
	: m_Scenes{}
	, m_Meshes{}
{

}

Scenes::~Scenes()
{

}

void Scenes::LoadFromFile(const std::string& filename, CommandList& commandList)
{
	// Load glTF 2.0 document
	fx::gltf::Document document;

	if (StringEndsWith(filename, ".gltf"))
		document = fx::gltf::LoadFromText(filename);
	if (StringEndsWith(filename, ".glb"))
		document = fx::gltf::LoadFromBinary(filename);

	m_Scenes.resize(document.scenes.size());
	m_Nodes.resize(document.nodes.size());
	m_Meshes.resize(document.meshes.size());

	// Generate meshes
	for (int i = 0; i < document.meshes.size(); ++i)
	{
		m_Meshes[i].Load(document, i, commandList);
	}

	const XMMATRIX rootTransform = DirectX::XMMatrixMultiply(DirectX::XMMatrixIdentity(), DirectX::XMMatrixScaling(1, 1, 1));

	// Generate scenes and nodes
	for (int i = 0; i < document.scenes.size(); ++i)
	{
		m_Scenes[i].m_Name = document.scenes[i].name;

		for (const uint32_t node : document.scenes[i].nodes)
		{
			m_Nodes[node].Load(document, node, rootTransform, m_Nodes);

			const Node& currentSceneNode = m_Nodes[node];
			if(currentSceneNode.MeshIndex() > -1)
				m_Scenes[i].m_Instances.push_back({ currentSceneNode.Transform(), currentSceneNode.MeshIndex() });

			m_Scenes[i].m_SceneNodes.push_back(node);
		}

		// Set base transform for all mesh instances
		for (auto& instance : m_Scenes[i].m_Instances)
		{
			Mesh& mesh = m_Meshes[instance.MeshIndex];
			mesh.SetBaseTransform(instance.Transform);
		}
	}

	if (document.scene > -1)
		m_CurrentScene = std::make_unique<Scene>(m_Scenes[document.scene]);
	else
		m_CurrentScene = std::make_unique<Scene>(m_Scenes[0]);
}

void Scenes::RenderCurrentScene(CommandList& commandList)
{
	// Loop over all instances of scene and render
	for (auto& instance : m_CurrentScene->m_Instances)
	{
		Mesh& mesh = m_Meshes[instance.MeshIndex];
		mesh.Render(commandList);
	}
}

