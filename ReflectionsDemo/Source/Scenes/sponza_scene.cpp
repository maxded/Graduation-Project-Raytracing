#include "sponza_scene.h"
#include "neel_engine.h"
#include "commandqueue.h"

SponzaScene::SponzaScene()
{
}

SponzaScene::~SponzaScene()
{
}

void SponzaScene::Load(const std::string& filename)
{
	auto command_queue = NeelEngine::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto command_list = command_queue->GetCommandList();
	
	LoadFromFile(filename, *command_list);

	auto fence_value = command_queue->ExecuteCommandList(command_list);
	command_queue->WaitForFenceValue(fence_value);
	
}

void SponzaScene::Update(UpdateEventArgs& e)
{
	Scene::Update(e);
}

void SponzaScene::Render(CommandList& command_list)
{
	// Upload lights
	SceneLightProperties light_props;
	light_props.NumPointLights = static_cast<uint32_t>(point_lights_.size());
	light_props.NumSpotLights = static_cast<uint32_t>(spot_lights_.size());
	light_props.NumDirectionalLights = static_cast<uint32_t>(directional_lights_.size());

	command_list.SetGraphics32BitConstants(2, light_props);
	command_list.SetGraphicsDynamicStructuredBuffer(3, point_lights_);
	command_list.SetGraphicsDynamicStructuredBuffer(4, spot_lights_);

	// Loop over all instances of scene and render
	for (auto& instance : mesh_instances_)
	{
		Mesh& mesh = document_data_.Meshes[instance.MeshIndex];
		mesh.SetBaseTransform(instance.Transform);
		mesh.Render(command_list);
	}
}
