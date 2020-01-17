#pragma once

#include "node.h"
#include "mesh.h"
#include "commandlist.h"
#include "mesh_instance.h"
#include "shader_data.h"
#include "events.h"
#include "material.h"

class Scene
{
public:
	Scene();
	virtual ~Scene();

	// Load glTF 2.0 scene from file.
	void LoadFromFile(const std::string& filename, CommandList& command_list, bool load_basic_geometry = false);

	void LoadBasicGeometry(CommandList& command_list);

	std::vector<Mesh>& GetMeshes() { return meshes_; }
	
	const std::vector<MeshInstance>& GetInstances() const { return mesh_instances_; }

	const bool BasicGeometryLoaded() const { return basic_geometry_loaded_; }
	const int GetTotalMeshes() const { return total_number_meshes_; }

	std::unique_ptr<Mesh> CubeMesh;
	std::unique_ptr<Mesh> SphereMesh;
	std::unique_ptr<Mesh> ConeMesh;
protected:

	std::unique_ptr<Mesh> LoadBasicGeometry(std::string& filepath, CommandList& command_list);
	
	std::string name_;
	std::vector<MeshInstance> mesh_instances_;

	/**
	* Data extracted from the glTF 2.0 file
	*/
	std::vector<Mesh>	meshes_;
	int total_number_meshes_;
	bool basic_geometry_loaded_;
};
