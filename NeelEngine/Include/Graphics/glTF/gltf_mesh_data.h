#pragma once

#include <gltf.h>

#include "material_data.h"

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
		return index_buffer_;
	}

	const BufferInfo& VertexBuffer() const noexcept
	{
		return vertex_buffer_;
	}

	const BufferInfo& NormalBuffer() const noexcept
	{
		return normal_buffer_;
	}

	const BufferInfo& TangentBuffer() const noexcept
	{
		return tangent_buffer_;
	}

	const BufferInfo& TexCoord0Buffer() const noexcept
	{
		return tex_coord0_buffer_;
	}

	const MaterialData& Material() const noexcept
	{
		return material_data_;
	}

protected:
	MeshData() = delete;
	MeshData(fx::gltf::Document const& doc, std::size_t mesh_index, std::size_t primitve_index);
	virtual ~MeshData();

private:
	BufferInfo index_buffer_{};
	BufferInfo vertex_buffer_{};
	BufferInfo normal_buffer_{};
	BufferInfo tangent_buffer_{};
	BufferInfo tex_coord0_buffer_{};

	MaterialData material_data_{};

	static BufferInfo GetData(fx::gltf::Document const& doc, fx::gltf::Accessor const& accessor);
	static uint32_t CalculateDataTypeSize(fx::gltf::Accessor const& accessor) noexcept;
};
