#include <FrameworkPCH.h>

#include <Scenes.h>
#include <gltf.h>

using namespace DirectX;

Scenes::Scenes()
	: m_Scenes{}
	, m_DocumentData{}
	, m_CurrentScene(nullptr)
{}

Scenes::~Scenes()
{}

void Scenes::LoadFromFile(const std::string& filename, CommandList& commandList)
{
	// Load glTF 2.0 document
	fx::gltf::Document document;

	if (StringEndsWith(filename, ".gltf"))
		document = fx::gltf::LoadFromText(filename);
	if (StringEndsWith(filename, ".glb"))
		document = fx::gltf::LoadFromBinary(filename);

	DocumentData documentData;

	documentData.m_Meshes.resize(document.meshes.size());

	// Generate mesh data for current document
	for (int i = 0; i < document.meshes.size(); i++)
	{
		documentData.m_Meshes[i].Load(document, i, commandList);
	}

	const XMMATRIX rootTransform = XMMatrixIdentity();

	std::vector<Node> Nodes(document.nodes.size());

	if (!document.scenes.empty())
	{
		// Generate scenes and nodes
		for (int i = 0; i < document.scenes.size(); i++)
		{
			Scene scene;
			scene.m_Name = document.scenes[i].name;

			for (const uint32_t sceneNode : document.scenes[i].nodes)
			{
				Node::Load(document, sceneNode, rootTransform, Nodes);
			}

			for (auto& node : Nodes)
			{
				if (node.MeshIndex() >= 0)
				{
					scene.m_Instances.push_back({ node.Transform(), node.MeshIndex() });
				}
			}

			scene.m_pData = std::make_shared<DocumentData>(documentData);
			m_Scenes.emplace_back(scene);
		}
	}
	else
	{
		Scene scene;

		static uint32_t sceneNumber = 0;
		scene.m_Name = "Scene " + std::to_string(sceneNumber);

		// No scene data - display individual meshes
		for (int32_t i = 0; i < document.meshes.size(); i++)
		{
			scene.m_Instances.push_back({ XMMatrixIdentity(), i });
		}

		scene.m_pData = std::make_shared<DocumentData>(documentData);
		m_Scenes.emplace_back(scene);
	}

	m_DocumentData.emplace_back(documentData);
	
	if (!m_CurrentScene)
	{
		m_CurrentScene = std::make_unique<Scene>(m_Scenes[0]);
	}
}

void Scenes::RenderCurrentScene(CommandList& commandList)
{
	// Loop over all instances of scene and render
	for (auto& instance : m_CurrentScene->m_Instances)
	{
		Mesh& mesh = m_CurrentScene->m_pData->m_Meshes[instance.MeshIndex];
		mesh.SetBaseTransform(instance.Transform);
		mesh.Render(commandList);
	}
}

void Scenes::SetCurrentScene(uint32_t index)
{
	// Check for valid index
	if (index <= m_Scenes.size() - 1)
		m_CurrentScene = std::make_unique<Scene>(m_Scenes[index]);
}

