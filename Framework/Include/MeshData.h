#pragma once

#include <gltf.h>
#include <d3d12.h>

class MeshData
{
	friend class Mesh;
public:
	struct BufferInfo
	{
		const fx::gltf::Accessor* Accessor;

		uint8_t const* Data;
		uint32_t DataStride;
		uint32_t TotalSize;

		bool HasData() const noexcept
		{
			return Data != nullptr;
		}
	};

	const BufferInfo& IndexBuffer() const noexcept
	{
		return m_indexBuffer;
	}

	const BufferInfo& VertexBuffer() const noexcept
	{
		return m_vertexBuffer;
	}

	const BufferInfo& NormalBuffer() const noexcept
	{
		return m_normalBuffer;
	}

	const BufferInfo& TangentBuffer() const noexcept
	{
		return m_tangentBuffer;
	}

	const BufferInfo& TexCoord0Buffer() const noexcept
	{
		return m_texCoord0Buffer;
	}

protected:
	MeshData() = delete;
	MeshData(fx::gltf::Document const& doc, std::size_t meshIndex, std::size_t primitveIndex);
	virtual ~MeshData();

private:
	BufferInfo m_indexBuffer{};
	BufferInfo m_vertexBuffer{};
	BufferInfo m_normalBuffer{};
	BufferInfo m_tangentBuffer{};
	BufferInfo m_texCoord0Buffer{};

	static BufferInfo GetData(fx::gltf::Document const& doc, fx::gltf::Accessor const& accessor);
	static uint32_t CalculateDataTypeSize(fx::gltf::Accessor const& accessor) noexcept;
};