#include <FrameworkPCH.h>

#include <MeshData.h>

MeshData::MeshData(fx::gltf::Document const& doc, std::size_t meshIndex, std::size_t primitveIndex)
{
	fx::gltf::Mesh const& mesh = doc.meshes[meshIndex];
	fx::gltf::Primitive const& primitive = mesh.primitives[primitveIndex];

	for (auto const& attrib : primitive.attributes)
	{
		if (attrib.first == "POSITION")
		{
			m_vertexBuffer = GetData(doc, doc.accessors[attrib.second]);
		}
		else if (attrib.first == "NORMAL")
		{
			m_normalBuffer = GetData(doc, doc.accessors[attrib.second]);
		}
		else if (attrib.first == "TANGENT")
		{
			m_tangentBuffer = GetData(doc, doc.accessors[attrib.second]);
		}
		else if (attrib.first == "TEXCOORD_0")
		{
			m_texCoord0Buffer = GetData(doc, doc.accessors[attrib.second]);
		}
	}

	m_indexBuffer = GetData(doc, doc.accessors[primitive.indices]);
}

MeshData::~MeshData()
{

}

MeshData::BufferInfo MeshData::GetData(fx::gltf::Document const& doc, fx::gltf::Accessor const& accessor)
{
	const fx::gltf::BufferView& bufferView = doc.bufferViews[accessor.bufferView];
	const fx::gltf::Buffer& buffer		   = doc.buffers[bufferView.buffer];

	const uint32_t dataTypeSize = CalculateDataTypeSize(accessor);
	return BufferInfo{ &accessor, &buffer.data[static_cast<uint64_t>(bufferView.byteOffset) + accessor.byteOffset], dataTypeSize, accessor.count * dataTypeSize };
}

uint32_t MeshData::CalculateDataTypeSize(fx::gltf::Accessor const& accessor) noexcept
{
	uint32_t elementSize = 0;
	switch (accessor.componentType)
	{
	case fx::gltf::Accessor::ComponentType::Byte:
	case fx::gltf::Accessor::ComponentType::UnsignedByte:
		elementSize = 1;
		break;
	case fx::gltf::Accessor::ComponentType::Short:
	case fx::gltf::Accessor::ComponentType::UnsignedShort:
		elementSize = 2;
		break;
	case fx::gltf::Accessor::ComponentType::Float:
	case fx::gltf::Accessor::ComponentType::UnsignedInt:
		elementSize = 4;
		break;
	}

	switch (accessor.type)
	{
	case fx::gltf::Accessor::Type::Mat2:
		return 4 * elementSize;
		break;
	case fx::gltf::Accessor::Type::Mat3:
		return 9 * elementSize;
		break;
	case fx::gltf::Accessor::Type::Mat4:
		return 16 * elementSize;
		break;
	case fx::gltf::Accessor::Type::Scalar:
		return elementSize;
		break;
	case fx::gltf::Accessor::Type::Vec2:
		return 2 * elementSize;
		break;
	case fx::gltf::Accessor::Type::Vec3:
		return 3 * elementSize;
		break;
	case fx::gltf::Accessor::Type::Vec4:
		return 4 * elementSize;
		break;
	}

	return 0;
}
