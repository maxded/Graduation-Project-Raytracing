#include "neel_engine_pch.h"

#include "gltf_scene.h"
#include "camera.h"

#include <DirectXColors.h>

Scene::Scene()
	: name_("Scene")
	  , animate_lights_(true)
{
}

Scene::~Scene()
{
}

void Scene::LoadFromFile(const std::string& filename, CommandList& command_list)
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
		document_data_.Meshes.resize(document.meshes.size());

		for (int i = 0; i < document.meshes.size(); i++)
		{
			document_data_.Meshes[i].Load(document, i,command_list, &materials);
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

	// Load basic geometry for scene (debugging purposes)
	std::string basic_geometry[3];
	basic_geometry[0] = "Assets/BasicGeometry/Cube.gltf";
	basic_geometry[1] = "Assets/BasicGeometry/Sphere.gltf";
	basic_geometry[2] = "Assets/BasicGeometry/Cone.gltf";

	cube_mesh_		= LoadBasicGeometry(basic_geometry[0], command_list);
	sphere_mesh_	= LoadBasicGeometry(basic_geometry[1], command_list);
	cone_mesh_		= LoadBasicGeometry(basic_geometry[2], command_list);	
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
	
	document_data_.Meshes.push_back(m);

	return std::make_unique<Mesh>(document_data_.Meshes.back());
}

void Scene::Unload()
{
	// Reset all mesh's resources on gpu.
	for(auto& mesh : document_data_.Meshes)
	{
		mesh.Unload();
	}
}

void Scene::Update(UpdateEventArgs& e)
{
	Camera& camera = Camera::Get();

	XMMATRIX view_matrix = camera.GetViewMatrix();

	const int num_point_lights			= 12;
	const int num_spot_lights			= 4;
	const int num_directional_lights	= 1;

	static const XMVECTORF32 light_colors[] =
	{
		Colors::Red, Colors::Green, Colors::Blue, Colors::Orange, Colors::DarkTurquoise, Colors::Indigo, Colors::Violet,
		Colors::Aqua, Colors::CadetBlue, Colors::GreenYellow, Colors::Lime, Colors::Azure
	};

	static float light_anim_time = 0.0f;
	if (animate_lights_)
	{
		light_anim_time += static_cast<float>(e.ElapsedTime) * 0.2f * XM_PI;
	}

	const float radius = 3.5f;
	const float offset = 2.0f * XM_PI / num_point_lights;
	const float offset2 = offset + (offset / 2.0f);

	// Setup the light buffers.
	point_lights_.resize(num_point_lights);
	for (int i = 0; i < num_point_lights; ++i)
	{
		PointLight& l = point_lights_[i];

		l.PositionWS = {
			static_cast<float>(std::sin(light_anim_time + offset * i)) * radius,
			2.0f,
			static_cast<float>(std::cos(light_anim_time + offset * i)) * radius,
			1.0f
		};
		XMVECTOR position_ws = XMLoadFloat4(&l.PositionWS);
		XMVECTOR position_vs = XMVector3TransformCoord(position_ws, view_matrix);
		XMStoreFloat4(&l.PositionVS, position_vs);

		l.Color = XMFLOAT4(light_colors[i]);
		l.Intensity = 1.0f;
		l.Attenuation = 0.0f;
	}

	spot_lights_.resize(num_spot_lights);
	for (int i = 0; i < num_spot_lights; ++i)
	{
		SpotLight& l = spot_lights_[i];

		l.PositionWS = {
			static_cast<float>(std::sin(light_anim_time + offset * i + offset2)) * radius,
			9.0f,
			static_cast<float>(std::cos(light_anim_time + offset * i + offset2)) * radius,
			1.0f
		};
		XMVECTOR position_ws = XMLoadFloat4(&l.PositionWS);
		XMVECTOR position_vs = XMVector3TransformCoord(position_ws, view_matrix);
		XMStoreFloat4(&l.PositionVS, position_vs);

		XMVECTOR direction_ws = XMVector3Normalize(XMVectorSetW(XMVectorNegate(position_ws), 0));
		XMVECTOR direction_vs = XMVector3Normalize(XMVector3TransformNormal(direction_ws, view_matrix));
		XMStoreFloat4(&l.DirectionWS, direction_ws);
		XMStoreFloat4(&l.DirectionVS, direction_vs);

		l.Color = XMFLOAT4(light_colors[num_point_lights + i]);
		l.Intensity = 1.0f;
		l.SpotAngle = XMConvertToRadians(45.0f);
		l.Attenuation = 0.0f;
	}

	directional_lights_.resize(num_directional_lights);
	for(int i = 0; i < num_directional_lights; ++i)
	{
		DirectionalLight& l = directional_lights_[i];

		XMVECTOR direction_ws = { -0.57f, 0.57f, 0.57f, 0.0 };
		XMVECTOR direction_vs = XMVector3Normalize(XMVector3TransformNormal(direction_ws, view_matrix));
		
		XMStoreFloat4(&l.DirectionWS, direction_ws);
		XMStoreFloat4(&l.DirectionVS, direction_vs);

		l.Color = XMFLOAT4(Colors::White);
	}
}

void Scene::PrepareRender(CommandList& command_list)
{
}

void Scene::Render(CommandList& command_list)
{
}


