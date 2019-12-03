#include "sponza_scene.h"

SponzaScene::SponzaScene()
{
}

SponzaScene::~SponzaScene()
{
}

void SponzaScene::Load(const std::string& filename, CommandList& command_list)
{
	LoadFromFile(filename, command_list);
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
