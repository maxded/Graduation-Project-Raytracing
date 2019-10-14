#pragma once

#include <Node.h>
#include <Mesh.h>
#include <CommandList.h>
#include <MeshInstance.h>
#include <Light.h>
#include <Events.h>

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
		{}

		uint32_t NumPointLights;
		uint32_t NumSpotLights;
		uint32_t NumDirectionalLights;
	};

	/**
	* Scene update for generic lighting setup
	* Override in base class for custom behaviour
	*/
	virtual void Update(UpdateEventArgs& e);

	virtual void Render(CommandList& commandList);

	void SetAnimateLights(bool animate) { m_AnimateLights = animate; }

	void ToggleAnimateLights() { m_AnimateLights = !m_AnimateLights; }

protected:
	// Copies and moves are not allowed.
	/*Scene(const Scene&)				 = delete;
	Scene& operator=(const Scene&)	 = delete;

	Scene(Scene&& allocation)		 = delete;
	Scene& operator=(Scene && other) = delete;*/

	// Load glTF 2.0 scene from file
	void LoadFromFile(const std::string& filename, CommandList& commandList);

	std::string						m_Name;
	std::vector<PointLight>			m_PointLights;
	std::vector<SpotLight>			m_SpotLights;
	std::vector<DirectionalLight>	m_DirectionalLights;

	bool m_AnimateLights;

	std::vector<MeshInstance>		m_MeshInstances;

	/**
	* Data extracted from the glTF 2.0 file
	*/
	struct DocumentData
	{
		DocumentData()
			: Meshes{}
		{}

		std::vector<Mesh>	Meshes;
	};

	/**
	* Scene constant data for shaders
	*/
	struct SceneConstantData
	{
		SceneConstantData()
		{}

		// 
	};

	DocumentData	m_DocumentData;

private:	
};