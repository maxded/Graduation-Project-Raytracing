#include <FrameworkPCH.h>

#include <Model.h>
#include <Helpers.h>
#include <Mesh.h>

Model::Model()
	: m_Meshes({})
{
	
}

Model::~Model()
{
}

std::unique_ptr<Model> Model::LoadModel(const std::string& filename, CommandList& commandList)
{
	fx::gltf::Document document;

	if (StringEndsWith(filename, ".gltf"))
		document = fx::gltf::LoadFromText(filename);
	if (StringEndsWith(filename, ".glb"))
		document = fx::gltf::LoadFromBinary(filename);

	std::unique_ptr<Model> model(new Model());

	model->m_Meshes.resize(document.meshes.size());

	// Load meshes
	for (int i = 0; i < document.meshes.size(); ++i)
	{	
		Mesh& mesh = model->m_Meshes[i];

		mesh.Load(document, i, commandList);
	}

	return model;
}

void Model::Render(CommandList& commandList)
{
	for (auto& mesh : m_Meshes)
	{
		mesh.Render(commandList);
	}
}


