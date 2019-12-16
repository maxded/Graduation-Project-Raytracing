#include "neel_engine_pch.h"

#include "neel_engine.h"
#include "mesh.h"
#include "gltf_mesh_data.h"
#include "camera.h"

Mesh::Mesh()
	: name_("unavailable")
	  , base_transform_{XMMatrixIdentity()}
	  , world_matrix_{ XMMatrixIdentity()}
{
}

Mesh::~Mesh()
{
}

void Mesh::SetWorldMatrix(const DirectX::XMFLOAT3& translation, const float rotation_y, float scale)
{
	const XMMATRIX t = XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&translation));
	const XMMATRIX r = XMMatrixRotationY(rotation_y);
	const XMMATRIX s = XMMatrixScaling(scale, scale, scale);
	
	world_matrix_ = t * r * s;
}

void Mesh::SetWorldMatrix(const DirectX::XMMATRIX& world_matrix)
{
	world_matrix_ = world_matrix;
}

std::vector<ShaderOptions> Mesh::RequiredShaderOptions() const
{
	std::vector<ShaderOptions> required_shader_options{};

	for (const auto& submesh : sub_meshes_)
	{
		required_shader_options.push_back(submesh.ShaderConfigurations);
	}

	return required_shader_options;
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

		std::array<size_t, 4> num_elements {0, 0, 0, 0};
		std::array<size_t, 4 > element_size {0, 0, 0, 0};

		// Copy position data to upload buffer...
		std::memcpy(cpu, v_buffer.Data, v_buffer.TotalSize);
		num_elements[vertex_slot_] = v_buffer.Accessor->count;
		element_size[vertex_slot_] = v_buffer.DataStride;
		offset += v_buffer.TotalSize;

		// Copy normal data to upload buffer...
		std::memcpy(cpu + offset, n_buffer.Data, n_buffer.TotalSize);
		num_elements[normal_slot_] = n_buffer.Accessor->count;
		element_size[normal_slot_] = n_buffer.DataStride;
		offset += n_buffer.TotalSize;

		if (t_buffer.HasData())
		{
			// Copy tangent data to upload buffer...
			std::memcpy(cpu + offset, t_buffer.Data, t_buffer.TotalSize);
			num_elements[tangent_slot_] = t_buffer.Accessor->count;
			element_size[tangent_slot_] = t_buffer.DataStride;
			offset += t_buffer.TotalSize;
		}

		if (c_buffer.HasData())
		{
			// Copy texcoord data to upload buffer...
			std::memcpy(cpu + offset, c_buffer.Data, c_buffer.TotalSize);
			num_elements[texcoord0_slot_] = c_buffer.Accessor->count;
			element_size[texcoord0_slot_] = c_buffer.DataStride;
			offset += c_buffer.TotalSize;
		}

		// Copy index data to upload buffer...
		std::memcpy(cpu + offset, i_buffer.Data, i_buffer.TotalSize);
		submesh.IndexCount = i_buffer.Accessor->count;
		DXGI_FORMAT index_format = (i_buffer.DataStride == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

		command_list.CopyBuffer(submesh.VBuffer, total_buffer_size - i_buffer.TotalSize, cpu);
		command_list.CopyIndexBuffer(submesh.IBuffer, submesh.IndexCount, index_format, cpu + offset);

		// Create views for all individual buffers
		submesh.VBuffer.CreateViews(num_elements, element_size);

		submesh.VBuffer.SetName(name_ + " Vertex Buffer");
		submesh.IBuffer.SetName(name_ + " Index Buffer");

		delete[] p_buffer;

		// Set material properties for this sub mesh
		submesh.SetMaterial(mesh.Material());
		materials_.push_back(submesh.Material);

		if(t_buffer.HasData())
		{
			submesh.ShaderConfigurations |= ShaderOptions::HAS_TANGENTS;
		}
	}
}

void Mesh::Unload()
{
	for(auto& submesh : sub_meshes_)
	{
		submesh.VBuffer.Reset();
		submesh.IBuffer.Reset();
	}
}

void Mesh::Render(RenderContext& render_context)
{
	Camera& camera = Camera::Get();

	constant_data_.ModelMatrix				= base_transform_ * world_matrix_;
	
	XMMATRIX model_view						= constant_data_.ModelMatrix * camera.GetViewMatrix();
	XMMATRIX inverse_transpose_model		= XMMatrixTranspose(XMMatrixInverse(nullptr, constant_data_.ModelMatrix));
	XMMATRIX model_view_projection			= constant_data_.ModelMatrix * camera.GetViewMatrix() * camera.GetProjectionMatrix();

	constant_data_.ModelViewMatrix				= model_view;
	constant_data_.InverseTransposeModelMatrix	= inverse_transpose_model;
	constant_data_.ModelViewProjectionMatrix	= model_view_projection;
	constant_data_.CameraPosition				= camera.GetTranslation();

	render_context.CommandList.SetGraphicsDynamicStructuredBuffer(2, materials_);

	int material_index = 0;
	for (auto& submesh : sub_meshes_)
	{
		const ShaderOptions options = (render_context.OverrideOptions == ShaderOptions::None ? submesh.ShaderConfigurations : render_context.OverrideOptions);
		if (options != render_context.CurrentOptions)
		{
			render_context.CommandList.SetPipelineState(render_context.PipelineStateMap[options]);
			render_context.CurrentOptions = options;
		}

		constant_data_.MaterialIndex = material_index;
		
		render_context.CommandList.SetGraphicsDynamicConstantBuffer(0, constant_data_);
		
		render_context.CommandList.SetPrimitiveTopology(submesh.Topology);
		render_context.CommandList.SetVertexBuffer(0, submesh.VBuffer);

		if (submesh.IndexCount > 0)
		{
			render_context.CommandList.SetIndexBuffer(submesh.IBuffer);
			render_context.CommandList.DrawIndexed(submesh.IndexCount, 1, 0, 0, 0);
		}
		else
		{
			render_context.CommandList.Draw(submesh.VBuffer.GetNumVertices(), 1, 0, 0);
		}

		material_index++;
	}
}

void Mesh::SetBaseTransform(const DirectX::XMMATRIX& base_transform)
{
	base_transform_ = base_transform;
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

		ShaderOptions options = ShaderOptions::None;

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

		ShaderConfigurations = options;
	}
	else
	{
		// Use a default material
		ShaderConfigurations = ShaderOptions::None;
		Material.BaseColorFactor = DirectX::XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		Material.MetallicFactor = 0;
		Material.RoughnessFactor = 0.5f;
	}
}
