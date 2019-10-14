#include "DefaultScene.h"

DefaultScene::DefaultScene()
{}

DefaultScene::~DefaultScene()
{}

void DefaultScene::Load(const std::string& filename, CommandList& commandList)
{
	LoadFromFile(filename, commandList);
}

void DefaultScene::Update(UpdateEventArgs& e)
{
	Scene::Update(e);
}

void DefaultScene::Render(CommandList& commandList)
{
	// Upload lights
	SceneLightProperties lightProps;
	lightProps.NumPointLights		= static_cast<uint32_t>(m_PointLights.size());
	lightProps.NumSpotLights		= static_cast<uint32_t>(m_SpotLights.size());
	lightProps.NumDirectionalLights = static_cast<uint32_t>(m_DirectionalLights.size());

	commandList.SetGraphics32BitConstants(2, lightProps);
	commandList.SetGraphicsDynamicStructuredBuffer(3, m_PointLights);
	commandList.SetGraphicsDynamicStructuredBuffer(4, m_SpotLights);
	
	// Loop over all instances of scene and render
	for (auto& instance : m_MeshInstances)
	{
		Mesh& mesh = m_DocumentData.Meshes[instance.MeshIndex];
		mesh.SetBaseTransform(instance.Transform);
		mesh.Render(commandList);
	}
}
