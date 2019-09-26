#pragma once

#include <Node.h>
#include <Mesh.h>
#include <CommandList.h>
#include <MeshInstance.h>

class Scenes
{
public:
	struct Scene
	{
		std::string m_Name;
		std::vector<uint32_t>	  m_SceneNodes;
		std::vector<MeshInstance> m_Instances;
	};

public:
	Scenes();
	virtual ~Scenes();

	void LoadFromFile(const std::string& filename, CommandList& commandList);

	void RenderCurrentScene(CommandList& commandList);

	const Scene& GetCurrentScene() const { return *m_CurrentScene; }

protected:

private:
	std::vector<Scene> m_Scenes;
	std::vector<Node>  m_Nodes;
	std::vector<Mesh>  m_Meshes;

	std::unique_ptr<Scene> m_CurrentScene;
};