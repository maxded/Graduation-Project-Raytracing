#include "neel_engine_pch.h"

#include "scene.h"
#include "camera.h"
#include <DirectXColors.h>

Scene::Scene()
	: name_("Scene")
	  , animate_lights_(false)
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

	document_data_.Meshes.resize(document.meshes.size());

	// Generate mesh data for current document.
	for (int i = 0; i < document.meshes.size(); i++)
	{
		document_data_.Meshes[i].Load(document, i, command_list);
	}

	// RESTRICTION: only load document.scenes[0] .
	if (!document.scenes.empty())
	{
		const XMMATRIX root_transform = XMMatrixIdentity();
		
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
				mesh_instances_.push_back({node.Transform(), node.MeshIndex()});
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
			mesh_instances_.push_back({XMMatrixIdentity(), i});
		}
	}
}

void Scene::Unload()
{
	for(auto& mesh : document_data_.Meshes)
	{
		mesh.Unload();
	}
}

void Scene::Update(UpdateEventArgs& e)
{
	Camera& camera = Camera::Get();

	XMMATRIX view_matrix = camera.GetViewMatrix();

	const int num_point_lights = 4;
	const int num_spot_lights = 4;

	static const XMVECTORF32 light_colors[] =
	{
		Colors::White, Colors::Orange, Colors::Yellow, Colors::Green, Colors::Blue, Colors::Indigo, Colors::Violet,
		Colors::White
	};

	static float light_anim_time = 0.0f;
	if (animate_lights_)
	{
		light_anim_time += static_cast<float>(e.ElapsedTime) * 0.5f * XM_PI;
	}

	const float radius = 8.0f;
	const float offset = 2.0f * XM_PI / num_point_lights;
	const float offset2 = offset + (offset / 2.0f);

	// Setup the light buffers.
	point_lights_.resize(num_point_lights);
	for (int i = 0; i < num_point_lights; ++i)
	{
		PointLight& l = point_lights_[i];

		l.PositionWS = {
			static_cast<float>(std::sin(light_anim_time + offset * i)) * radius,
			9.0f,
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
}

void Scene::Render(CommandList& command_list)
{
}
