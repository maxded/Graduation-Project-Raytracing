#pragma once

#include "buffer.h"

#include <array>

class VertexBuffer : public Buffer
{
public:
	VertexBuffer(const std::string& name = "Vertex Buffer");
	virtual ~VertexBuffer();

	void CreateViews(size_t num_elements, size_t element_size) override;

	void CreateViews(std::array<size_t, 4> num_elements, std::array<size_t, 4> element_size);

	/**
	 * Get the vertex buffer view for binding to the Input Assembler stage.
	 */
	std::array<D3D12_VERTEX_BUFFER_VIEW, 4> GetVertexBufferViews() const
	{
		return vertex_buffer_views_;
	}

	uint32_t GetNumVertices() const
	{
		return num_vertices_;
	}


	/**
	* Get the SRV for a resource.
	*/
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(
		const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr) const override;

	/**
	* Get the UAV for a (sub)resource.
	*/
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr) const override;
protected:
private:
	uint32_t num_vertices_;
	uint32_t vertex_stride_;

	uint32_t gpu_offset_;

	std::array<D3D12_VERTEX_BUFFER_VIEW, 4> vertex_buffer_views_;
};
