#include "neel_engine_pch.h"

#include "gltf_scene.h"
#include "camera.h"

#include <DirectXColors.h>

Scene::Scene()
	: CubeMesh(nullptr)
	, SphereMesh(nullptr)
	, ConeMesh(nullptr)
	, name_("Scene")
	, total_number_meshes_(0)
	, basic_geometry_loaded_(false)
{}

Scene::~Scene()
{}

void Scene::LoadFromFile(const std::string& filename, CommandList& command_list, bool load_basic_geometry)
{
	// Load glTF 2.0 document.
	fx::gltf::Document document;

	if (StringEndsWith(filename, ".gltf"))
		document = fx::gltf::LoadFromText(filename);
	if (StringEndsWith(filename, ".glb"))
		document = fx::gltf::LoadFromBinary(filename);

	std::vector<Material> materials(document.materials.size());
	
	// Generate material data for current document.
	{
		for(int i = 0; i < document.materials.size(); i++)
		{
			const fx::gltf::Material& material = document.materials[i];
			materials[i].Load(document, i, command_list, filename);
		}
	}
	
	// Generate mesh data for current document.
	{
		meshes_.resize(document.meshes.size());

		for (int i = 0; i < document.meshes.size(); i++)
		{
			meshes_[i].Load(document, i, command_list, &materials);
			total_number_meshes_ += meshes_[i].GetSubMeshes().size();
		}
	}

	// Generate node hiearchy for scene[0]
	{
		// RESTRICTION: only load document.scenes[0] .
		if (!document.scenes.empty())
		{
			const XMMATRIX root_transform = DirectX::XMMatrixIdentity();

			std::vector<Node> nodes(document.nodes.size());

			if (!document.scenes[0].name.empty())
			{
				name_ = document.scenes[0].name;
			}

			for (const uint32_t scene_node : document.scenes[0].nodes)
			{
				Node::Load(document, scene_node, root_transform, nodes);
			}

			for (auto& node : nodes)
			{
				if (node.MeshIndex() >= 0)
				{
					mesh_instances_.push_back({ node.Transform(), node.MeshIndex() });

					// Set base transform for mesh.			
					meshes_[node.mesh_index_].SetBaseTransform(node.Transform());
				}
			}
		}
		// If no scene graph is present, display all individual meshes.
		else
		{
			static uint32_t scene_number = 0;
			name_ = "Scene " + std::to_string(scene_number);

			// No scene data - display individual meshes.
			for (int32_t i = 0; i < document.meshes.size(); i++)
			{
				mesh_instances_.push_back({ XMMatrixIdentity(), i });
			}
		}
	}

	// Sort mesh instances based on pipelinestate object.

	if(load_basic_geometry)
	{
		LoadBasicGeometry(command_list);		
	}
}

void Scene::LoadBasicGeometry(CommandList& command_list)
{
	// Load basic geometry for scene (debugging purposes)
	std::string basic_geometry[3];
	basic_geometry[0] = "Assets/BasicGeometry/Cube.gltf";
	basic_geometry[1] = "Assets/BasicGeometry/Sphere.gltf";
	basic_geometry[2] = "Assets/BasicGeometry/Cone.gltf";

	CubeMesh		= LoadBasicGeometry(basic_geometry[0], command_list);
	SphereMesh		= LoadBasicGeometry(basic_geometry[1], command_list);
	ConeMesh		= LoadBasicGeometry(basic_geometry[2], command_list);

	basic_geometry_loaded_ = true;
}

std::unique_ptr<Mesh> Scene::LoadBasicGeometry(std::string& filepath, CommandList& command_list)
{
	// Load glTF 2.0 document.
	fx::gltf::Document document;

	if (StringEndsWith(filepath, ".gltf"))
		document = fx::gltf::LoadFromText(filepath);
	if (StringEndsWith(filepath, ".glb"))
		document = fx::gltf::LoadFromBinary(filepath);

	Mesh m;
	m.Load(document, 0, command_list);
	
	meshes_.push_back(m);

	total_number_meshes_++;

	return std::make_unique<Mesh>(meshes_.back());
}




