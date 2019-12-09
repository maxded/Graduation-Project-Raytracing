#pragma once

#include "node.h"
#include "mesh.h"
#include "commandlist.h"
#include "mesh_instance.h"
#include "shader_data.h"
#include "events.h"
#include "texture.h"

class Scene
{
public:
	Scene();
	virtual ~Scene();

	/**
	* Scene light properties for shaders
	*/
	struct SceneLightProperties
	{
		SceneLightProperties()
			 : NumPointLights(0)
			  , NumSpotLights(0)
			  , NumDirectionalLights(0)
		{
		}

		uint32_t NumPointLights;
		uint32_t NumSpotLights;
		uint32_t NumDirectionalLights;
	};

	virtual void Load(const std::string& filename) = 0;

	/**
	* Scene update for generic lighting setup
	* Override in base class for custom behaviour
	*/
	virtual void Update(UpdateEventArgs& e);

	virtual void PrepareRender(CommandList& command_list);

	virtual void Render(CommandList& command_list);

	virtual RenderTarget& GetRenderTarget() = 0;

	void SetAnimateLights(bool animate) { animate_lights_ = animate; }

	void ToggleAnimateLights() { animate_lights_ = !animate_lights_; }

	void Unload();

protected:
	// Load glTF 2.0 scene from file.
	void LoadFromFile(const std::string& filename, CommandList& command_list);

	std::string name_;
	std::vector<PointLight> point_lights_;
	std::vector<SpotLight> spot_lights_;
	std::vector<DirectionalLight> directional_lights_;

	bool animate_lights_;

	std::vector<MeshInstance> mesh_instances_;

	/**
	* Data extracted from the glTF 2.0 file
	*/
	struct DocumentData
	{
		DocumentData()
		{
		}

		std::vector<Mesh>	 Meshes;
		std::vector<Texture> Textures;
	};

	DocumentData	document_data_;
private:
};
