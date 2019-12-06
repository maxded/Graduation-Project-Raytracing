#include "neel_engine_pch.h"

#include "gltf_mesh_data.h"

MeshData::MeshData(fx::gltf::Document const& doc, std::size_t mesh_index, std::size_t primitve_index)
{
	const fx::gltf::Mesh& mesh = doc.meshes[mesh_index];
	const fx::gltf::Primitive& primitive = mesh.primitives[primitve_index];

	for (auto const& attrib : primitive.attributes)
	{
		if (attrib.first == "POSITION")
		{
			vertex_buffer_ = GetData(doc, doc.accessors[attrib.second]);
		}
		else if (attrib.first == "NORMAL")
		{
			normal_buffer_ = GetData(doc, doc.accessors[attrib.second]);
		}
		else if (attrib.first == "TANGENT")
		{
			tangent_buffer_ = GetData(doc, doc.accessors[attrib.second]);
		}
		else if (attrib.first == "TEXCOORD_0")
		{
			tex_coord0_buffer_ = GetData(doc, doc.accessors[attrib.second]);
		}
	}

	index_buffer_ = GetData(doc, doc.accessors[primitive.indices]);

	if (primitive.material >= 0)
	{
		material_data_.SetData(doc.materials[primitive.material]);
	}
}

MeshData::~MeshData()
{
}

MeshData::BufferInfo MeshData::GetData(fx::gltf::Document const& doc, fx::gltf::Accessor const& accessor)
{
	const fx::gltf::BufferView& buffer_view = doc.bufferViews[accessor.bufferView];
	const fx::gltf::Buffer& buffer = doc.buffers[buffer_view.buffer];

	const uint32_t data_type_size = CalculateDataTypeSize(accessor);
	return BufferInfo{
		&accessor, &buffer.data[static_cast<uint64_t>(buffer_view.byteOffset) + accessor.byteOffset], data_type_size,
		accessor.count * data_type_size
	};
}

uint32_t MeshData::CalculateDataTypeSize(fx::gltf::Accessor const& accessor) noexcept
{
	uint32_t element_size = 0;
	switch (accessor.componentType)
	{
		case fx::gltf::Accessor::ComponentType::Byte:
		case fx::gltf::Accessor::ComponentType::UnsignedByte:
			element_size = 2;
			break;
		case fx::gltf::Accessor::ComponentType::Short:
		case fx::gltf::Accessor::ComponentType::UnsignedShort:
			element_size = 2;
			break;
		case fx::gltf::Accessor::ComponentType::Float:
		case fx::gltf::Accessor::ComponentType::UnsignedInt:
			element_size = 4;
			break;
		default:
			break;
	}

	switch (accessor.type)
	{
		case fx::gltf::Accessor::Type::Mat2:
			return 4 * element_size;
			break;
		case fx::gltf::Accessor::Type::Mat3:
			return 9 * element_size;
			break;
		case fx::gltf::Accessor::Type::Mat4:
			return 16 * element_size;
			break;
		case fx::gltf::Accessor::Type::Scalar:
			return element_size;
			break;
		case fx::gltf::Accessor::Type::Vec2:
			return 2 * element_size;
			break;
		case fx::gltf::Accessor::Type::Vec3:
			return 3 * element_size;
			break;
		case fx::gltf::Accessor::Type::Vec4:
			return 4 * element_size;
			break;
		default:
			break;
	}

	return 0;
}
