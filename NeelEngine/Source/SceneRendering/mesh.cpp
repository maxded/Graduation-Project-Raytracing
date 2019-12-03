#include "neel_engine_pch.h"

#include "neel_engine.h"
#include "mesh.h"
#include "mesh_data.h"
#include "camera.h"

Mesh::Mesh()
	: name_("unavailable")
	  , base_transform_{XMMatrixIdentity()}
{
}

Mesh::~Mesh()
{
}

void Mesh::SetWorldMatrix(const DirectX::XMFLOAT3& translation, const DirectX::XMVECTOR& quaternion, float scale)
{
	const XMMATRIX t = XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&translation));
	const XMMATRIX r = XMMatrixRotationQuaternion(quaternion);
	const XMMATRIX s = XMMatrixScaling(scale, scale, scale);

	data_.ModelMatrix = base_transform_ * t * r * s;
}

void Mesh::SetWorldMatrix(const DirectX::XMMATRIX& world_matrix)
{
	data_.ModelMatrix = base_transform_ * world_matrix;
}

void Mesh::Load(const fx::gltf::Document& doc, std::size_t mesh_index, CommandList& command_list)
{
	auto device = NeelEngine::Get().GetDevice();

	name_ = doc.meshes[mesh_index].name;

	sub_meshes_.resize(doc.meshes[mesh_index].primitives.size());

	for (std::size_t i = 0; i < doc.meshes[mesh_index].primitives.size(); i++)
	{
		MeshData mesh(doc, mesh_index, i);

		const MeshData::BufferInfo& v_buffer = mesh.VertexBuffer();
		const MeshData::BufferInfo& n_buffer = mesh.NormalBuffer();
		const MeshData::BufferInfo& t_buffer = mesh.TangentBuffer();
		const MeshData::BufferInfo& c_buffer = mesh.TexCoord0Buffer();
		const MeshData::BufferInfo& i_buffer = mesh.IndexBuffer();

		if (!v_buffer.HasData() || !n_buffer.HasData() || !i_buffer.HasData())
		{
			throw std::runtime_error("Only meshes with vertex, normal, and index buffers are supported");
		}

		const fx::gltf::Primitive& primitive = doc.meshes[mesh_index].primitives[i];

		SubMesh& submesh = sub_meshes_[i];

		// Get submesh primitive topology for rendering
		switch (primitive.mode)
		{
			case fx::gltf::Primitive::Mode::Points:
				submesh.Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
				break;
			case fx::gltf::Primitive::Mode::Lines:
				submesh.Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
				break;
			case fx::gltf::Primitive::Mode::LineLoop:
				submesh.Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST;
				break;
			case fx::gltf::Primitive::Mode::LineStrip:
				submesh.Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
				break;
			case fx::gltf::Primitive::Mode::Triangles:
				submesh.Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				break;
			case fx::gltf::Primitive::Mode::TriangleStrip:
				submesh.Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
				break;
			case fx::gltf::Primitive::Mode::TriangleFan:
				submesh.Topology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
				break;
		}

		const uint32_t total_buffer_size =
			v_buffer.TotalSize +
			n_buffer.TotalSize +
			t_buffer.TotalSize +
			c_buffer.TotalSize +
			i_buffer.TotalSize;

		void* p_buffer = new char[total_buffer_size];
		uint8_t* cpu = static_cast<uint8_t*>(p_buffer);
		uint32_t offset = 0;

		if (!cpu)
			throw std::bad_alloc();

		std::vector<uint32_t> num_elements;
		std::vector<uint32_t> element_size;

		// Copy position data to upload buffer...
		std::memcpy(cpu, v_buffer.Data, v_buffer.TotalSize);
		num_elements.push_back(v_buffer.Accessor->count);
		element_size.push_back(v_buffer.DataStride);
		offset += v_buffer.TotalSize;

		// Copy normal data to upload buffer...
		std::memcpy(cpu + offset, n_buffer.Data, n_buffer.TotalSize);
		num_elements.push_back(n_buffer.Accessor->count);
		element_size.push_back(n_buffer.DataStride);
		offset += n_buffer.TotalSize;

		if (t_buffer.HasData())
		{
			// Copy tangent data to upload buffer...
			std::memcpy(cpu + offset, t_buffer.Data, t_buffer.TotalSize);
			num_elements.push_back(t_buffer.Accessor->count);
			element_size.push_back(t_buffer.DataStride);
			offset += t_buffer.TotalSize;
		}

		if (c_buffer.HasData())
		{
			// Copy texcoord data to upload buffer...
			std::memcpy(cpu + offset, c_buffer.Data, c_buffer.TotalSize);
			num_elements.push_back(c_buffer.Accessor->count);
			element_size.push_back(c_buffer.DataStride);
			offset += c_buffer.TotalSize;
		}

		// Copy index data to upload buffer...
		std::memcpy(cpu + offset, i_buffer.Data, i_buffer.TotalSize);
		submesh.IndexCount = i_buffer.Accessor->count;
		DXGI_FORMAT index_format = (i_buffer.DataStride == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

		command_list.CopyBuffer(submesh.VBuffer, total_buffer_size, cpu);
		command_list.CopyIndexBuffer(submesh.IBuffer, submesh.IndexCount, index_format, cpu + offset);

		// Create views for all individual buffers
		submesh.VBuffer.CreateViews(num_elements, element_size);
		submesh.IBuffer.CreateViews(i_buffer.Accessor->count, i_buffer.DataStride);

		submesh.VBuffer.SetName(name_ + " Vertex Buffer");
		submesh.IBuffer.SetName(name_ + " Index Buffer");

		delete[] p_buffer;

		// Set material properties for this sub mesh
		submesh.SetMaterial(mesh.Material());
	}
}

void Mesh::Render(CommandList& command_list)
{
	Camera& camera = Camera::Get();

	XMMATRIX model_view = data_.ModelMatrix * camera.GetViewMatrix();
	XMMATRIX inverse_transpose_model_view = XMMatrixTranspose(XMMatrixInverse(nullptr, model_view));
	XMMATRIX model_view_projection = data_.ModelMatrix * camera.GetViewMatrix() * camera.GetProjectionMatrix();

	data_.ModelViewMatrix = model_view;
	data_.InverseTransposeModelViewMatrix = inverse_transpose_model_view;
	data_.ModelViewProjectionMatrix = model_view_projection;

	command_list.SetGraphicsDynamicConstantBuffer(0, data_);

	for (auto& submesh : sub_meshes_)
	{
		command_list.SetPrimitiveTopology(submesh.Topology);
		command_list.SetVertexBuffer(0, submesh.VBuffer);

		if (submesh.IndexCount > 0)
		{
			command_list.SetIndexBuffer(submesh.IBuffer);
			command_list.DrawIndexed(submesh.IndexCount, 1, 0, 0, 0);
		}
		else
		{
			command_list.Draw(submesh.VBuffer.GetNumVertices(), 1, 0, 0);
		}
	}
}

void Mesh::SetBaseTransform(const DirectX::XMMATRIX& base_transform)
{
	base_transform_ = base_transform;
	data_.ModelMatrix = base_transform_;
}

void Mesh::SubMesh::SetMaterial(const MaterialData& material_data)
{
	if (material_data.HasData())
	{
		auto& m = material_data.Data();

		Material.BaseColorIndex = m.pbrMetallicRoughness.baseColorTexture.index;
		Material.BaseColorFactor = XMFLOAT4(m.pbrMetallicRoughness.baseColorFactor.data());

		Material.MetalRoughIndex = m.pbrMetallicRoughness.metallicRoughnessTexture.index;
		Material.MetallicFactor = m.pbrMetallicRoughness.metallicFactor;
		Material.RoughnessFactor = m.pbrMetallicRoughness.roughnessFactor;

		Material.NormalIndex = m.normalTexture.index;
		Material.NormalScale = m.normalTexture.scale;

		Material.AoIndex = m.occlusionTexture.index;
		Material.AoStrength = m.occlusionTexture.strength;

		Material.EmissiveIndex = m.emissiveTexture.index;
		Material.EmissiveFactor = XMFLOAT3(m.emissiveFactor.data());

		ShaderOptions options = ShaderOptions::kNone;

		if (Material.BaseColorIndex >= 0)
			options |= ShaderOptions::HAS_BASECOLORMAP;
		if (Material.NormalIndex >= 0)
			options |= ShaderOptions::HAS_NORMALMAP;
		if (Material.MetalRoughIndex >= 0)
			options |= ShaderOptions::HAS_METALROUGHNESSMAP;
		if (Material.AoIndex >= 0)
		{
			options |= ShaderOptions::HAS_OCCLUSIONMAP;

			if (Material.AoIndex == Material.MetalRoughIndex)
			{
				options |= ShaderOptions::HAS_OCCLUSIONMAP_COMBINED;
			}
		}
		if (Material.EmissiveIndex >= 0)
			options |= ShaderOptions::HAS_EMISSIVEMAP;

		if (options == ShaderOptions::kNone)
		{
			options = ShaderOptions::USE_FACTORS_ONLY;
		}

		SOptions = options;
	}
	else
	{
		// Use a default material
		SOptions = ShaderOptions::USE_FACTORS_ONLY;
		Material.BaseColorFactor = DirectX::XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		Material.MetallicFactor = 0;
		Material.RoughnessFactor = 0.5f;
	}
}
