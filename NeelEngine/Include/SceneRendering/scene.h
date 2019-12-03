#pragma once

#include "node.h"
#include "mesh.h"
#include "commandlist.h"
#include "mesh_instance.h"
#include "shader_data.h"
#include "events.h"

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
			: NumDirectionalLights(0)
			  , NumSpotLights(0)
			  , NumPointLights(0)
		{
		}

		uint32_t NumPointLights;
		uint32_t NumSpotLights;
		uint32_t NumDirectionalLights;
	};

	/**
	* Scene update for generic lighting setup
	* Override in base class for custom behaviour
	*/
	virtual void Update(UpdateEventArgs& e);

	virtual void Render(CommandList& command_list);

	void SetAnimateLights(bool animate) { animate_lights_ = animate; }

	void ToggleAnimateLights() { animate_lights_ = !animate_lights_; }

protected:
	// Copies and moves are not allowed.
	/*Scene(const Scene&)				 = delete;
	Scene& operator=(const Scene&)	 = delete;

	Scene(Scene&& allocation)		 = delete;
	Scene& operator=(Scene && other) = delete;*/

	// Load glTF 2.0 scene from file
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

		std::vector<Mesh> Meshes;
	};

	/**
	* Scene constant data for shaders
	*/
	struct SceneConstantData
	{
		SceneConstantData()
		{
		}

		// 
	};

	DocumentData document_data_;

private:
};
