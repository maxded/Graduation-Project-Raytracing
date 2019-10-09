#pragma once

#include <Node.h>
#include <Mesh.h>
#include <CommandList.h>
#include <MeshInstance.h>

class Scenes
{
	struct DocumentData;

public:
	struct Scene
	{
		Scene()
			: m_Name("")
			, m_Instances{}
			, m_pData(nullptr)
		{}

		std::string m_Name;
		std::vector<MeshInstance> m_Instances;
		std::shared_ptr<DocumentData> m_pData;
	};

public:
	Scenes();
	virtual ~Scenes();

	void LoadFromFile(const std::string& filename, CommandList& commandList);

	void RenderCurrentScene(CommandList& commandList);

	const Scene& GetCurrentScene() const { return *m_CurrentScene; }

	void SetCurrentScene(uint32_t index);

protected:

private:
	struct DocumentData
	{
		DocumentData()
			: m_Meshes{}
		{}

		std::vector<Mesh>	m_Meshes;
	};

	std::vector<DocumentData> m_DocumentData;
	std::vector<Scene>		  m_Scenes;
	std::unique_ptr<Scene>	  m_CurrentScene;
};