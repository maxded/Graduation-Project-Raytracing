#pragma once

#include <CommandList.h>
#include <gltf.h>

#include <string>
#include <vector>

class Mesh;

class Model
{
public:
	virtual ~Model();

	static std::unique_ptr<Model> LoadModel(const std::string& filename, CommandList& commandList);

	void Render(CommandList& commandlist);
protected:

private:
	Model();
	Model(const Model& copy) = delete;
	
	std::vector<Mesh> m_Meshes;	
};