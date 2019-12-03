#pragma once

#include "vertex_buffer.h"
#include "index_buffer.h"
#include "gltf.h"
#include "DirectXMath.h"
#include "commandlist.h"
#include "shader_data.h"
#include "material_data.h"
#include "shader_options.h"

class Mesh
{
	friend class Scene;
public:
	Mesh();
	~Mesh();

	void SetBaseTransform(const DirectX::XMMATRIX& base_transform);

	void SetWorldMatrix(const DirectX::XMFLOAT3& translation, const DirectX::XMVECTOR& quaternion, float scale);
	void SetWorldMatrix(const DirectX::XMMATRIX& world_matrix);

	void Render(CommandList& commandlist);

	static const UINT vertex_slot_ = 0;
	static const UINT normal_slot_ = 1;
	static const UINT tangent_slot_ = 2;
	static const UINT texcoord0_slot_ = 3;

protected:
	void Load(const fx::gltf::Document& doc, std::size_t mesh_index, CommandList& command_list);

	void Unload();
private:
	std::string name_;

	struct SubMesh
	{
		SubMesh()
			: SOptions{}
			  , Topology(D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
			  , IndexCount(0)
		{
		}

		void SetMaterial(const MaterialData& material_data);

		VertexBuffer VBuffer;
		IndexBuffer IBuffer;

		UINT IndexCount;
		D3D_PRIMITIVE_TOPOLOGY Topology;

		Material2 Material;
		ShaderOptions SOptions;
	};

	std::vector<SubMesh> sub_meshes_;

	struct ConstantData
	{
		ConstantData()
			: ModelMatrix{DirectX::XMMatrixIdentity()}
			  , ModelViewMatrix{DirectX::XMMatrixIdentity()}
			  , InverseTransposeModelViewMatrix{DirectX::XMMatrixIdentity()}
			  , ModelViewProjectionMatrix{DirectX::XMMatrixIdentity()}
		{
		}

		DirectX::XMMATRIX ModelMatrix;
		DirectX::XMMATRIX ModelViewMatrix;
		DirectX::XMMATRIX InverseTransposeModelViewMatrix;
		DirectX::XMMATRIX ModelViewProjectionMatrix;
	};

	ConstantData data_;

	DirectX::XMMATRIX base_transform_;
};
