#pragma once

#include "vertex_buffer.h"
#include "index_buffer.h"
#include "DirectXMath.h"
#include "commandlist.h"
#include "material.h"

#include "render_context.h"

#include "gltf.h"

class Mesh
{
private:
	friend class Scene;

	struct SubMesh
	{
		SubMesh()
			: IndexCount(0)
			, Topology(D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
		{}

		VertexBuffer			VBuffer;
		IndexBuffer				IBuffer;
		UINT					IndexCount;
		D3D_PRIMITIVE_TOPOLOGY  Topology;

		Material				Material;
	};
public:
	Mesh();
	~Mesh();

	void SetBaseTransform(const DirectX::XMMATRIX& base_transform);

	void SetWorldMatrix(const DirectX::XMFLOAT3& translation, const float rotation_y, float scale);
	void SetWorldMatrix(const DirectX::XMMATRIX& world_matrix);

	void Render(RenderContext& render_context);

	static const UINT vertex_slot_		= 0;
	static const UINT normal_slot_		= 1;
	static const UINT tangent_slot_		= 2;
	static const UINT texcoord0_slot_	= 3;

	std::vector<ShaderOptions> RequiredShaderOptions() const;

	std::vector<SubMesh>& GetSubMeshes() { return sub_meshes_; };

	void SetEmissive(DirectX::XMFLOAT3 color);

protected:
	void Load(const fx::gltf::Document& doc, std::size_t mesh_index, CommandList& command_list, std::vector<Material>* scene_materials = nullptr);

	void Unload();
private:
	DirectX::XMMATRIX	base_transform_;
	std::string			name_;
	
	std::vector<SubMesh> sub_meshes_;

	MeshConstantData constant_data_;

	DirectX::XMMATRIX world_matrix_;
};
