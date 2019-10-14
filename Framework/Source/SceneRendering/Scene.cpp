#include <FrameworkPCH.h>

#include <Scene.h>
#include <gltf.h>
#include <Camera.h>
#include <DirectXColors.h>

Scene::Scene()
	: m_MeshInstances{}
	, m_DocumentData{}
	, m_AnimateLights(false)
	, m_DirectionalLights{}
	, m_SpotLights{}
	, m_PointLights{}
	, m_Name("Scene")
{}

Scene::~Scene()
{}

void Scene::LoadFromFile(const std::string& filename, CommandList& commandList)
{
	// Load glTF 2.0 document
	fx::gltf::Document document;

	if (StringEndsWith(filename, ".gltf"))
		document = fx::gltf::LoadFromText(filename);
	if (StringEndsWith(filename, ".glb"))
		document = fx::gltf::LoadFromBinary(filename);

	m_DocumentData.Meshes.resize(document.meshes.size());

	// Generate mesh data for current document
	for (int i = 0; i < document.meshes.size(); i++)
	{
		m_DocumentData.Meshes[i].Load(document, i, commandList);
	}

	const XMMATRIX rootTransform = XMMatrixIdentity();

	std::vector<Node> Nodes(document.nodes.size());

	if (!document.scenes.empty())
	{		
		if (!document.scenes[0].name.empty())
		{
			m_Name = document.scenes[0].name;
		}
			
		for (const uint32_t sceneNode : document.scenes[0].nodes)
		{
			Node::Load(document, sceneNode, rootTransform, Nodes);
		}

		for (auto& node : Nodes)
		{
			if (node.MeshIndex() >= 0)
			{
				m_MeshInstances.push_back({ node.Transform(), node.MeshIndex() });
			}
		}
	}
	else
	{
		static uint32_t sceneNumber = 0;
		m_Name = "Scene " + std::to_string(sceneNumber);

		// No scene data - display individual meshes
		for (int32_t i = 0; i < document.meshes.size(); i++)
		{
			m_MeshInstances.push_back({ XMMatrixIdentity(), i });
		}
	}
}

void Scene::Update(UpdateEventArgs& e)
{
	Camera& camera = Camera::Get();

	XMMATRIX viewMatrix = camera.get_ViewMatrix();

	const int numPointLights = 4;
	const int numSpotLights = 4;

	static const XMVECTORF32 LightColors[] =
	{
		Colors::White, Colors::Orange, Colors::Yellow, Colors::Green, Colors::Blue, Colors::Indigo, Colors::Violet, Colors::White
	};

	static float lightAnimTime = 0.0f;
	if (m_AnimateLights)
	{
		lightAnimTime += static_cast<float>(e.ElapsedTime) * 0.5f * XM_PI;
	}

	const float radius = 8.0f;
	const float offset = 2.0f * XM_PI / numPointLights;
	const float offset2 = offset + (offset / 2.0f);

	// Setup the light buffers.
	m_PointLights.resize(numPointLights);
	for (int i = 0; i < numPointLights; ++i)
	{
		PointLight& l = m_PointLights[i];

		l.PositionWS = {
			static_cast<float>(std::sin(lightAnimTime + offset * i)) * radius,
			9.0f,
			static_cast<float>(std::cos(lightAnimTime + offset * i)) * radius,
			1.0f
		};
		XMVECTOR positionWS = XMLoadFloat4(&l.PositionWS);
		XMVECTOR positionVS = XMVector3TransformCoord(positionWS, viewMatrix);
		XMStoreFloat4(&l.PositionVS, positionVS);

		l.Color = XMFLOAT4(LightColors[i]);
		l.Intensity = 1.0f;
		l.Attenuation = 0.0f;
	}

	m_SpotLights.resize(numSpotLights);
	for (int i = 0; i < numSpotLights; ++i)
	{
		SpotLight& l = m_SpotLights[i];

		l.PositionWS = {
			static_cast<float>(std::sin(lightAnimTime + offset * i + offset2)) * radius,
			9.0f,
			static_cast<float>(std::cos(lightAnimTime + offset * i + offset2)) * radius,
			1.0f
		};
		XMVECTOR positionWS = XMLoadFloat4(&l.PositionWS);
		XMVECTOR positionVS = XMVector3TransformCoord(positionWS, viewMatrix);
		XMStoreFloat4(&l.PositionVS, positionVS);

		XMVECTOR directionWS = XMVector3Normalize(XMVectorSetW(XMVectorNegate(positionWS), 0));
		XMVECTOR directionVS = XMVector3Normalize(XMVector3TransformNormal(directionWS, viewMatrix));
		XMStoreFloat4(&l.DirectionWS, directionWS);
		XMStoreFloat4(&l.DirectionVS, directionVS);

		l.Color = XMFLOAT4(LightColors[numPointLights + i]);
		l.Intensity = 1.0f;
		l.SpotAngle = XMConvertToRadians(45.0f);
		l.Attenuation = 0.0f;
	}
}

void Scene::Render(CommandList& commandList)
{

}
